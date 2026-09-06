#pragma once

#include "Core/Types/CoreTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/Buffer.h"
#include "Math/Matrix.h"
#include "Serialization/Archive.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <algorithm>
#include <cmath>
#include <memory>

inline void SerializeSkeletalMatrix(FArchive& Ar, FMatrix& Matrix)
{
	Ar.Serialize(Matrix.Data, sizeof(float) * 16);
}

// 행렬 basis(회전 축)의 스케일을 1 로 접되 translation 은 그대로 둔다.
// 본 글로벌에 베이크된 균일 스케일을 제거해 "본=스케일1(rigid)" 전제(래그돌 재구성·캡슐 피팅)
// 와 일관시키는 데 쓴다. 스키닝(InvBind×Global)에서는 어차피 스케일이 상쇄되므로 표시 결과 불변.
inline FMatrix StripMatrixBasisScale(const FMatrix& In)
{
	const FVector S = In.GetScale();
	const float SX = std::abs(S.X) > 1e-8f ? S.X : 1.0f;
	const float SY = std::abs(S.Y) > 1e-8f ? S.Y : 1.0f;
	const float SZ = std::abs(S.Z) > 1e-8f ? S.Z : 1.0f;

	FMatrix Out = In;
	Out.M[0][0] /= SX; Out.M[0][1] /= SX; Out.M[0][2] /= SX;
	Out.M[1][0] /= SY; Out.M[1][1] /= SY; Out.M[1][2] /= SY;
	Out.M[2][0] /= SZ; Out.M[2][1] /= SZ; Out.M[2][2] /= SZ;
	// row 3(translation)과 동차 열은 보존.
	return Out;
}

struct FBone
{
	FString Name;
	int32 ParentIndex = -1;

	// Runtime pose semantics. The legacy LocalMatrix/GlobalMatrix slots are kept
	// for existing package layout, but code should use these named poses.
	FMatrix ReferenceLocalPose = FMatrix::Identity;
	FMatrix ReferenceGlobalPose = FMatrix::Identity;
	FMatrix SkinBindGlobalPose = FMatrix::Identity;
	FMatrix LocalMatrix = FMatrix::Identity;
	FMatrix GlobalMatrix = FMatrix::Identity;
	FMatrix InverseBindPoseMatrix = FMatrix::Identity;

	void SyncSeparatedPoseDataFromLegacy()
	{
		ReferenceLocalPose = LocalMatrix;
		ReferenceGlobalPose = GlobalMatrix;
		SkinBindGlobalPose = GlobalMatrix;
	}

	void SyncLegacyPoseDataFromSeparated()
	{
		LocalMatrix = ReferenceLocalPose;
		GlobalMatrix = SkinBindGlobalPose;
	}

	const FMatrix& GetReferenceLocalPose() const { return ReferenceLocalPose; }
	const FMatrix& GetReferenceGlobalPose() const { return ReferenceGlobalPose; }
	const FMatrix& GetSkinBindGlobalPose() const { return SkinBindGlobalPose; }
	const FMatrix& GetInverseBindPose() const { return InverseBindPoseMatrix; }

	friend FArchive& operator<<(FArchive& Ar, FBone& Bone)
	{
		if (Ar.IsSaving())
		{
			Bone.SyncLegacyPoseDataFromSeparated();
		}

		Ar << Bone.Name;
		Ar << Bone.ParentIndex;
		SerializeSkeletalMatrix(Ar, Bone.LocalMatrix);
		SerializeSkeletalMatrix(Ar, Bone.GlobalMatrix);
		SerializeSkeletalMatrix(Ar, Bone.InverseBindPoseMatrix);

		if (Ar.IsLoading())
		{
			Bone.SyncSeparatedPoseDataFromLegacy();
		}
		return Ar;
	}
};

struct FSkeletalMeshSection
{
	int32 MaterialIndex = -1;
	FString MaterialSlotName;
	uint32 FirstIndex;
	uint32 IndexCount;

	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshSection& Section)
	{
		Ar << Section.MaterialSlotName;
		Ar << Section.FirstIndex;
		Ar << Section.IndexCount;
		return Ar;
	}
};

struct FSkeletalMaterial
{
	UMaterial* MaterialInterface = nullptr;
	FString MaterialSlotName = "None";
	FString MaterialPath;

	friend FArchive& operator<<(FArchive& Ar, FSkeletalMaterial& Mat)
	{
		Ar << Mat.MaterialSlotName;

		// Material 포인터는 실행마다 달라질 수 있다.
		// .sketbin에는 다시 찾을 수 있는 .mat 경로만 저장한다.
		if (Ar.IsSaving() && Mat.MaterialInterface)
		{
			Mat.MaterialPath = Mat.MaterialInterface->GetAssetPathFileName();
		}
		Ar << Mat.MaterialPath;

		if (Ar.IsLoading())
		{
			if (!Mat.MaterialPath.empty())
			{
				Mat.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(Mat.MaterialPath);
			}
			else
			{
				Mat.MaterialInterface = nullptr;
			}
		}

		return Ar;
	}
};

