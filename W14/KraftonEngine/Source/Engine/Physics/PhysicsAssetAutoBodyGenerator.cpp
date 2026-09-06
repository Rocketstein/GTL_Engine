#include "Physics/PhysicsAssetAutoBodyGenerator.h"

#include "Animation/Skeleton/Skeleton.h"
#include "Animation/Skeleton/SkeletonTypes.h"
#include "Core/Logging/Log.h"
#include "Math/Matrix.h"
#include "Math/MathUtils.h"
#include "Math/Transform.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsAssetTypes.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cctype>

namespace
{
    struct FAutoBodyFitResult
    {
        FTransform BodyComponentTransform;
        FVector BoxHalfExtent = FVector(0.4f, 0.4f, 1.2f);
        float CapsuleRadius = 0.4f;
        float CapsuleHalfHeight = 1.2f;
    };

    struct FAutoBodyPointFit
    {
        bool bHasPoints = false;
        FVector Min = FVector::ZeroVector;
        FVector Max = FVector::ZeroVector;
        TArray<FVector> Points;
        TArray<float> PointWeights;

        void AddPoint(const FVector& Point, float Weight = 1.0f)
        {
            Points.push_back(Point);
            PointWeights.push_back((std::max)(Weight, 0.0f));
            if (!bHasPoints)
            {
                Min = Point;
                Max = Point;
                bHasPoints = true;
                return;
            }

            Min.X = (std::min)(Min.X, Point.X);
            Min.Y = (std::min)(Min.Y, Point.Y);
            Min.Z = (std::min)(Min.Z, Point.Z);
            Max.X = (std::max)(Max.X, Point.X);
            Max.Y = (std::max)(Max.Y, Point.Y);
            Max.Z = (std::max)(Max.Z, Point.Z);
        }

        FVector GetExtent() const
        {
            return (Max - Min) * 0.5f;
        }
    };

    struct FMergedAutoBodyData
    {
        TArray<FAutoBodyPointFit> ExtraFits;
        TArray<float> MergedSizes;
        TArray<bool> bMergedIntoParent;
        int32 ForcedRootBoneIndex = -1;
    };

    constexpr float AutoBodyMinExtent = 0.05f;
    constexpr float AutoBodyRadiusPadding = 1.10f;
    constexpr float AutoBodyFallbackRadiusRatio = 0.18f;
    constexpr float AutoBodyMinWeldSize = 1.0e-6f;
    constexpr float AutoBodyPcaBoneAxisAlignmentThreshold = 0.65f;
    constexpr float AutoBodyPcaBoneAxisSoftAlignmentThreshold = 0.85f;
    constexpr float AutoBodyPcaBoneAxisRadiusTolerance = 1.25f;
    constexpr float AutoBodyPcaRadiusRegressionThreshold = 1.50f;
    constexpr float AutoBodyBoundsTrimWeightFraction = 0.05f;
    constexpr float AutoBodyTrimmedLengthMinRatio = 0.60f;

    bool HasBoneName(const FName& BoneName)
    {
        return BoneName.IsValid() && BoneName != FName::None;
    }

    FString ToLowerBoneName(FString BoneName)
    {
        std::transform(
            BoneName.begin(),
            BoneName.end(),
            BoneName.begin(),
            [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
        return BoneName;
    }

    bool ContainsToken(const FString& Value, const char* Token)
    {
        return Value.find(Token) != FString::npos;
    }

    bool IsLikelyHelperBoneName(const FString& BoneName)
    {
        if (BoneName.empty())
        {
            return true;
        }

        const FString LowerName = ToLowerBoneName(BoneName);
        return
            LowerName == "root" ||
            LowerName == "end" ||
            ContainsToken(LowerName, "ik_") ||
            ContainsToken(LowerName, "_ik") ||
            ContainsToken(LowerName, "ik-") ||
            ContainsToken(LowerName, "-ik") ||
            ContainsToken(LowerName, "socket") ||
            ContainsToken(LowerName, "weapon") ||
            ContainsToken(LowerName, "attach") ||
            ContainsToken(LowerName, "ctrl") ||
            ContainsToken(LowerName, "control") ||
            ContainsToken(LowerName, "helper") ||
            ContainsToken(LowerName, "virtual") ||
            ContainsToken(LowerName, "twist") ||
            ContainsToken(LowerName, "roll") ||
            ContainsToken(LowerName, "nub") ||
            ContainsToken(LowerName, "dummy") ||
            ContainsToken(LowerName, "marker");
    }

    FVector GetMatrixTranslation(const FMatrix& Matrix)
    {
        return Matrix.GetLocation();
    }

    const FMatrix& GetGenerationBoneGlobalPose(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        int32 MeshBoneIndex);

    FTransform MakeTransformNoScale(const FMatrix& Matrix)
    {
        FTransform Transform;
        Transform.Location = Matrix.GetLocation();
        Transform.Rotation = Matrix.ToQuatWithoutScale().GetNormalized();
        Transform.Scale = FVector::OneVector;
        return Transform;
    }

    FTransform ComposeComponentTransforms(const FTransform& Parent, const FTransform& Local)
    {
        FTransform Result = Local;
        Result.Location = Parent.Location + Parent.Rotation.RotateVector(Local.Location);
        Result.Rotation = (Parent.Rotation * Local.Rotation).GetNormalized();
        Result.Scale = FVector::OneVector;
        return Result;
    }

    FTransform MakeLocalTransformFromComponent(const FTransform& Parent, const FTransform& Component)
    {
        const FQuat ParentInv = Parent.Rotation.GetNormalized().Inverse();
        FTransform Local;
        Local.Location = ParentInv.RotateVector(Component.Location - Parent.Location);
        Local.Rotation = (ParentInv * Component.Rotation.GetNormalized()).GetNormalized();
        Local.Scale = FVector::OneVector;
        return Local;
    }

    FVector SafeNormal(const FVector& Value, const FVector& Fallback)
    {
        if (Value.IsNearlyZero(1.0e-6f))
        {
            return Fallback;
        }
        FVector Result = Value;
        Result.Normalize();
        return Result;
    }

    float GetVectorComponent(const FVector& Value, int32 Index)
    {
        if (Index == 0) return Value.X;
        if (Index == 1) return Value.Y;
        return Value.Z;
    }

    float GetMaxComponent(const FVector& Value)
    {
        return (std::max)(Value.X, (std::max)(Value.Y, Value.Z));
    }

    FVector ClampAutoBodyHalfExtent(const FVector& HalfExtent)
    {
        return FVector(
            (std::max)(HalfExtent.X, AutoBodyMinExtent),
            (std::max)(HalfExtent.Y, AutoBodyMinExtent),
            (std::max)(HalfExtent.Z, AutoBodyMinExtent));
    }

    void BuildBasisFromZAxis(const FVector& InAxisZ, FVector& OutAxisX, FVector& OutAxisY, FVector& OutAxisZ)
    {
        OutAxisZ = SafeNormal(InAxisZ, FVector::ZAxisVector);
        const FVector UpCandidate = std::fabs(OutAxisZ.Z) < 0.90f ? FVector::ZAxisVector : FVector::YAxisVector;
        OutAxisX = SafeNormal(UpCandidate.Cross(OutAxisZ), FVector::XAxisVector);
        OutAxisY = SafeNormal(OutAxisZ.Cross(OutAxisX), FVector::YAxisVector);
    }

    FQuat MakeQuatFromLocalAxes(const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ)
    {
        FMatrix Matrix = FMatrix::Identity;
        Matrix.SetAxes(AxisX, AxisY, AxisZ);
        return Matrix.ToQuatWithoutScale().GetNormalized();
    }

    int32 FindMeshBoneIndexByName(const FSkeletalMesh* MeshAsset, const FString& BoneName)
    {
        if (!MeshAsset || BoneName.empty())
        {
            return -1;
        }

        for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MeshAsset->Bones.size()); ++BoneIndex)
        {
            if (MeshAsset->Bones[BoneIndex].Name == BoneName)
            {
                return BoneIndex;
            }
        }
        return -1;
    }

