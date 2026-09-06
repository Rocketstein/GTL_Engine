#pragma once

#include "Core/EngineTypes.h"
#include "Editor/UI/EditorPanel.h"
#include "Math/Transform.h"

class FSkeletalMeshViewer;
class USkeletalMesh;
class USkeletalMeshComponent;

class FEditorSkeletalMeshViewerPanel : public FEditorPanel
{
public:
    void Initialize(UEditorEngine* InEditorEngine, FSkeletalMeshViewer* InOwner)
    {
        FEditorPanel::Initialize(InEditorEngine);
        Owner = InOwner;
    }

    // FEditorPanel을(를) 통해 상속됨
    void Render(float DeltaTime);

    void Release();
	void RenderToolbar();
    void RenderPreviewViewport(float DeltaTime);
	void RenderBoneHierarchyTree();
    void RenderBoneNode(uint32 RootBone);
	void RenderInspector();
	USkeletalMeshComponent* GetPreviewMeshComponent();

	void SetSkelMesh();
    bool GetCachedBoneLocalTransform(int32 BoneIndex, FTransform& OutTransform);
    bool SetCachedBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform, bool bApplyToComponent = true);
    FQuat GetCachedBoneComponentRotation(int32 BoneIndex);
    FVector GetCachedBoneComponentScale(int32 BoneIndex);

private:
    void EnsureBoneLocalTransformCache(USkeletalMeshComponent* MeshComp);
    void RebuildBoneLocalTransformCache(USkeletalMeshComponent* MeshComp);
    FQuat GetCachedGlobalRotation(USkeletalMeshComponent* MeshComp, int32 BoneIndex) const;
    FVector GetCachedGlobalScale(USkeletalMeshComponent* MeshComp, int32 BoneIndex) const;

    FSkeletalMeshViewer* Owner = nullptr;

	TArray<FTransform> BoneLocalTransforms;// 헤더에 추가

	FColor SelectedColor = FColor(124, 252, 0);
	FColor BoneColor = FColor(0, 0, 55);
    FColor SelectedChildColor = FColor::White();
    FColor SelectedParentColor = FColor(255,127,80);
};
