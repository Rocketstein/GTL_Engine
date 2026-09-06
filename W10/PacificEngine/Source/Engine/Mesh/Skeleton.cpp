#include "Skeleton.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(USkeleton, UObject)

namespace
{
    void SerializeMatrix(FArchive& Ar, FMatrix& Matrix)
    {
        for (float& Value : Matrix.Data)
        {
            Ar << Value;
        }
    }

    void SerializeMatrixArray(FArchive& Ar, TArray<FMatrix>& Matrices)
    {
        uint32 ArrayNum = static_cast<uint32>(Matrices.size());
        Ar << ArrayNum;

        if (Ar.IsLoading())
        {
            Matrices.resize(ArrayNum);
        }

        for (FMatrix& Matrix : Matrices)
        {
            SerializeMatrix(Ar, Matrix);
        }
    }

    void EnsurePoseMatrixFallbacks(TArray<FMatrix>& Matrices, const TArray<FBoneInfo>& Bones)
    {
        if (Matrices.size() == Bones.size())
        {
            return;
        }

        Matrices.clear();
        Matrices.reserve(Bones.size());
        for (const FBoneInfo& Bone : Bones)
        {
            Matrices.push_back(Bone.ReferenceTransform.ToMatrix());
        }
    }
}

void USkeleton::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    // FBoneInfo contains FName, so serialize each element through FBoneInfo::operator<<.
    uint32 ArrayNum = static_cast<uint32>(Bones.size());
    Ar << ArrayNum;

    if (Ar.IsLoading())
    {
        Bones.resize(ArrayNum);
    }

    for (auto& Bone : Bones)
    {
        Ar << Bone;
    }

    SerializeMatrixArray(Ar, ReferenceLocalMatrices);
    SerializeMatrixArray(Ar, DisplayLocalMatrices);

    if (Ar.IsLoading())
    {
        EnsurePoseMatrixFallbacks(ReferenceLocalMatrices, Bones);
        EnsurePoseMatrixFallbacks(DisplayLocalMatrices, Bones);
    }
}

int32 USkeleton::AddBone(FName InName, int32 InParentIndex, const FTransform& InRefTransform)
{
    FBoneInfo Bone;
    Bone.Name = InName;
    Bone.ParentIndex = InParentIndex;
    Bone.ReferenceTransform = InRefTransform;
    Bones.push_back(Bone);
    ReferenceLocalMatrices.push_back(InRefTransform.ToMatrix());
    DisplayLocalMatrices.push_back(InRefTransform.ToMatrix());
    
    return static_cast<int32>(Bones.size() - 1);
}

bool USkeleton::SetBoneReferenceTransform(int32 BoneIndex, const FTransform& InRefTransform)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return false;
    }

    Bones[BoneIndex].ReferenceTransform = InRefTransform;
    if (BoneIndex < static_cast<int32>(ReferenceLocalMatrices.size()))
    {
        ReferenceLocalMatrices[BoneIndex] = InRefTransform.ToMatrix();
    }
    return true;
}

bool USkeleton::SetBoneReferenceMatrix(int32 BoneIndex, const FMatrix& InReferenceMatrix)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return false;
    }

    if (ReferenceLocalMatrices.size() != Bones.size())
    {
        ReferenceLocalMatrices.resize(Bones.size(), FMatrix::Identity);
    }

    ReferenceLocalMatrices[BoneIndex] = InReferenceMatrix;
    return true;
}

bool USkeleton::SetBoneDisplayMatrix(int32 BoneIndex, const FMatrix& InDisplayMatrix)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return false;
    }

    if (DisplayLocalMatrices.size() != Bones.size())
    {
        DisplayLocalMatrices.resize(Bones.size(), FMatrix::Identity);
    }

    DisplayLocalMatrices[BoneIndex] = InDisplayMatrix;
    return true;
}

int32 USkeleton::FindBoneIndex(const FString& InName) const
{
    for (int32 i = 0; i < (int32)Bones.size(); ++i)
    {
        if (Bones[i].Name == InName)
        {
            return i;
        }
    }
    return -1;
}
