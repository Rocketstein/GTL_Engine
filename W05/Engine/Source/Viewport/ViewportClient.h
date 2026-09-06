#pragma once

#include "ApplicationCore/InputEvent.h"
#include "Core/Platform/PlatformTypes.h"
#include "Renderer/D3D11/D3D11SceneRenderer.h"
#include "Renderer/SceneView.h"
#include "ShowFlags.h"
#include "Viewport/ViewportCameraController.h"
#include "Viewport/ViewportCameraTransform.h"
#include <memory>
#include <windows.h>
#include "Picker.h"
#include "Viewport/Gizmo.h"

class FD3D11RendererModule;
class FScene;
class UPrimitiveComponent;

struct FViewportPerformanceStats
{
    double LastFrameTimeMs = 0.0;
    double LastPickTimeMs = 0.0;
    double TotalPickTimeMs = 0.0;
    uint64 TotalPickCount = 0;
    uint64 LastFrustumCullingTotalObjects = 0;
    uint64 LastFrustumCullingVisibleObjects = 0;
    uint64 LastFrustumCullingCulledObjects = 0;
    uint64 LastCpuOcclusionCullingTotalObjects = 0;
    uint64 LastCpuOcclusionCullingVisibleObjects = 0;
    uint64 LastCpuOcclusionCullingCulledObjects = 0;
    uint64 LastCpuNodeFrustumTested = 0;
    uint64 LastCpuNodeFrustumCulled = 0;
    uint64 LastCpuNodeFrustumTestedObjects = 0;
    uint64 LastCpuNodeFrustumCulledObjects = 0;
    uint64 LastCpuObjectFrustumInput = 0;
    uint64 LastCpuObjectFrustumCulled = 0;
    uint64 LastCpuNodeOcclusionTested = 0;
    uint64 LastCpuNodeOcclusionCulled = 0;
    uint64 LastCpuNodeOcclusionTestedObjects = 0;
    uint64 LastCpuNodeOcclusionCulledObjects = 0;
    uint64 LastCpuDistanceLodInput = 0;
    uint64 LastCpuDistanceLodCulled = 0;
    uint64 LastCpuOcclusionBufferWidth = 0;
    uint64 LastCpuOcclusionBufferHeight = 0;
    bool   bLastCpuOcclusionPassExecuted = false;
    bool   bLastCpuDistanceLodPassImplemented = false;
    uint64 LastGpuOcclusionCullingTotalObjects = 0;
    uint64 LastGpuOcclusionCullingVisibleObjects = 0;
    uint64 LastGpuOcclusionCullingCulledObjects = 0;
    uint64 LastGpuFrustumCullingTotalObjects = 0;
    uint64 LastGpuFrustumCullingVisibleObjects = 0;
    uint64 LastGpuFrustumCullingCulledObjects = 0;
    uint64 LastGpuInstanceCullingTotalObjects = 0;
    uint64 LastGpuInstanceCullingVisibleObjects = 0;
    uint64 LastGpuInstanceCullingCulledObjects = 0;
    uint64 LastGpuLodSelectionTotalObjects = 0;
    uint64 LastGpuLodSelectionCulledObjects = 0;
    bool   bLastGpuFrustumPassImplemented = false;
    bool   bLastGpuInstancePassImplemented = false;
    bool   bLastGpuLodPassImplemented = false;
    bool   bLastGpuHiZOcclusionPassExecuted = false;
};

struct FCullingControlSettings
{
    bool bCpuNodeFrustumCulling = false;
    bool bCpuObjectFrustumCulling = false;
    bool bCpuNodeOcclusionCulling = false;
    bool bCpuObjectOcclusionCulling = false;
    bool bCpuDistanceLodCulling = false; // TODO: not implemented yet
    int  CpuNodeFrustumMinChildren = 3;
    int  CpuNodeOcclusionMinChildren = 3;
    float CpuOcclusionTestBias = 0.0002f;
    float CpuOcclusionBiasPerScreenRadiusNdc = 0.001f;
    float CpuOcclusionMaxDynamicBias = 0.001f;
    float CpuNearOcclusionSkipDistance = 0.0f;
    float CpuMinOccluderScreenCoverage = 0.0f;
    float CpuMinOccluderExtent = 0.0f;
    float CpuMaxNodeOcclusionScreenCoverage = 0.5f;
    float CpuOcclusionBoundsExtentScale = 0.85f;
    // 0 means "unlimited". A fixed small cap made far-view occluders never contribute.
    float CpuMaxOccluderDistance = 0.0f;

    bool bGpuFrustumCulling = false; // TODO: not implemented yet
    bool bGpuHiZOcclusionCulling = false;
    bool bGpuInstanceCulling = false; // TODO: not implemented yet
    bool bGpuLodSelection = false;    // TODO: not implemented yet
};

class FViewportClient
{
  public:
    FViewportClient();
    ~FViewportClient();

    void Tick(float DeltaTime);
    void Draw(float Width, float Height);
    void ResetInputState();

