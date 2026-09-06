#pragma once

#include "Object/Object.h"
#include "Math/Transform.h"
#include "Math/Matrix.h"
#include "Serialization/Archive.h"

struct FBoneInfo
{
    FName Name;
    int32 ParentIndex = -1;
    FTransform ReferenceTransform; // Bind pose in local space

    friend FArchive& operator<<(FArchive& Ar, FBoneInfo& Bone)
    {
        Ar << Bone.Name << Bone.ParentIndex << Bone.ReferenceTransform;
        return Ar;
    }
};

class USkeleton : public UObject
{
public:
    DECLARE_CLASS(USkeleton, UObject)

    USkeleton() = default;
    
    void Serialize(FArchive& Ar) override;

    int32 AddBone(FName InName, int32 InParentIndex, const FTransform& InRefTransform);
    bool SetBoneReferenceTransform(int32 BoneIndex, const FTransform& InRefTransform);
    bool SetBoneDisplayTransform(int32 BoneIndex, const FTransform& InDisplayTransform);
    bool SetBoneReferenceMatrix(int32 BoneIndex, const FMatrix& InReferenceMatrix);
    bool SetBoneDisplayMatrix(int32 BoneIndex, const FMatrix& InDisplayMatrix);
    int32 FindBoneIndex(const FString& InName) const;
    
    const TArray<FBoneInfo>& GetBones() const { return Bones; }
    const TArray<FTransform>& GetDisplayTransforms() const { return DisplayTransforms; }
    const TArray<FMatrix>& GetReferenceLocalMatrices() const { return ReferenceLocalMatrices; }
    const TArray<FMatrix>& GetDisplayLocalMatrices() const { return DisplayLocalMatrices; }
    
private:
    TArray<FBoneInfo> Bones;
    TArray<FTransform> DisplayTransforms;
    TArray<FMatrix> ReferenceLocalMatrices;
    TArray<FMatrix> DisplayLocalMatrices;
};
