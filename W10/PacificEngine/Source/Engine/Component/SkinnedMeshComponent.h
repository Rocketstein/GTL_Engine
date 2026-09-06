#pragma once

#include "Component/MeshComponent.h"
#include "Render/RHI/D3D11/Buffers/SkeletalMeshBuffer.h"

#include <memory>

class USkeletalMesh;
struct FBoneInfo;
class FGraphicsProgram;

enum class ESkeletalDebugPoseMode
{
    SkinBindPose,
    FbxLocalPose
};

class USkinnedMeshComponent : public UMeshComponent
{
public:
    struct FRuntimeSkinnedRenderBufferEntry
    {
        FRuntimeVertexElementRequestList RequestedElements;
        std::unique_ptr<FSkeletalMeshBuffer> RenderBuffer;
    };

    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)
    USkinnedMeshComponent() = default;
    ~USkinnedMeshComponent() override = default;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHit) override;

    FMeshDataView GetMeshDataView() const override;
    void SetSkeletalMesh(USkeletalMesh* InMesh);
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

    void RefreshReferencePose();
    void RefreshDisplayPose();
    void RefreshEditedDisplayPose();
    void RefreshEditedDisplayPoseFrom(int32 BoneIndex);
    void RefreshBoneTransforms();
    void RefreshBoneTransformsFrom(int32 BoneIndex);
    void UpdateSkinningMatrices();
    void UpdateSkinningMatricesFrom(int32 BoneIndex);
    void UpdateSkinnedVertices();
    void PruneUnusedSkinnedRuntimeRenderBuffers();  // Material 변경 등으로 사용되는 vertex layout 달라졌을 때 기존 버퍼 할당 해제

	void ResetToReferencePose();

	bool SetBoneLocalMatrix(int32 BoneIndex, const FMatrix& LocalMatrix);
    bool SetBoneLocalMatrices(const TArray<FMatrix>& LocalMatrices);
	const FMatrix& GetBoneLocalMatrix(int32 BoneIndex) const;
	const FMatrix& GetBoneComponentMatrix(int32 BoneIndex) const;
    FMatrix GetBoneDebugWorldMatrix(int32 BoneIndex) const;
    void SetSkeletalDebugPoseMode(ESkeletalDebugPoseMode InMode);
    ESkeletalDebugPoseMode GetSkeletalDebugPoseMode() const { return DebugPoseMode; }


    const TArray<FVertexPNCT_T>& GetSkinnedVertices() const;
    const TArray<uint32>& GetIndices() const;
    FSkeletalMeshBuffer* GetSkinnedRenderBuffer(int32 SubMeshIndex) const;
    FSkeletalMeshBuffer* GetSkinnedRenderBufferForShader(int32 SubMeshIndex, const FGraphicsProgram* InShader) const;

    int32 GetNumBones() const;
    const FBoneInfo* GetBoneInfo(int32 BoneIndex) const;
    FMatrix GetBoneWorldMatrix(int32 BoneIndex) const;
    int32 FindBoneIndex(const FName& BoneName) const;

protected:
    void CacheLocalBounds() override;

    USkeletalMesh* SkeletalMesh = nullptr;

	TArray<FMatrix> RefPoseBoneLocalMatrices; // reference pose에서 각 본의 Local transform을 저장하는 배열
    TArray<FMatrix> RefPoseBoneGlobalMatrices;
    TArray<FMatrix> DisplayPoseBoneLocalMatrices;
    TArray<FMatrix> DisplayPoseBoneGlobalMatrices;
    TArray<FMatrix> EditedDisplayPoseBoneGlobalMatrices;
    TArray<FMatrix> CurrentBoneLocalMatrices; // 현재 pose에서 각 본의 Local transform을 저장하는 배열
    TArray<FMatrix> CurrentBoneGlobalMatrices;

	TArray<FMatrix> SkinningMatrices;

    TArray<FVertexPNCT_T> SkinnedVertices;
    TArray<uint32> SkinnedIndices;
    TArray<std::unique_ptr<FSkeletalMeshBuffer>> SkinnedRenderBuffers;
    TArray<TArray<FRuntimeSkinnedRenderBufferEntry>> SkinnedRuntimeRenderBufferCaches;
    TArray<TArray<FRuntimeVertexElementRequestList>> DesiredSkinnedRuntimeBufferLayouts;

    FString SkeletalMeshPath = "None";
    ESkeletalDebugPoseMode DebugPoseMode = ESkeletalDebugPoseMode::SkinBindPose;
};