    bool                InputKey(EKey Key, EInputEvent Event);
    bool                InputAxis(EKey Key, float Delta);
    bool                MouseMove(int32 X, int32 Y);
    bool                CapturedMouseMove(int32 DeltaX, int32 DeltaY);
    void                ProcessClick(int32 X, int32 Y);
    EPointerReleaseType GetReleaseType(int32 X, int32 Y) const;

    void            SetRenderer(FD3D11RendererModule *InRenderer) { Renderer = InRenderer; }
    void SetScene(FScene *InScene)
    {
        Scene = InScene;
        Picker->SetScene(InScene);
    }
    void            SetOwnerWindow(HWND InOwnerWindow) { OwnerWindow = InOwnerWindow; }
    void            SetShowGrid(bool bEnabled);
    bool            IsShowGrid() const;
    void            SetGridSpacing(float InSpacing);
    float           GetGridSpacing() const;
    void            SetShowWorldAxis(bool bEnabled);
    bool            IsShowWorldAxis() const;
    void            SetShowGizmo(bool bEnabled);
    bool            IsShowGizmo() const;
    void            SetShowSelectedAABB(bool bEnabled);
    bool            IsShowSelectedAABB() const;
    void            SetGridRenderMode(EGridRenderMode) { GridRenderMode = EGridRenderMode::CpuLineBatch; }
    EGridRenderMode GetGridRenderMode() const { return EGridRenderMode::CpuLineBatch; }
    void            SetCameraLocation(const FVector3 &InLocation)
    {
        FViewportCameraTransform T = CameraController.GetTransform();
        T.SetLocation(InLocation);
        CameraController.SetTransform(T);
    }
    void SetCameraRotation(const FRotator &InRotation)
    {
        FViewportCameraTransform T = CameraController.GetTransform();
        T.SetRotation(InRotation);
        CameraController.SetTransform(T);
    }
    void       SetPerspectiveCameraParams(float InFovDegrees, float InNearPlane, float InFarPlane);
    FVector3   GetCameraLocation() const { return CameraController.GetTransform().GetLocation(); }
    FRotator   GetCameraRotation() const { return CameraController.GetTransform().GetRotation(); }
    float      GetCameraFovDegrees() const { return CameraState.VerticalFovDegrees; }
    float      GetCameraNearPlane() const { return CameraState.NearPlane; }
    float      GetCameraFarPlane() const { return CameraState.FarPlane; }
    UPrimitiveComponent *GetSelectedPrimitive() const
    {
        return (Gizmo != nullptr) ? Gizmo->GetTarget() : nullptr;
    }

    void SetShowPrimitives(bool bEnabled);
    bool IsShowPrimitives() const;
    void SetShowLines(bool bEnabled);
    bool IsShowLines() const;
    EShowFlags GetShowFlags() const { return ShowFlags; }

    const FViewportPerformanceStats &GetPerformanceStats() const { return PerformanceStats; }
    FSceneView                      &GetSceneView() { return SceneView; }
    const FCullingControlSettings   &GetCullingControlSettings() const { return CullingControlSettings; }
    void SetCullingControlSettings(const FCullingControlSettings &InSettings)
    {
        CullingControlSettings = InSettings;
    }

    void FlushGizmo();
    void SetGizmoHoveringEnabled(bool bEnabled);
    bool IsGizmoHoveringEnabled() const;
    void SetGizmoMaxScreenSpaceScale(float InMaxScale);
    float GetGizmoMaxScreenSpaceScale() const;
  private:
    FSceneView BuildSceneView(float Width, float Height) const;

  private:
    FD3D11RendererModule     *Renderer = nullptr;
    FScene                   *Scene = nullptr;
    HWND                      OwnerWindow = nullptr;
    FViewportCameraController CameraController;
    FCameraInput              CurrentCameraInput;
    FCameraViewState          CameraState;
    FSceneView                SceneView;
    FViewportPerformanceStats PerformanceStats;
    FCullingControlSettings   CullingControlSettings;
    UGizmo                   *Gizmo = nullptr;
    FPicker                  *Picker = nullptr;


    bool       bSpacePrevDown = false;
    bool       bLeftMouseDown = false;
    bool       bGizmoPickMeshUploaded = false;
    int32      MouseDownX = 0;
    int32      MouseDownY = 0;
    int32      LastMouseX = 0;
    int32      LastMouseY = 0;
    int32      LastHoverMouseX = INT_MIN; // sentinel: force raycast on first frame
    int32      LastHoverMouseY = INT_MIN;
    float      PendingWheelDelta = 0.0f;
    bool       bRightMouseDown = false;
    bool       bMiddleMouseDown = false;
    EShowFlags ShowFlags = static_cast<EShowFlags>(
        static_cast<uint32>(EShowFlags::SF_Primitives) | static_cast<uint32>(EShowFlags::SF_Lines) |
        static_cast<uint32>(EShowFlags::SF_Grid) | static_cast<uint32>(EShowFlags::SF_WorldAxis) |
        static_cast<uint32>(EShowFlags::SF_Gizmo));
    EGridRenderMode GridRenderMode = EGridRenderMode::CpuLineBatch;
};
