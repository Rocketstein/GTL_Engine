#pragma once
#include "Core/EngineTypes.h"
#include <Core/RayTypes.h>

class FPreviewViewportClient;
class FSkeletalMeshViewer;
class FSkelViewerViewportInputController;

const int32 INDEX_NONE = -1;

class FPreviewViewportTool
{
public:
    FPreviewViewportTool(FPreviewViewportClient* InOwner, FSkeletalMeshViewer* InViewer, FSkelViewerViewportInputController* InController)
        : Owner(InOwner), Viewer(InViewer), Controller(InController)
    {
    }

    virtual ~FPreviewViewportTool() = default;

    virtual bool HandleInput(float DeltaTime) = 0;
    virtual void ResetState() {}

protected:
    bool BuildMouseRay(FRay& OutRay) const;

    FPreviewViewportClient* Owner = nullptr;
    FSkeletalMeshViewer* Viewer = nullptr;
    FSkelViewerViewportInputController* Controller = nullptr;
};

class FSkeletalMeshGizmoTool : public FPreviewViewportTool
{
public:
    using FPreviewViewportTool::FPreviewViewportTool;

    bool HandleInput(float DeltaTime) override;
    void ResetState() override;

private:
    bool HandleDragStart(const FRay& Ray);
    bool HandleDragUpdate(const FRay& Ray);
    bool HandleDragEnd();
};

class FSkeletalMeshViewerBoneSelectTool : public FPreviewViewportTool
{
public:
    using FPreviewViewportTool::FPreviewViewportTool;

    virtual bool HandleInput(float DeltaTime) override;
private:
    bool HandleBonePicking(const FRay& Ray);
};

class FSkeletalMeshNavigationTool : public FPreviewViewportTool
{
public:
    using FPreviewViewportTool::FPreviewViewportTool;

    virtual bool HandleInput(float DeltaTime) override;

private:
    bool HandleKeyboardAndMouseNavigation(float DeltaTime);
    bool HandleWheelZoom(float DeltaTime);

	float CameraMoveSensitivity = 0.2f;
    float CameraRotateSensitivity = 2.0f;
    float CameraZoomSpeed = 2.0f;
    float CameraMoveSpeed = 10.0f;
    float CameraRotationSpeed = 2.0f;
};

class FSkeletalMeshCommandTool : public FPreviewViewportTool
{
public:
    using FPreviewViewportTool::FPreviewViewportTool;

    bool HandleInput(float DeltaTime) override;
};