struct FSkeletalMeshRange
{
	uint32 VertexStart = 0;
	uint32 VertexEnd = 0;
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
	// Legacy serialization slot. New imports bake mesh bind transforms into vertices.
	FMatrix MeshBindGlobal = FMatrix::Identity;

	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshRange& Range)
	{
		Ar << Range.VertexStart;
		Ar << Range.VertexEnd;
		Ar << Range.FirstIndex;
		Ar << Range.IndexCount;
		SerializeSkeletalMatrix(Ar, Range.MeshBindGlobal);
		return Ar;
	}
};

struct FMorphTargetDelta
{
	uint32  VertexIndex   = 0;
	FVector PositionDelta = FVector::ZeroVector;

	friend FArchive& operator<<(FArchive& Ar, FMorphTargetDelta& Delta)
	{
		Ar << Delta.VertexIndex;
		Ar << Delta.PositionDelta;
		return Ar;
	}
};

struct FMorphTarget
{
	FString                   Name;
	TArray<FMorphTargetDelta> Deltas;

	friend FArchive& operator<<(FArchive& Ar, FMorphTarget& Target)
	{
		Ar << Target.Name;
		Ar << Target.Deltas;
		return Ar;
	}
};

struct FSkeletalMesh
{
	FString PathFileName;
	FString SkeletonPath = "None";
	FString SkeletonAssetGuid;
	FString SkeletonCompatibilitySignature;

	TArray<FVertexPNCTBW> Vertices;
	TArray<uint32> Indices;

	TArray<FSkeletalMeshSection> Sections;
	TArray<FSkeletalMeshRange> MeshRanges;

	TArray<FBone>        Bones;
	TArray<FMorphTarget> MorphTargets;

	std::unique_ptr<FMeshBuffer> RenderBuffer;

	FVector BoundsCenter = FVector(0, 0, 0);
	FVector BoundsExtent = FVector(0, 0, 0);
	bool    bBoundsValid = false;

	int32 FindMorphTargetIndex(const FString& TargetName) const
	{
		for (int32 Index = 0; Index < static_cast<int32>(MorphTargets.size()); ++Index)
		{
			if (MorphTargets[Index].Name == TargetName)
			{
				return Index;
			}
		}
		return -1;
	}

	const FMorphTarget* FindMorphTarget(const FString& TargetName) const
	{
		const int32 Index = FindMorphTargetIndex(TargetName);
		return Index >= 0 ? &MorphTargets[Index] : nullptr;
	}

	int32 FindBoneIndex(const FString& BoneName) const
	{
		for (int32 Index = 0; Index < static_cast<int32>(Bones.size()); ++Index)
		{
			if (Bones[Index].Name == BoneName)
			{
				return Index;
			}
		}
		return -1;
	}

	// 이 본이 스키닝에 관여하는가(= 어떤 버텍스든 가중치>0 으로 참조). 헬퍼/IK/트위스트처럼
	// 스킨에 안 쓰이는 본을 걸러낼 때 사용(예: PhysicsAsset 바디 자동 생성 대상 판정).
	// 최초 호출 시 버텍스를 1회 스캔해 마스크를 캐싱한다(직렬화 제외).
	bool IsBoneSkinned(int32 BoneIndex) const
	{
		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
		{
			return false;
		}
		EnsureSkinnedBoneMask();
		return SkinnedBoneMask[BoneIndex];
	}

	bool IsBoneSkinned(const FString& BoneName) const
	{
		return IsBoneSkinned(FindBoneIndex(BoneName));
	}