    bool IsDirectChildMeshBone(const FSkeletalMesh* MeshAsset, int32 ChildBoneIndex, int32 ParentBoneIndex)
    {
        return MeshAsset &&
            ChildBoneIndex >= 0 &&
            ParentBoneIndex >= 0 &&
            ChildBoneIndex < static_cast<int32>(MeshAsset->Bones.size()) &&
            ParentBoneIndex < static_cast<int32>(MeshAsset->Bones.size()) &&
            MeshAsset->Bones[ChildBoneIndex].ParentIndex == ParentBoneIndex;
    }

    bool IsDescendantMeshBone(const FSkeletalMesh* MeshAsset, int32 BoneIndex, int32 AncestorBoneIndex)
    {
        if (!MeshAsset || BoneIndex < 0 || AncestorBoneIndex < 0 ||
            BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()) ||
            AncestorBoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
        {
            return false;
        }

        int32 Cursor = BoneIndex;
        while (Cursor >= 0 && Cursor < static_cast<int32>(MeshAsset->Bones.size()))
        {
            if (Cursor == AncestorBoneIndex)
            {
                return true;
            }
            Cursor = MeshAsset->Bones[Cursor].ParentIndex;
        }
        return false;
    }

    int32 CountWeightedVerticesForMeshBone(const FSkeletalMesh* MeshAsset, int32 MeshBoneIndex, float MinWeight = 1.0e-4f)
    {
        if (!MeshAsset || MeshBoneIndex < 0 || MeshBoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
        {
            return 0;
        }

        int32 Count = 0;
        for (const FVertexPNCTBW& Vertex : MeshAsset->Vertices)
        {
            for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
            {
                if (Vertex.BoneIndices[InfluenceIndex] == MeshBoneIndex && Vertex.BoneWeights[InfluenceIndex] > MinWeight)
                {
                    ++Count;
                    break;
                }
            }
        }
        return Count;
    }

    int32 CountWeightedVerticesInMeshSubtree(const FSkeletalMesh* MeshAsset, int32 MeshBoneIndex, float MinWeight = 1.0e-4f)
    {
        if (!MeshAsset || MeshBoneIndex < 0 || MeshBoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
        {
            return 0;
        }

        int32 Count = 0;
        for (int32 CandidateBoneIndex = 0; CandidateBoneIndex < static_cast<int32>(MeshAsset->Bones.size()); ++CandidateBoneIndex)
        {
            if (IsDescendantMeshBone(MeshAsset, CandidateBoneIndex, MeshBoneIndex))
            {
                Count += CountWeightedVerticesForMeshBone(MeshAsset, CandidateBoneIndex, MinWeight);
            }
        }
        return Count;
    }

    bool IsSpineLikeBoneName(const FString& LowerName)
    {
        return ContainsToken(LowerName, "spine") || ContainsToken(LowerName, "chest") || ContainsToken(LowerName, "torso");
    }

    bool IsPelvisLikeBoneName(const FString& LowerName)
    {
        return ContainsToken(LowerName, "pelvis") || ContainsToken(LowerName, "hip");
    }

    bool IsHandLikeBoneName(const FString& LowerName)
    {
        return ContainsToken(LowerName, "hand") || ContainsToken(LowerName, "palm");
    }

    bool IsAxialChildPreferred(const FString& ParentLowerName, const FString& ChildLowerName)
    {
        if (IsSpineLikeBoneName(ParentLowerName))
        {
            return ContainsToken(ChildLowerName, "spine") || ContainsToken(ChildLowerName, "neck") || ContainsToken(ChildLowerName, "head");
        }
        if (IsPelvisLikeBoneName(ParentLowerName))
        {
            return ContainsToken(ChildLowerName, "spine") || ContainsToken(ChildLowerName, "abdomen") || ContainsToken(ChildLowerName, "chest");
        }
        if (ContainsToken(ParentLowerName, "neck"))
        {
            return ContainsToken(ChildLowerName, "head");
        }
        if (ContainsToken(ParentLowerName, "upperarm") || ContainsToken(ParentLowerName, "upper_arm"))
        {
            return ContainsToken(ChildLowerName, "lowerarm") || ContainsToken(ChildLowerName, "lower_arm") || ContainsToken(ChildLowerName, "forearm");
        }
        if (ContainsToken(ParentLowerName, "lowerarm") || ContainsToken(ParentLowerName, "forearm"))
        {
            return ContainsToken(ChildLowerName, "hand");
        }
        if (ContainsToken(ParentLowerName, "thigh") || ContainsToken(ParentLowerName, "upleg") || ContainsToken(ParentLowerName, "upperleg"))
        {
            return ContainsToken(ChildLowerName, "calf") || ContainsToken(ChildLowerName, "shin") || ContainsToken(ChildLowerName, "lowerleg");
        }
        if (ContainsToken(ParentLowerName, "calf") || ContainsToken(ParentLowerName, "shin") || ContainsToken(ParentLowerName, "lowerleg"))
        {
            return ContainsToken(ChildLowerName, "foot");
        }
        return false;
    }

    bool IsAxialChildBadMatch(const FString& ParentLowerName, const FString& ChildLowerName)
    {
        if (IsSpineLikeBoneName(ParentLowerName))
        {
            return ContainsToken(ChildLowerName, "clavicle") || ContainsToken(ChildLowerName, "shoulder") || ContainsToken(ChildLowerName, "arm");
        }
        if (IsPelvisLikeBoneName(ParentLowerName))
        {
            return ContainsToken(ChildLowerName, "thigh") || ContainsToken(ChildLowerName, "leg");
        }
        return false;
    }

    TArray<int32> CollectDirectChildMeshBoneIndices(const FSkeletalMesh* MeshAsset, int32 MeshBoneIndex)
    {
        TArray<int32> Children;
        if (!MeshAsset || MeshBoneIndex < 0 || MeshBoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
        {
            return Children;
        }

        for (int32 BoneIndex = MeshBoneIndex + 1; BoneIndex < static_cast<int32>(MeshAsset->Bones.size()); ++BoneIndex)
        {
            if (IsDirectChildMeshBone(MeshAsset, BoneIndex, MeshBoneIndex))
            {
                Children.push_back(BoneIndex);
            }
        }
        return Children;
    }

    int32 FindBestAxisChildMeshBoneIndex(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        int32 MeshBoneIndex)
    {
        const TArray<int32> Children = CollectDirectChildMeshBoneIndices(MeshAsset, MeshBoneIndex);
        if (Children.empty())
        {
            return -1;
        }

        const FString ParentLowerName = ToLowerBoneName(MeshAsset->Bones[MeshBoneIndex].Name);
        const FVector ParentLocation = GetMatrixTranslation(GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex));
        const bool bHasAnyNonHelperChild = std::any_of(
            Children.begin(),
            Children.end(),
            [&](int32 ChildIndex)
            {
                return !IsLikelyHelperBoneName(MeshAsset->Bones[ChildIndex].Name);
            });

        int32 BestChildIndex = -1;
        float BestScore = -FLT_MAX;
        for (int32 ChildIndex : Children)
        {
            const FString ChildLowerName = ToLowerBoneName(MeshAsset->Bones[ChildIndex].Name);
            const bool bHelperChild = IsLikelyHelperBoneName(ChildLowerName);
            if (bHasAnyNonHelperChild && bHelperChild)
            {
                continue;
            }

            const FVector ChildLocation = GetMatrixTranslation(GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, ChildIndex));
            const float SegmentLength = FVector::Distance(ParentLocation, ChildLocation);
            if (SegmentLength <= 1.0e-6f)
            {
                continue;
            }

            const int32 DirectWeightCount = CountWeightedVerticesForMeshBone(MeshAsset, ChildIndex);
            const int32 SubtreeWeightCount = CountWeightedVerticesInMeshSubtree(MeshAsset, ChildIndex);
            float Score = SegmentLength;
            if (SubtreeWeightCount > 0)
            {
                Score += 1000.0f + static_cast<float>((std::min)(SubtreeWeightCount, 1000));
            }
            if (DirectWeightCount > 0)
            {
                Score += 500.0f;
            }
            if (!bHelperChild)
            {
                Score += 100.0f;
            }
            if (IsAxialChildPreferred(ParentLowerName, ChildLowerName))
            {
                Score += 100000.0f;
            }
            if (IsAxialChildBadMatch(ParentLowerName, ChildLowerName))
            {
                Score -= 100000.0f;
            }

            if (Score > BestScore)
            {
                BestScore = Score;
                BestChildIndex = ChildIndex;
            }
        }

        return BestChildIndex;
    }

    bool TryBuildAveragedChildAxisSegment(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        int32 MeshBoneIndex,
        const FVector& Start,
        FVector& OutEnd)
    {
        if (!MeshAsset || MeshBoneIndex < 0 || MeshBoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
        {
            return false;
        }

        const FString ParentLowerName = ToLowerBoneName(MeshAsset->Bones[MeshBoneIndex].Name);
        if (!IsHandLikeBoneName(ParentLowerName))
        {
            return false;
        }

        const TArray<int32> Children = CollectDirectChildMeshBoneIndices(MeshAsset, MeshBoneIndex);
        if (Children.size() < 2)
        {
            return false;
        }

        FVector DirectionSum = FVector::ZeroVector;
        float LengthSum = 0.0f;
        float WeightSum = 0.0f;
        for (int32 ChildIndex : Children)
        {
            const FString ChildLowerName = ToLowerBoneName(MeshAsset->Bones[ChildIndex].Name);
            if (IsLikelyHelperBoneName(ChildLowerName))
            {
                continue;
            }

            const FVector ChildLocation = GetMatrixTranslation(GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, ChildIndex));
            FVector Direction = ChildLocation - Start;
            const float Length = Direction.Length();
            if (Length <= 1.0e-6f)
            {
                continue;
            }
            Direction /= Length;

            const int32 SubtreeWeightCount = CountWeightedVerticesInMeshSubtree(MeshAsset, ChildIndex);
            const float Weight = static_cast<float>((std::max)(SubtreeWeightCount, 1));
            DirectionSum += Direction * Weight;
            LengthSum += Length * Weight;
            WeightSum += Weight;
        }

        if (WeightSum <= 0.0f || DirectionSum.IsNearlyZero(1.0e-6f))
        {
            return false;
        }

        DirectionSum.Normalize();
        const float AverageLength = (std::max)(LengthSum / WeightSum, AutoBodyMinExtent * 2.0f);
        OutEnd = Start + DirectionSum * AverageLength;
        return true;
    }

    const FMatrix& GetGenerationBoneGlobalPose(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        int32 MeshBoneIndex)
    {
        if (OverrideBoneGlobalMatrices &&
            MeshBoneIndex >= 0 &&
            MeshBoneIndex < static_cast<int32>(OverrideBoneGlobalMatrices->size()))
        {
            return (*OverrideBoneGlobalMatrices)[MeshBoneIndex];
        }

        return MeshAsset->Bones[MeshBoneIndex].GetReferenceGlobalPose();
    }

    bool GetBoneAxisSegment(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        int32 MeshBoneIndex,
        FVector& OutStart,
        FVector& OutEnd)
    {
        if (!MeshAsset || MeshBoneIndex < 0 || MeshBoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
        {
            return false;
        }

        OutStart = GetMatrixTranslation(GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex));
        if (TryBuildAveragedChildAxisSegment(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex, OutStart, OutEnd))
        {
            return true;
        }

        const int32 ChildIndex = FindBestAxisChildMeshBoneIndex(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex);
        if (ChildIndex >= 0)
        {
            OutEnd = GetMatrixTranslation(GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, ChildIndex));
            if (!FVector(OutEnd - OutStart).IsNearlyZero(1.0e-6f))
            {
                return true;
            }
        }

        const int32 ParentIndex = MeshAsset->Bones[MeshBoneIndex].ParentIndex;
        if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(MeshAsset->Bones.size()))
        {
            const FVector ParentLocation = GetMatrixTranslation(GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, ParentIndex));
            FVector Direction = OutStart - ParentLocation;
            if (!Direction.IsNearlyZero(1.0e-6f))
            {
                Direction.Normalize();
                const float Length = (std::max)(FVector::Distance(OutStart, ParentLocation) * 0.35f, AutoBodyMinExtent * 2.0f);
                OutEnd = OutStart + Direction * Length;
                OutStart -= Direction * Length;
                return true;
            }
        }

        OutEnd = OutStart + FVector::ZAxisVector * (AutoBodyMinExtent * 4.0f);
        return true;
    }

    FVector TransformVertexToGenerationPose(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        const FVertexPNCTBW& Vertex)
    {
        if (!MeshAsset)
        {
            return Vertex.Position;
        }

        FVector PosedPosition = FVector::ZeroVector;
        float AccumulatedWeight = 0.0f;

        // Use the same bind-to-pose skinning convention as the renderer.
        // This keeps auto body fitting in the visible pose space instead of raw FBX
        // mesh/bind space, which can be 90 degrees off for some imported assets.
        for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
        {
            const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
            const float Weight = Vertex.BoneWeights[InfluenceIndex];
            if (Weight <= 0.0f || BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
            {
                continue;
            }

            const FMatrix PoseSkinMatrix =
                MeshAsset->Bones[BoneIndex].GetInverseBindPose() *
                GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, BoneIndex);
            PosedPosition += PoseSkinMatrix.TransformPositionWithW(Vertex.Position) * Weight;
            AccumulatedWeight += Weight;
        }

        if (AccumulatedWeight <= 1.0e-6f)
        {
            return Vertex.Position;
        }

        return PosedPosition;
    }

    void CollectInfluencedBoneVertices(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        int32 MeshBoneIndex,
        float MinInfluenceWeight,
        TArray<FVector>& OutPoints,
        TArray<float>& OutWeights)
    {
        OutPoints.clear();
        OutWeights.clear();
        if (!MeshAsset || MeshBoneIndex < 0)
        {
            return;
        }

        const float ClampedMinWeight = (std::min)((std::max)(MinInfluenceWeight, 0.0f), 1.0f);
        for (const FVertexPNCTBW& Vertex : MeshAsset->Vertices)
        {
            for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
            {
                const int32 CandidateBoneIndex = Vertex.BoneIndices[InfluenceIndex];
                const float CandidateWeight = Vertex.BoneWeights[InfluenceIndex];
                if (CandidateBoneIndex == MeshBoneIndex && CandidateWeight >= ClampedMinWeight)
                {
                    OutPoints.push_back(TransformVertexToGenerationPose(MeshAsset, OverrideBoneGlobalMatrices, Vertex));
                    OutWeights.push_back(CandidateWeight);
                    break;
                }
            }
        }
    }


    float GetPointFitSize(const FAutoBodyPointFit& Fit)
    {
        return Fit.bHasPoints ? Fit.GetExtent().Length() : 0.0f;
    }

    bool IsValidRefBoneIndex(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex)
    {
        return BoneIndex >= 0 && BoneIndex < RefSkeleton.GetNumBones();
    }

    void AppendPointFit(FAutoBodyPointFit& TargetFit, const FAutoBodyPointFit& SourceFit)
    {
        if (!SourceFit.bHasPoints)
        {
            return;
        }

        for (int32 PointIndex = 0; PointIndex < static_cast<int32>(SourceFit.Points.size()); ++PointIndex)
        {
            const float Weight = PointIndex < static_cast<int32>(SourceFit.PointWeights.size())
                ? SourceFit.PointWeights[PointIndex]
                : 1.0f;
            TargetFit.AddPoint(SourceFit.Points[PointIndex], Weight);
        }
    }

    FAutoBodyPointFit MakeCombinedPointFit(const FAutoBodyPointFit& OwnFit, const FAutoBodyPointFit& ExtraFit)
    {
        FAutoBodyPointFit CombinedFit;
        AppendPointFit(CombinedFit, OwnFit);
        AppendPointFit(CombinedFit, ExtraFit);
        return CombinedFit;
    }

    TArray<FAutoBodyPointFit> BuildBonePointFits(
        const FReferenceSkeleton& RefSkeleton,
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        const FPhysicsAssetAutoBodyGeneratorOptions& Options)
    {
        TArray<FAutoBodyPointFit> Fits;
        Fits.resize(RefSkeleton.GetNumBones());
        if (!MeshAsset)
        {
            return Fits;
        }

        for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNumBones(); ++BoneIndex)
        {
            const int32 MeshBoneIndex = FindMeshBoneIndexByName(MeshAsset, RefSkeleton.Bones[BoneIndex].Name);
            if (MeshBoneIndex < 0 || MeshBoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
            {
                continue;
            }

            TArray<FVector> Points;
            TArray<float> PointWeights;
            CollectInfluencedBoneVertices(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex, Options.MinInfluenceWeight, Points, PointWeights);
            for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
            {
                const float Weight = PointIndex < static_cast<int32>(PointWeights.size())
                    ? PointWeights[PointIndex]
                    : 1.0f;
                Fits[BoneIndex].AddPoint(Points[PointIndex], Weight);
            }
        }

        return Fits;
    }

    FMergedAutoBodyData BuildMergedAutoBodyData(
        const FReferenceSkeleton& RefSkeleton,
        const TArray<FAutoBodyPointFit>& BoneFits,
        const FPhysicsAssetAutoBodyGeneratorOptions& Options)
    {
        FMergedAutoBodyData Data;
        const int32 BoneCount = RefSkeleton.GetNumBones();
        Data.ExtraFits.resize(BoneCount);
        Data.MergedSizes.resize(BoneCount, 0.0f);
        Data.bMergedIntoParent.resize(BoneCount, false);

        if (!Options.bMergeSmallBones)
        {
            return Data;
        }

        const float MinBoneSize = (std::max)(Options.MinBoneSize, 0.0f);
        const float MinWeldSize = (std::max)(Options.MinWeldSize, AutoBodyMinWeldSize);

        for (int32 BoneIndex = BoneCount - 1; BoneIndex >= 0; --BoneIndex)
        {
            if (!IsValidRefBoneIndex(RefSkeleton, BoneIndex))
            {
                continue;
            }

            const FReferenceBone& RefBone = RefSkeleton.Bones[BoneIndex];
            const float MyMergedSize = Data.MergedSizes[BoneIndex] +
                (BoneIndex < static_cast<int32>(BoneFits.size()) ? GetPointFitSize(BoneFits[BoneIndex]) : 0.0f);
            Data.MergedSizes[BoneIndex] = MyMergedSize;

            const bool bHelperBone = Options.bSkipHelperBones && IsLikelyHelperBoneName(RefBone.Name);
            const bool bSmallEnoughToMerge = MyMergedSize < MinBoneSize && MyMergedSize >= MinWeldSize;
            if (!bHelperBone && !bSmallEnoughToMerge)
            {
                continue;
            }

            const int32 ParentIndex = RefBone.ParentIndex;
            if (!IsValidRefBoneIndex(RefSkeleton, ParentIndex))
            {
                continue;
            }

            Data.MergedSizes[ParentIndex] += MyMergedSize;
            if (BoneIndex < static_cast<int32>(BoneFits.size()))
            {
                AppendPointFit(Data.ExtraFits[ParentIndex], BoneFits[BoneIndex]);
            }
            AppendPointFit(Data.ExtraFits[ParentIndex], Data.ExtraFits[BoneIndex]);
            Data.bMergedIntoParent[BoneIndex] = true;
        }

        int32 FirstParentBoneIndex = -1;
        for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            if (Data.MergedSizes[BoneIndex] <= MinBoneSize)
            {
                continue;
            }

            const int32 ParentBoneIndex = RefSkeleton.Bones[BoneIndex].ParentIndex;
            if (ParentBoneIndex == -1)
            {
                break;
            }

            if (FirstParentBoneIndex == -1)
            {
                FirstParentBoneIndex = ParentBoneIndex;
                continue;
            }

            if (ParentBoneIndex == FirstParentBoneIndex)
            {
                Data.ForcedRootBoneIndex = ParentBoneIndex;
                break;
            }
        }

        return Data;
    }

    bool ShouldGenerateBodyForBone(
        const FReferenceSkeleton& RefSkeleton,
        const FMergedAutoBodyData& MergedData,
        const FPhysicsAssetAutoBodyGeneratorOptions& Options,
        int32 BoneIndex)
    {
        if (!IsValidRefBoneIndex(RefSkeleton, BoneIndex))
        {
            return false;
        }

        if (Options.bSkipHelperBones &&
            IsLikelyHelperBoneName(RefSkeleton.Bones[BoneIndex].Name) &&
            BoneIndex != MergedData.ForcedRootBoneIndex)
        {
            return false;
        }

        if (!Options.bMergeSmallBones)
        {
            return true;
        }

        if (BoneIndex == MergedData.ForcedRootBoneIndex)
        {
            return true;
        }

        if (BoneIndex < static_cast<int32>(MergedData.bMergedIntoParent.size()) && MergedData.bMergedIntoParent[BoneIndex])
        {
            return false;
        }

        const float MinBoneSize = (std::max)(Options.MinBoneSize, 0.0f);
        if (BoneIndex < static_cast<int32>(MergedData.MergedSizes.size()) && MergedData.MergedSizes[BoneIndex] < MinBoneSize)
        {
            return false;
        }

        return true;
    }

    float GetPointPCAWeight(const TArray<float>& PointWeights, int32 PointIndex)
    {
        if (PointIndex < 0 || PointIndex >= static_cast<int32>(PointWeights.size()))
        {
            return 1.0f;
        }
        return (std::max)(PointWeights[PointIndex], 0.0f);
    }

    struct FProjectedFitBounds
    {
        FVector Min = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    };

    struct FWeightedProjectionSample
    {
        float Value = 0.0f;
        float Weight = 0.0f;
    };

    void AccumulateProjectedBounds(FProjectedFitBounds& Bounds, const FVector& Projected)
    {
        Bounds.Min.X = (std::min)(Bounds.Min.X, Projected.X);
        Bounds.Min.Y = (std::min)(Bounds.Min.Y, Projected.Y);
        Bounds.Min.Z = (std::min)(Bounds.Min.Z, Projected.Z);
        Bounds.Max.X = (std::max)(Bounds.Max.X, Projected.X);
        Bounds.Max.Y = (std::max)(Bounds.Max.Y, Projected.Y);
        Bounds.Max.Z = (std::max)(Bounds.Max.Z, Projected.Z);
    }

    bool ComputeWeightedTrimmedRange(TArray<FWeightedProjectionSample> Samples, float& OutMin, float& OutMax)
    {
        Samples.erase(
            std::remove_if(
                Samples.begin(),
                Samples.end(),
                [](const FWeightedProjectionSample& Sample)
                {
                    return Sample.Weight <= 0.0f;
                }),
            Samples.end());

        if (Samples.size() < 4)
        {
            return false;
        }

        std::sort(
            Samples.begin(),
            Samples.end(),
            [](const FWeightedProjectionSample& A, const FWeightedProjectionSample& B)
            {
                return A.Value < B.Value;
            });

        double TotalWeight = 0.0;
        for (const FWeightedProjectionSample& Sample : Samples)
        {
            TotalWeight += static_cast<double>(Sample.Weight);
        }
        if (TotalWeight <= 1.0e-8)
        {
            return false;
        }

        const double LowerTarget = TotalWeight * AutoBodyBoundsTrimWeightFraction;
        const double UpperTarget = TotalWeight * (1.0 - AutoBodyBoundsTrimWeightFraction);
        double AccumulatedWeight = 0.0;
        bool bFoundLower = false;
        OutMin = Samples.front().Value;
        OutMax = Samples.back().Value;

        for (const FWeightedProjectionSample& Sample : Samples)
        {
            AccumulatedWeight += static_cast<double>(Sample.Weight);
            if (!bFoundLower && AccumulatedWeight >= LowerTarget)
            {
                OutMin = Sample.Value;
                bFoundLower = true;
            }
            if (AccumulatedWeight >= UpperTarget)
            {
                OutMax = Sample.Value;
                break;
            }
        }

        return OutMax >= OutMin;
    }

    float SelectTrimmedRangeMin(float FullMin, float FullMax, float TrimmedMin, float TrimmedMax, bool bAllowShorterRange)
    {
        const float FullSpan = FullMax - FullMin;
        const float TrimmedSpan = TrimmedMax - TrimmedMin;
        if (TrimmedSpan <= 1.0e-6f)
        {
            return FullMin;
        }

        if (!bAllowShorterRange && FullSpan > 1.0e-6f && TrimmedSpan < FullSpan * AutoBodyTrimmedLengthMinRatio)
        {
            return FullMin;
        }

        return TrimmedMin;
    }

    float SelectTrimmedRangeMax(float FullMin, float FullMax, float TrimmedMin, float TrimmedMax, bool bAllowShorterRange)
    {
        const float FullSpan = FullMax - FullMin;
        const float TrimmedSpan = TrimmedMax - TrimmedMin;
        if (TrimmedSpan <= 1.0e-6f)
        {
            return FullMax;
        }

        if (!bAllowShorterRange && FullSpan > 1.0e-6f && TrimmedSpan < FullSpan * AutoBodyTrimmedLengthMinRatio)
        {
            return FullMax;
        }

        return TrimmedMax;
    }

    bool FitCapsuleToPointsOnBasis(
        const TArray<FVector>& Points,
        const TArray<float>& PointWeights,
        const FVector& Origin,
        const FVector& AxisX,
        const FVector& AxisY,
        const FVector& AxisZ,
        FAutoBodyFitResult& OutFit)
    {
        if (Points.empty())
        {
            return false;
        }

        FProjectedFitBounds FullBounds;
        TArray<FWeightedProjectionSample> WeightedX;
        TArray<FWeightedProjectionSample> WeightedY;
        TArray<FWeightedProjectionSample> WeightedZ;
        WeightedX.reserve(Points.size());
        WeightedY.reserve(Points.size());
        WeightedZ.reserve(Points.size());

        for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
        {
            const FVector& Point = Points[PointIndex];
            const FVector Delta = Point - Origin;
            const FVector Projected(Delta.Dot(AxisX), Delta.Dot(AxisY), Delta.Dot(AxisZ));
            AccumulateProjectedBounds(FullBounds, Projected);

            const float Weight = GetPointPCAWeight(PointWeights, PointIndex);
            WeightedX.push_back({ Projected.X, Weight });
            WeightedY.push_back({ Projected.Y, Weight });
            WeightedZ.push_back({ Projected.Z, Weight });
        }

        FProjectedFitBounds FitBounds = FullBounds;
        float TrimmedMin = 0.0f;
        float TrimmedMax = 0.0f;
        if (ComputeWeightedTrimmedRange(WeightedX, TrimmedMin, TrimmedMax))
        {
            FitBounds.Min.X = SelectTrimmedRangeMin(FullBounds.Min.X, FullBounds.Max.X, TrimmedMin, TrimmedMax, true);
            FitBounds.Max.X = SelectTrimmedRangeMax(FullBounds.Min.X, FullBounds.Max.X, TrimmedMin, TrimmedMax, true);
        }
        if (ComputeWeightedTrimmedRange(WeightedY, TrimmedMin, TrimmedMax))
        {
            FitBounds.Min.Y = SelectTrimmedRangeMin(FullBounds.Min.Y, FullBounds.Max.Y, TrimmedMin, TrimmedMax, true);
            FitBounds.Max.Y = SelectTrimmedRangeMax(FullBounds.Min.Y, FullBounds.Max.Y, TrimmedMin, TrimmedMax, true);
        }
        if (ComputeWeightedTrimmedRange(WeightedZ, TrimmedMin, TrimmedMax))
        {
            FitBounds.Min.Z = SelectTrimmedRangeMin(FullBounds.Min.Z, FullBounds.Max.Z, TrimmedMin, TrimmedMax, false);
            FitBounds.Max.Z = SelectTrimmedRangeMax(FullBounds.Min.Z, FullBounds.Max.Z, TrimmedMin, TrimmedMax, false);
        }

        const FVector CenterLocal((FitBounds.Min.X + FitBounds.Max.X) * 0.5f, (FitBounds.Min.Y + FitBounds.Max.Y) * 0.5f, (FitBounds.Min.Z + FitBounds.Max.Z) * 0.5f);
        const FVector Center = Origin + AxisX * CenterLocal.X + AxisY * CenterLocal.Y + AxisZ * CenterLocal.Z;
        const float HalfLength = (std::max)((FitBounds.Max.Z - FitBounds.Min.Z) * 0.5f, AutoBodyMinExtent);
        const float RadiusX = (std::max)((FitBounds.Max.X - FitBounds.Min.X) * 0.5f, AutoBodyMinExtent);
        const float RadiusY = (std::max)((FitBounds.Max.Y - FitBounds.Min.Y) * 0.5f, AutoBodyMinExtent);
        const float Radius = (std::max)((std::max)(RadiusX, RadiusY) * AutoBodyRadiusPadding, AutoBodyMinExtent);

        OutFit.BodyComponentTransform.Location = Center;
        OutFit.BodyComponentTransform.Rotation = MakeQuatFromLocalAxes(AxisX, AxisY, AxisZ);
        OutFit.BodyComponentTransform.Scale = FVector::OneVector;
        OutFit.BoxHalfExtent = FVector(RadiusX, RadiusY, HalfLength);
        OutFit.CapsuleRadius = Radius;
        OutFit.CapsuleHalfHeight = (std::max)(HalfLength, Radius + AutoBodyMinExtent);
        return true;
    }

    bool ShouldPreferBoneAxisFitOverPCA(
        const FAutoBodyFitResult& PcaFit,
        const FAutoBodyFitResult& BoneAxisFit,
        const FVector& PcaAxisZ,
        const FVector& BoneAxisZ)
    {
        const float AxisAlignment = std::fabs(PcaAxisZ.Dot(BoneAxisZ));
        if (AxisAlignment < AutoBodyPcaBoneAxisAlignmentThreshold)
        {
            return true;
        }

        if (PcaFit.CapsuleRadius > BoneAxisFit.CapsuleRadius * AutoBodyPcaRadiusRegressionThreshold)
        {
            return true;
        }

        return AxisAlignment < AutoBodyPcaBoneAxisSoftAlignmentThreshold &&
            BoneAxisFit.CapsuleRadius <= PcaFit.CapsuleRadius * AutoBodyPcaBoneAxisRadiusTolerance &&
            BoneAxisFit.CapsuleHalfHeight >= PcaFit.CapsuleHalfHeight * 0.75f;
    }

    int32 FindPCAAxisMostAlignedWithBone(const FVector EigenVectors[3], const FVector& BoneAxis)
    {
        int32 BestIndex = 0;
        float BestAlignment = -1.0f;
        for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            const float Alignment = std::fabs(EigenVectors[AxisIndex].Dot(BoneAxis));
            if (Alignment > BestAlignment)
            {
                BestAlignment = Alignment;
                BestIndex = AxisIndex;
            }
        }
        return BestIndex;
    }

    int32 FindBestPCASecondaryAxisIndex(const double EigenValues[3], int32 PrimaryAxisIndex)
    {
        int32 BestIndex = PrimaryAxisIndex == 0 ? 1 : 0;
        for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            if (AxisIndex == PrimaryAxisIndex)
            {
                continue;
            }

            if (AxisIndex != BestIndex && EigenValues[AxisIndex] > EigenValues[BestIndex])
            {
                BestIndex = AxisIndex;
            }
        }
        return BestIndex;
    }

    void ComputeSymmetricEigen3x3(const double InMatrix[3][3], double OutValues[3], FVector OutVectors[3])
    {
        double A[3][3] = {};
        double V[3][3] = {};
        for (int32 Row = 0; Row < 3; ++Row)
        {
            for (int32 Col = 0; Col < 3; ++Col)
            {
                A[Row][Col] = InMatrix[Row][Col];
                V[Row][Col] = Row == Col ? 1.0 : 0.0;
            }
        }

        for (int32 Iteration = 0; Iteration < 32; ++Iteration)
        {
            int32 P = 0;
            int32 Q = 1;
            double MaxOffDiagonal = std::fabs(A[0][1]);
            if (std::fabs(A[0][2]) > MaxOffDiagonal)
            {
                P = 0;
                Q = 2;
                MaxOffDiagonal = std::fabs(A[0][2]);
            }
            if (std::fabs(A[1][2]) > MaxOffDiagonal)
            {
                P = 1;
                Q = 2;
                MaxOffDiagonal = std::fabs(A[1][2]);
            }
            if (MaxOffDiagonal < 1.0e-10)
            {
                break;
            }

            const double App = A[P][P];
            const double Aqq = A[Q][Q];
            const double Apq = A[P][Q];
            const double Theta = (Aqq - App) / (2.0 * Apq);
            const double Sign = Theta >= 0.0 ? 1.0 : -1.0;
            const double T = Sign / (std::fabs(Theta) + std::sqrt(Theta * Theta + 1.0));
            const double C = 1.0 / std::sqrt(T * T + 1.0);
            const double S = T * C;

            for (int32 K = 0; K < 3; ++K)
            {
                if (K == P || K == Q)
                {
                    continue;
                }
                const double Akp = A[K][P];
                const double Akq = A[K][Q];
                A[K][P] = A[P][K] = C * Akp - S * Akq;
                A[K][Q] = A[Q][K] = S * Akp + C * Akq;
            }

            A[P][P] = C * C * App - 2.0 * S * C * Apq + S * S * Aqq;
            A[Q][Q] = S * S * App + 2.0 * S * C * Apq + C * C * Aqq;
            A[P][Q] = A[Q][P] = 0.0;

            for (int32 K = 0; K < 3; ++K)
            {
                const double Vkp = V[K][P];
                const double Vkq = V[K][Q];
                V[K][P] = C * Vkp - S * Vkq;
                V[K][Q] = S * Vkp + C * Vkq;
            }
        }

        int32 Order[3] = { 0, 1, 2 };
        std::sort(
            Order,
            Order + 3,
            [&](int32 AIndex, int32 BIndex)
            {
                return A[AIndex][AIndex] > A[BIndex][BIndex];
            });

        for (int32 Rank = 0; Rank < 3; ++Rank)
        {
            const int32 SourceIndex = Order[Rank];
            OutValues[Rank] = A[SourceIndex][SourceIndex];
            OutVectors[Rank] = FVector(
                static_cast<float>(V[0][SourceIndex]),
                static_cast<float>(V[1][SourceIndex]),
                static_cast<float>(V[2][SourceIndex]));
            OutVectors[Rank] = SafeNormal(OutVectors[Rank], Rank == 0 ? FVector::ZAxisVector : FVector::XAxisVector);
        }
    }

    float Clamp01(float Value)
    {
        return (std::min)((std::max)(Value, 0.0f), 1.0f);
    }

    float DistancePointToSegment(const FVector& Point, const FVector& SegmentStart, const FVector& SegmentEnd)
    {
        const FVector Segment = SegmentEnd - SegmentStart;
        const float SegmentLengthSq = Segment.Dot(Segment);
        if (SegmentLengthSq <= 1.0e-8f)
        {
            return FVector::Distance(Point, SegmentStart);
        }

        const float T = Clamp01((Point - SegmentStart).Dot(Segment) / SegmentLengthSq);
        const FVector ClosestPoint = SegmentStart + Segment * T;
        return FVector::Distance(Point, ClosestPoint);
    }

    FVector ComputeWeightedMean(const TArray<FVector>& Points, const TArray<float>& PointWeights)
    {
        FVector Mean = FVector::ZeroVector;
        float TotalWeight = 0.0f;
        for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
        {
            const float Weight = GetPointPCAWeight(PointWeights, PointIndex);
            if (Weight <= 0.0f)
            {
                continue;
            }
            Mean += Points[PointIndex] * Weight;
            TotalWeight += Weight;
        }

        return TotalWeight > 1.0e-8f ? Mean / TotalWeight : FVector::ZeroVector;
    }

    float EstimateCapsuleRadiusAroundSegment(
        const TArray<FVector>& Points,
        const TArray<float>& PointWeights,
        const FVector& SegmentStart,
        const FVector& SegmentEnd,
        float SegmentLength)
    {
        float WeightedDistance = 0.0f;
        float MaxDistance = 0.0f;
        float WeightSum = 0.0f;
        for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
        {
            const float Weight = GetPointPCAWeight(PointWeights, PointIndex);
            if (Weight <= 0.0f)
            {
                continue;
            }

            const float Distance = DistancePointToSegment(Points[PointIndex], SegmentStart, SegmentEnd);
            WeightedDistance += Distance * Weight;
            MaxDistance = (std::max)(MaxDistance, Distance);
            WeightSum += Weight;
        }

        const float BoneLengthRadius = (std::max)(SegmentLength * AutoBodyFallbackRadiusRatio, AutoBodyMinExtent);
        if (WeightSum <= 1.0e-6f)
        {
            return BoneLengthRadius;
        }

        const float MeanDistance = WeightedDistance / WeightSum;
        const float PointRadius = (std::max)(MeanDistance * 1.35f, AutoBodyMinExtent);
        return (std::min)((std::max)(PointRadius, BoneLengthRadius * 0.50f), BoneLengthRadius * 1.75f);
    }

    void BuildSkeletonSegmentAutoBodyFit(
        const TArray<FVector>& Points,
        const TArray<float>& PointWeights,
        const FVector& SegmentStart,
        const FVector& SegmentEnd,
        FAutoBodyFitResult& OutFit)
    {
        FVector AxisX;
        FVector AxisY;
        FVector AxisZ;
        BuildBasisFromZAxis(SegmentEnd - SegmentStart, AxisX, AxisY, AxisZ);

        const float SegmentLength = (std::max)(FVector::Distance(SegmentStart, SegmentEnd), AutoBodyMinExtent * 2.0f);
        const float Radius = EstimateCapsuleRadiusAroundSegment(Points, PointWeights, SegmentStart, SegmentEnd, SegmentLength);
        OutFit.BodyComponentTransform.Location = (SegmentStart + SegmentEnd) * 0.5f;
        OutFit.BodyComponentTransform.Rotation = MakeQuatFromLocalAxes(AxisX, AxisY, AxisZ);
        OutFit.BodyComponentTransform.Scale = FVector::OneVector;
        OutFit.BoxHalfExtent = FVector(Radius, Radius, SegmentLength * 0.5f);
        OutFit.CapsuleRadius = Radius;
        OutFit.CapsuleHalfHeight = (std::max)(SegmentLength * 0.5f, Radius + AutoBodyMinExtent);
    }

    bool ShouldForceSkeletonSegmentFit(
        const TArray<FVector>& Points,
        const TArray<float>& PointWeights,
        const FVector& SegmentStart,
        const FVector& SegmentEnd)
    {
        const float SegmentLength = FVector::Distance(SegmentStart, SegmentEnd);
        if (SegmentLength <= AutoBodyMinExtent * 2.0f)
        {
            return false;
        }

        if (Points.empty())
        {
            return true;
        }

        const FVector Mean = ComputeWeightedMean(Points, PointWeights);
        const float MeanDistance = DistancePointToSegment(Mean, SegmentStart, SegmentEnd);
        const float AllowedDistance = (std::max)(SegmentLength * 0.85f, AutoBodyMinExtent * 4.0f);
        if (MeanDistance > AllowedDistance)
        {
            return true;
        }

        int32 FarPointCount = 0;
        float WeightSum = 0.0f;
        float FarWeightSum = 0.0f;
        for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
        {
            const float Weight = GetPointPCAWeight(PointWeights, PointIndex);
            WeightSum += Weight;
            const float Distance = DistancePointToSegment(Points[PointIndex], SegmentStart, SegmentEnd);
            if (Distance > AllowedDistance)
            {
                ++FarPointCount;
                FarWeightSum += Weight;
            }
        }

        return FarPointCount > static_cast<int32>(Points.size()) / 2 ||
            (WeightSum > 1.0e-6f && FarWeightSum > WeightSum * 0.55f);
    }

    bool IsFitCenterSuspiciousAgainstSegment(
        const FAutoBodyFitResult& Fit,
        const FVector& SegmentStart,
        const FVector& SegmentEnd)
    {
        const float SegmentLength = FVector::Distance(SegmentStart, SegmentEnd);
        if (SegmentLength <= AutoBodyMinExtent * 2.0f)
        {
            return false;
        }

        const float Distance = DistancePointToSegment(Fit.BodyComponentTransform.Location, SegmentStart, SegmentEnd);
        const float AllowedDistance = (std::max)(SegmentLength * 0.75f, Fit.CapsuleRadius * 2.5f);
        return Distance > AllowedDistance;
    }

    bool BuildPCAAutoBodyFit(
        const TArray<FVector>& Points,
        const TArray<float>& PointWeights,
        const FVector& BoneAxis,
        FAutoBodyFitResult& OutFit)
    {
        if (Points.size() < 4)
        {
            return false;
        }

        FVector Mean = FVector::ZeroVector;
        double TotalWeight = 0.0;
        for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
        {
            const double Weight = static_cast<double>(GetPointPCAWeight(PointWeights, PointIndex));
            if (Weight <= 0.0)
            {
                continue;
            }

            Mean += Points[PointIndex] * static_cast<float>(Weight);
            TotalWeight += Weight;
        }
        if (TotalWeight <= 1.0e-8)
        {
            return false;
        }
        Mean /= static_cast<float>(TotalWeight);

        double Covariance[3][3] = {};
        for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
        {
            const double Weight = static_cast<double>(GetPointPCAWeight(PointWeights, PointIndex));
            if (Weight <= 0.0)
            {
                continue;
            }

            const FVector& Point = Points[PointIndex];
            const FVector Delta = Point - Mean;
            for (int32 Row = 0; Row < 3; ++Row)
            {
                for (int32 Col = 0; Col < 3; ++Col)
                {
                    Covariance[Row][Col] +=
                        Weight *
                        static_cast<double>(GetVectorComponent(Delta, Row)) *
                        static_cast<double>(GetVectorComponent(Delta, Col));
                }
            }
        }
        for (int32 Row = 0; Row < 3; ++Row)
        {
            for (int32 Col = 0; Col < 3; ++Col)
            {
                Covariance[Row][Col] /= TotalWeight;
            }
        }

        double EigenValues[3] = {};
        FVector EigenVectors[3];
        ComputeSymmetricEigen3x3(Covariance, EigenValues, EigenVectors);
        if (EigenValues[0] <= 1.0e-8)
        {
            return false;
        }

        const FVector StableBoneAxis = SafeNormal(BoneAxis, EigenVectors[0]);
        const int32 AxisZIndex = FindPCAAxisMostAlignedWithBone(EigenVectors, StableBoneAxis);

        FVector AxisZ = SafeNormal(EigenVectors[AxisZIndex], StableBoneAxis);
        if (AxisZ.Dot(StableBoneAxis) < 0.0f)
        {
            AxisZ *= -1.0f;
        }

        const int32 AxisXIndex = FindBestPCASecondaryAxisIndex(EigenValues, AxisZIndex);
        FVector AxisX = EigenVectors[AxisXIndex] - AxisZ * EigenVectors[AxisXIndex].Dot(AxisZ);
        if (AxisX.IsNearlyZero(1.0e-5f))
        {
            FVector AxisY;
            BuildBasisFromZAxis(AxisZ, AxisX, AxisY, AxisZ);
        }
        else
        {
            AxisX.Normalize();
        }
        FVector AxisY = SafeNormal(AxisZ.Cross(AxisX), FVector::YAxisVector);
        AxisX = SafeNormal(AxisY.Cross(AxisZ), AxisX);

        FAutoBodyFitResult PcaFit;
        if (!FitCapsuleToPointsOnBasis(Points, PointWeights, Mean, AxisX, AxisY, AxisZ, PcaFit))
        {
            return false;
        }

        FVector BoneAxisX;
        FVector BoneAxisY;
        FVector BoneAxisZ;
        BuildBasisFromZAxis(StableBoneAxis, BoneAxisX, BoneAxisY, BoneAxisZ);

        FAutoBodyFitResult BoneAxisFit;
        if (FitCapsuleToPointsOnBasis(Points, PointWeights, Mean, BoneAxisX, BoneAxisY, BoneAxisZ, BoneAxisFit) &&
            ShouldPreferBoneAxisFitOverPCA(PcaFit, BoneAxisFit, AxisZ, BoneAxisZ))
        {
            OutFit = BoneAxisFit;
            return true;
        }

        OutFit = PcaFit;
        return true;
    }

    bool BuildBoneAxisAutoBodyFit(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        int32 MeshBoneIndex,
        const TArray<FVector>& Points,
        const TArray<float>& PointWeights,
        FAutoBodyFitResult& OutFit)
    {
        FVector SegmentStart;
        FVector SegmentEnd;
        if (!GetBoneAxisSegment(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex, SegmentStart, SegmentEnd))
        {
            return false;
        }

        if (ShouldForceSkeletonSegmentFit(Points, PointWeights, SegmentStart, SegmentEnd))
        {
            BuildSkeletonSegmentAutoBodyFit(Points, PointWeights, SegmentStart, SegmentEnd, OutFit);
            return true;
        }

        FVector AxisX;
        FVector AxisY;
        FVector AxisZ;
        BuildBasisFromZAxis(SegmentEnd - SegmentStart, AxisX, AxisY, AxisZ);

        if (FitCapsuleToPointsOnBasis(Points, PointWeights, SegmentStart, AxisX, AxisY, AxisZ, OutFit) &&
            !IsFitCenterSuspiciousAgainstSegment(OutFit, SegmentStart, SegmentEnd))
        {
            return true;
        }

        BuildSkeletonSegmentAutoBodyFit(Points, PointWeights, SegmentStart, SegmentEnd, OutFit);
        return true;
    }

    bool BuildAutoBodyFit(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        int32 MeshBoneIndex,
        const TArray<FVector>& Points,
        const TArray<float>& PointWeights,
        EPhysicsAssetAutoBodyMethod Method,
        bool bAllowBoneAxisFallback,
        FAutoBodyFitResult& OutFit)
    {
        FVector SegmentStart;
        FVector SegmentEnd;
        GetBoneAxisSegment(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex, SegmentStart, SegmentEnd);
        const FVector BoneAxis = SegmentEnd - SegmentStart;

        if (ShouldForceSkeletonSegmentFit(Points, PointWeights, SegmentStart, SegmentEnd))
        {
            BuildSkeletonSegmentAutoBodyFit(Points, PointWeights, SegmentStart, SegmentEnd, OutFit);
            return true;
        }

        if (Method == EPhysicsAssetAutoBodyMethod::PCAAnalysis)
        {
            if (BuildPCAAutoBodyFit(Points, PointWeights, BoneAxis, OutFit) &&
                !IsFitCenterSuspiciousAgainstSegment(OutFit, SegmentStart, SegmentEnd))
            {
                return true;
            }

            if (!bAllowBoneAxisFallback)
            {
                return false;
            }
        }

        return BuildBoneAxisAutoBodyFit(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex, Points, PointWeights, OutFit);
    }

    FPhysicsAssetShapeSetup BuildShapeSetupFromFit(
        const FAutoBodyFitResult& Fit,
        const FPhysicsAssetAutoBodyGeneratorOptions& Options)
    {
        const float FitPadding = (std::max)(Options.FitPadding, 0.01f);
        const FVector BoxHalfExtent = ClampAutoBodyHalfExtent(Fit.BoxHalfExtent * FitPadding);
        const float SphereRadius = (std::max)(GetMaxComponent(BoxHalfExtent), AutoBodyMinExtent);
        const float CapsuleRadius = (std::max)(Fit.CapsuleRadius * FitPadding, AutoBodyMinExtent);
        const float CapsuleHalfHeight = (std::max)(Fit.CapsuleHalfHeight * FitPadding, CapsuleRadius + AutoBodyMinExtent);

        FPhysicsAssetShapeSetup Shape;
        Shape.LocalTransform = FTransform();
        Shape.BoxHalfExtent = BoxHalfExtent;
        Shape.SphereRadius = SphereRadius;
        Shape.CapsuleRadius = CapsuleRadius;
        Shape.CapsuleHalfHeight = CapsuleHalfHeight;

        switch (Options.PrimitiveType)
        {
        case EPhysicsAssetAutoBodyPrimitiveType::Box:
            Shape.Type = EPhysicsAssetShapeType::Box;
            break;
        case EPhysicsAssetAutoBodyPrimitiveType::Sphere:
            Shape.Type = EPhysicsAssetShapeType::Sphere;
            Shape.BoxHalfExtent = FVector(SphereRadius, SphereRadius, SphereRadius);
            Shape.CapsuleRadius = SphereRadius;
            Shape.CapsuleHalfHeight = SphereRadius;
            break;
        case EPhysicsAssetAutoBodyPrimitiveType::Capsule:
        default:
            Shape.Type = EPhysicsAssetShapeType::Capsule;
            break;
        }

        return Shape;
    }

    bool ComputeBodyComponentTransformFromSetup(
        const FSkeletalMesh* MeshAsset,
        const TArray<FMatrix>* OverrideBoneGlobalMatrices,
        const FPhysicsAssetBodySetup& BodySetup,
        FTransform& OutBodyComponentTransform)
    {
        if (!MeshAsset || !HasBoneName(BodySetup.BoneName))
        {
            return false;
        }

        const int32 MeshBoneIndex = FindMeshBoneIndexByName(MeshAsset, BodySetup.BoneName.ToString());
        if (MeshBoneIndex < 0 || MeshBoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
        {
            return false;
        }

        const FTransform BoneComponentTransform = MakeTransformNoScale(GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex));
        OutBodyComponentTransform = ComposeComponentTransforms(BoneComponentTransform, BodySetup.BodyLocalFrame);
        return true;
    }
} //namespace 실화?

