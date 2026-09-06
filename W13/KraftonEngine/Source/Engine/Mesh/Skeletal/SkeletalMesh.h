#pragma once

#include "Object/Object.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Animation/Skeleton/SkeletonTypes.h"
#include "Object/Ptr/SoftObjectPtr.h"

class USkeleton;
class UPhysicsAsset;


#include "Source/Engine/Mesh/Skeletal/SkeletalMesh.generated.h"

UCLASS()
class USkeletalMesh : public UObject
{
public:
	GENERATED_BODY()
	USkeletalMesh() = default;
	~USkeletalMesh() override = default;

    void Serialize(FArchive& Ar) override;

    const FString& GetAssetPathFileName() const
    {
        return AssetPathFileName;
    }

    void SetAssetPathFileName(const FString& InPathFileName)
    {
        AssetPathFileName = InPathFileName;
    }

    void                             SetSkeletalMeshAsset(FSkeletalMesh* InMesh);
    FSkeletalMesh*                   GetSkeletalMeshAsset() const;
    void                             SetSkeletalMaterials(TArray<FSkeletalMaterial>&& InMaterials);
    const TArray<FSkeletalMaterial>& GetSkeletalMaterials() const;

    void InitResources(ID3D11Device* InDevice);

    void       SetSkeleton(USkeleton* InSkeleton);
    USkeleton* GetSkeleton() const;

    void SetSkeletonBinding(const FSkeletonBinding& InBinding);
    const FSkeletonBinding& GetSkeletonBinding() const { return SkeletonBinding; }

    void SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset);
    void SetPhysicsAssetPath(const FString& InPath);
    UPhysicsAsset* GetPhysicsAsset() const;
    const FString& GetPhysicsAssetPath() const { return PhysicsAssetPath.ToString(); }

private:
    void CacheSectionMaterialIndices();
    void SyncSkeletonBindingToAsset();
    void SyncSkeletonBindingFromAsset();

private:
    FString AssetPathFileName = "None";

    FSkeletalMesh*            SkeletalMeshAsset = nullptr;
    TArray<FSkeletalMaterial> SkeletalMaterials;

    FSkeletonBinding SkeletonBinding;
    USkeleton*       Skeleton = nullptr;
    // GetPhysicsAsset() 에서 경로로 lazy 해석해 채우므로 const 게터에서 갱신 가능하도록 mutable.
    mutable UPhysicsAsset* PhysicsAsset = nullptr;
    UPROPERTY(Edit, Save, Category="Physics", DisplayName="Physics Asset", AssetType="PhysicsAsset")
    FSoftObjectPtr   PhysicsAssetPath = "None";
};
