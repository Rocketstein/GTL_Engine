#pragma once
#include "Viewport/IAssetViewer.h"
#include "Core/RayTypes.h"
#include "Preview/PreviewSceneContext.h"
#include "UI/EditorSkeletalMeshViewerPanel.h"
#include "Math/Transform.h"
#include "Viewport/SkeletalPreviewViewportClient.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include <d3d11.h>
#include <memory>

class AActor;
class UGizmoComponent;
class USkeletalMeshComponent;
class FSkelViewerViewportInputController;

struct FSkeletalMeshViewerState
{
    USkeletalMesh* ActiveMesh = nullptr;
    int32 SelectedBoneIndex = -1;
    TArray<TArray<uint32>> BoneHierarchy;
    EViewMode ViewMode;

    bool bShowMesh = true;
    bool bShowSkeleton = true;
    bool bUseFbxLocalSkeleton = false;

	void reset() {
        ActiveMesh			= nullptr;
		SelectedBoneIndex	= -1;
        BoneHierarchy.clear();

        bShowMesh		= true;
        bShowSkeleton	= true;
        bUseFbxLocalSkeleton = false;
	}
};

class FSkeletalMeshViewer : public IAssetViewer
{
public:
    FSkeletalMeshViewer();
    ~FSkeletalMeshViewer() override;

    void Initialize(
        uint32 InEditorId,
        UEditorEngine* InEditorEngine,
        ID3D11Device* InDevice,
        USkeletalMesh* InSkeletalMesh);

    void Release() override;
    void Tick(float DeltaTime) override;
    void Render(float DeltaTime) override;
    bool HandleInput(float DeltaTime);

    bool IsOpen() const override { return bOpen; }
    void SetOpen(bool bInOpen) override { bOpen = bInOpen; }

    uint32 GetViewerId() const override { return ViewerID; }
    EAssetViewerType GetViewerType() const override { return EAssetViewerType::SkeletalMesh; }
    bool GetViewportRect(FRect& OutRect) const override { return ViewportClient.GetViewportRect(OutRect); }
    FViewportClient* GetViewportClient() override { return &ViewportClient; }

	USkeletalMesh* GetSkeletalMesh();
    void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
    void FocusCameraOnPreviewMesh();
    void BuildBoneHierarchy();
    void ClearBoneSelection();
    void SelectBone(int32 BoneIndex);

    // Input Picking함수
    bool RaycastBonePicking(const FRay& Ray, int32& BestBoneIndex);
	//Gizmo Wrapper함수들
    bool GetCachedBoneLocalTransform(int32 BoneIndex, FTransform& OutTransform);
    bool SetCachedBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform, bool bApplyToComponent = true);
    FQuat GetCachedBoneComponentRotation(int32 BoneIndex);
    FVector GetCachedBoneComponentScale(int32 BoneIndex);

    FSkeletalMeshViewerState& GetState() { return ViewerState; }
    FPreviewSceneContext& GetPreviewScene() { return ViewerScene; };
    FSkeletalPreviewViewportClient& GetPreviewViewportClient() { return ViewportClient; }
    USkeletalMeshComponent* GetPreviewMeshComponent() const { return PreviewMeshComponent; }
    AActor* GetBoneGizmoTargetActor() const { return BoneGizmoTargetActor; }
    UGizmoComponent* GetBoneGizmo() const { return BoneGizmo; }

	void BeginInputFrame() override;
    bool InputKey(const FViewportKeyEvent& Event) override;
    bool InputAxis(const FViewportAxisEvent& Event) override;
    bool InputPointer(const FViewportPointerEvent& Event) override;
    void ResetInputState() override;
    void ResetKeyboardInputState() override;

private:
	//외부 Manager에서 확인하기위한 index
    uint32 ViewerID = 0;
    bool bOpen = false;
    FSkeletalMeshViewerState ViewerState;
    FEditorSkeletalMeshViewerPanel ViewerPanel;

	FSkeletalPreviewViewportClient ViewportClient;
    FPreviewSceneContext ViewerScene;

    std::unique_ptr<FSkelViewerViewportInputController> InputController;
    USkeletalMeshComponent* PreviewMeshComponent = nullptr;
    AActor* BoneGizmoTargetActor = nullptr;
    UGizmoComponent* BoneGizmo = nullptr;
};
