#pragma once

#include "Viewport/PreviewViewportClient.h"
#include "Math/Quat.h"

struct FPreviewViewportOptions
{
    bool bShowMesh = true;
    bool bShowSkeleton = true;
    int32 SelectedBoneIndex = 0;
};

class FSkeletalMeshViewer;
class UGizmoComponent;

class FSkeletalPreviewViewportClient : public FPreviewViewportClient
{
public:
    FSkeletalPreviewViewportClient();
    ~FSkeletalPreviewViewportClient() override;

    void Initialize(
        UEditorEngine* InEditorEngine,
        ID3D11Device* InDevice,
        FPreviewSceneContext* InPreviewContext,
        FSkeletalMeshViewer* InOwner);
    void Release() override;
    void Tick(float DeltaTime) override;

    void SetPreviewOptions(const FPreviewViewportOptions& InOptions) { Options = InOptions; }
    UGizmoComponent* GetGizmo() override;

    void BeginInputFrame() override;
    bool InputKey(const FViewportKeyEvent& Event) override;
    bool InputAxis(const FViewportAxisEvent& Event) override;
    bool InputPointer(const FViewportPointerEvent& Event) override;
    void ResetInputState() override;
    void ResetKeyboardInputState() override;

protected:
    EViewMode GetViewMode() const override;
    void BeforeCollect(FRenderCollectContext& CollectContext, FScene& Scene) override;
    void CollectPreviewWorld(FRenderer& Renderer, UWorld* World, FRenderCollectContext& CollectContext) override;
    void CollectPreviewOverlays(FRenderer& Renderer, FScene& Scene) override;

private:
	//다른 viewer도 쓰면 FPreviewViewportClient로 변경 할것
    void SyncBoneGizmoToSelection();
    void ApplyBoneGizmoToSelection();

    FSkeletalMeshViewer* OwnerViewer = nullptr;
    FPreviewViewportOptions Options;
    FQuat LastGizmoComponentRotation;
    bool bHasLastGizmoComponentRotation = false;
};