bool FPhysicsAssetAutoBodyGenerator::Regenerate(
    UPhysicsAsset* PhysicsAsset,
    USkeletalMesh* SkeletalMesh,
    const FPhysicsAssetAutoBodyGeneratorOptions& Options,
    FPhysicsAssetAutoBodyGeneratorResult* OutResult,
    const TArray<FMatrix>* OverrideBoneGlobalMatrices)
{
    FPhysicsAssetAutoBodyGeneratorResult Result;

    if (!PhysicsAsset || !SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        if (OutResult)
        {
            *OutResult = Result;
        }
        return false;
    }

    FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
    if (!MeshAsset)
    {
        if (OutResult)
        {
            *OutResult = Result;
        }
        return false;
    }

    const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetSkeleton()->GetReferenceSkeleton();
    if (RefSkeleton.GetNumBones() <= 0)
    {
        if (OutResult)
        {
            *OutResult = Result;
        }
        return false;
    }

    if (Options.bReplaceExisting)
    {
        PhysicsAsset->ClearBodySetups();
        Result.bAssetChanged = true;
    }

    const TArray<FAutoBodyPointFit> BoneFits = BuildBonePointFits(
        RefSkeleton,
        MeshAsset,
        OverrideBoneGlobalMatrices,
        Options);
    const FMergedAutoBodyData MergedData = BuildMergedAutoBodyData(
        RefSkeleton,
        BoneFits,
        Options);

    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNumBones(); ++BoneIndex)
    {
        const FReferenceBone& RefBone = RefSkeleton.Bones[BoneIndex];
        const FName BoneName(RefBone.Name);
        if (PhysicsAsset->HasBodySetupForBone(BoneName))
        {
            continue;
        }

        if (!ShouldGenerateBodyForBone(RefSkeleton, MergedData, Options, BoneIndex))
        {
            ++Result.SkippedBoneCount;
            continue;
        }

        const int32 MeshBoneIndex = FindMeshBoneIndexByName(MeshAsset, RefBone.Name);
        if (MeshBoneIndex < 0 || MeshBoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
        {
            ++Result.SkippedBoneCount;
            continue;
        }

        const FAutoBodyPointFit EmptyFit;
        const FAutoBodyPointFit& OwnFit = BoneIndex < static_cast<int32>(BoneFits.size()) ? BoneFits[BoneIndex] : EmptyFit;
        const FAutoBodyPointFit& ExtraFit = BoneIndex < static_cast<int32>(MergedData.ExtraFits.size()) ? MergedData.ExtraFits[BoneIndex] : EmptyFit;
        const FAutoBodyPointFit CombinedFit = MakeCombinedPointFit(OwnFit, ExtraFit);
        const TArray<FVector>& Points = CombinedFit.Points;
        const TArray<float>& PointWeights = CombinedFit.PointWeights;

        const int32 MinWeightedVertices = (std::max)(Options.MinWeightedVertices, 1);
        if (static_cast<int32>(Points.size()) < MinWeightedVertices && !Options.bAllowBoneAxisFallback)
        {
            ++Result.SkippedBoneCount;
            continue;
        }

        FAutoBodyFitResult Fit;
        if (!BuildAutoBodyFit(
                MeshAsset,
                OverrideBoneGlobalMatrices,
                MeshBoneIndex,
                Points,
                PointWeights,
                Options.Method,
                Options.bAllowBoneAxisFallback,
                Fit))
        {
            ++Result.SkippedBoneCount;
            continue;
        }

        const FTransform BoneComponentTransform = MakeTransformNoScale(GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, MeshBoneIndex));

        FPhysicsAssetBodySetup Body;
        Body.BoneName = BoneName;
        Body.BodyLocalFrame = MakeLocalTransformFromComponent(BoneComponentTransform, Fit.BodyComponentTransform);
        Body.Shapes.push_back(BuildShapeSetupFromFit(Fit, Options));

        const int32 NewBodyIndex = PhysicsAsset->AddBodySetup(Body);
        if (NewBodyIndex >= 0)
        {
            if (Result.FirstGeneratedBodyIndex < 0)
            {
                Result.FirstGeneratedBodyIndex = NewBodyIndex;
                Result.FirstGeneratedBoneName = BoneName;
            }
            ++Result.GeneratedBodyCount;
            Result.bAssetChanged = true;
        }
    }

    if (Options.bCreateConstraints)
    {
        for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNumBones(); ++BoneIndex)
        {
            const FName ChildBoneName(RefSkeleton.Bones[BoneIndex].Name);
            const int32 ChildBodyIndex = PhysicsAsset->FindBodySetupIndexByBoneName(ChildBoneName);
            if (ChildBodyIndex < 0)
            {
                continue;
            }

            int32 ParentBoneIndex = RefSkeleton.Bones[BoneIndex].ParentIndex;
            while (ParentBoneIndex >= 0 && ParentBoneIndex < RefSkeleton.GetNumBones())
            {
                const FName ParentBoneName(RefSkeleton.Bones[ParentBoneIndex].Name);
                const int32 ParentBodyIndex = PhysicsAsset->FindBodySetupIndexByBoneName(ParentBoneName);
                if (ParentBodyIndex >= 0)
                {
                    if (!PhysicsAsset->HasConstraintBetweenBones(ParentBoneName, ChildBoneName))
                    {
                        FPhysicsAssetConstraintSetup Constraint;
                        Constraint.ParentBoneName = ParentBoneName;
                        Constraint.ChildBoneName = ChildBoneName;
                        Constraint.bDisableCollisionBetweenBodies = Options.bDisableCollisionBetweenConstrainedBodies;

                        const int32 ChildMeshBoneIndex = FindMeshBoneIndexByName(MeshAsset, RefSkeleton.Bones[BoneIndex].Name);
                        FTransform ParentBodyComponentTransform;
                        FTransform ChildBodyComponentTransform;
                        if (ChildMeshBoneIndex >= 0 &&
                            ComputeBodyComponentTransformFromSetup(MeshAsset, OverrideBoneGlobalMatrices, PhysicsAsset->GetBodySetups()[ParentBodyIndex], ParentBodyComponentTransform) &&
                            ComputeBodyComponentTransformFromSetup(MeshAsset, OverrideBoneGlobalMatrices, PhysicsAsset->GetBodySetups()[ChildBodyIndex], ChildBodyComponentTransform))
                        {
                            FTransform JointComponentTransform;
                            JointComponentTransform.Location = GetGenerationBoneGlobalPose(MeshAsset, OverrideBoneGlobalMatrices, ChildMeshBoneIndex).GetLocation();
                            JointComponentTransform.Rotation = ChildBodyComponentTransform.Rotation;
                            JointComponentTransform.Scale = FVector::OneVector;
                            Constraint.ParentLocalFrame = MakeLocalTransformFromComponent(ParentBodyComponentTransform, JointComponentTransform);
                            Constraint.ChildLocalFrame = MakeLocalTransformFromComponent(ChildBodyComponentTransform, JointComponentTransform);
                        }

                        if (PhysicsAsset->AddConstraintSetup(Constraint) >= 0)
                        {
                            ++Result.GeneratedConstraintCount;
                            Result.bAssetChanged = true;
                        }
                    }
                    break;
                }
                ParentBoneIndex = RefSkeleton.Bones[ParentBoneIndex].ParentIndex;
            }
        }
    }

    if (!Result.bAssetChanged && !Options.bReplaceExisting)
    {
        if (OutResult)
        {
            *OutResult = Result;
        }
        return false;
    }

    if (OutResult)
    {
        *OutResult = Result;
    }
    return true;
}