	// 스키닝 관여 본 마스크 캐시(직렬화 제외, lazy 빌드). 메시 재임포트 시 FSkeletalMesh 객체가
	// 재생성되므로 별도 무효화는 두지 않는다. IsBoneSkinned 가 최초 호출 시 한 번 채운다.
	void EnsureSkinnedBoneMask() const
	{
		if (bSkinnedBoneMaskValid)
		{
			return;
		}
		SkinnedBoneMask.assign(Bones.size(), false);
		for (const FVertexPNCTBW& Vertex : Vertices)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				const int32 BoneIdx = Vertex.BoneIndices[i];
				if (BoneIdx >= 0 && BoneIdx < static_cast<int32>(SkinnedBoneMask.size()) && Vertex.BoneWeights[i] > 0.0f)
				{
					SkinnedBoneMask[BoneIdx] = true;
				}
			}
		}
		bSkinnedBoneMaskValid = true;
	}

	mutable TArray<bool> SkinnedBoneMask;
	mutable bool         bSkinnedBoneMaskValid = false;

	void NormalizeBonePoseData()
	{
		for (FBone& Bone : Bones)
		{
			Bone.ReferenceLocalPose = Bone.LocalMatrix;
			Bone.SkinBindGlobalPose = Bone.GlobalMatrix;
		}

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
		{
			FBone& Bone = Bones[BoneIndex];
			Bone.ReferenceGlobalPose = (Bone.ParentIndex >= 0 && Bone.ParentIndex < BoneIndex)
				? Bone.ReferenceLocalPose * Bones[Bone.ParentIndex].ReferenceGlobalPose
				: Bone.ReferenceLocalPose;
		}

		NormalizeBindPoseScale();
	}

	// 스켈레톤 바인드 포즈 basis 에 균일 스케일 S 가 베이크된 메시(예: cm/m 단위 불일치로
	// import 된 FBX)를 단위 스케일로 정규화한다. 스키닝은 InvBind×Global 에서 S 가 상쇄돼
	// 정상이지만, 래그돌은 바디에서 스케일1 rigid 글로벌을 재구성하고 캡슐 피팅도 InvBind(1/S)
	// 공간에서 재므로 "스케일1" 전제가 깨져 시뮬 시 메시가 1/S 로 수축하고 캡슐이 S 배 작아진다.
	// basis 스케일만 1 로 접고 translation(본 실제 위치)은 보존 → 표시 결과 불변, 다운스트림 일관.
	// 단위 스케일(정상) 메시는 감지 후 손대지 않는다. NormalizeBonePoseData 끝에서 호출.
	void NormalizeBindPoseScale()
	{
		if (Bones.empty()) { return; }

		// 베이크된 비단위 스케일 감지 — 없으면 그대로 둔다(기존 정상 메시 무변경).
		bool bHasBakedScale = false;
		for (const FBone& Bone : Bones)
		{
			const FVector S = Bone.ReferenceGlobalPose.GetScale();
			if (std::abs(S.X - 1.0f) > 1e-3f || std::abs(S.Y - 1.0f) > 1e-3f || std::abs(S.Z - 1.0f) > 1e-3f)
			{
				bHasBakedScale = true;
				break;
			}
		}
		if (!bHasBakedScale) { return; }

		// 1) 본 글로벌 basis 스케일을 1 로 접고(translation 보존) 역바인드 재유도.
		for (FBone& Bone : Bones)
		{
			Bone.ReferenceGlobalPose   = StripMatrixBasisScale(Bone.ReferenceGlobalPose);
			Bone.SkinBindGlobalPose    = Bone.ReferenceGlobalPose;
			Bone.InverseBindPoseMatrix = Bone.ReferenceGlobalPose.GetInverse();
		}

		// 2) 단위 스케일 글로벌로부터 로컬 재유도(parent-first 전제: Global = Local * ParentGlobal).
		for (int32 i = 0; i < static_cast<int32>(Bones.size()); ++i)
		{
			FBone& Bone = Bones[i];
			const int32 P = Bone.ParentIndex;
			Bone.ReferenceLocalPose = (P >= 0 && P < i)
				? Bone.ReferenceGlobalPose * Bones[P].ReferenceGlobalPose.GetInverse()
				: Bone.ReferenceGlobalPose;

			// legacy 슬롯 동기화(저장/구경로 호환).
			Bone.LocalMatrix  = Bone.ReferenceLocalPose;
			Bone.GlobalMatrix = Bone.ReferenceGlobalPose;
		}
	}

	void CacheBounds()
	{
		bBoundsValid = false;
		if (Vertices.empty()) return;

		FVector LocalMin = Vertices[0].Position;
		FVector LocalMax = Vertices[0].Position;
		for (const FVertexPNCTBW& Vertex : Vertices)
		{
			LocalMin.X = std::min<float>(LocalMin.X, Vertex.Position.X);
			LocalMin.Y = std::min<float>(LocalMin.Y, Vertex.Position.Y);
			LocalMin.Z = std::min<float>(LocalMin.Z, Vertex.Position.Z);
			LocalMax.X = std::max<float>(LocalMax.X, Vertex.Position.X);
			LocalMax.Y = std::max<float>(LocalMax.Y, Vertex.Position.Y);
			LocalMax.Z = std::max<float>(LocalMax.Z, Vertex.Position.Z);
		}

		BoundsCenter = (LocalMin + LocalMax) * 0.5f;
		BoundsExtent = (LocalMax - LocalMin) * 0.5f;
		bBoundsValid = true;
	}
};
