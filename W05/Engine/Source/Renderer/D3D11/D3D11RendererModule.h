#pragma once

#include "Renderer/D3D11/D3D11Device.h"
#include "Renderer/D3D11/D3D11DepthPass.h"
#include "Renderer/D3D11/D3D11SceneRenderer.h"
#include "Renderer/D3D11/D3D11GizmoRenderer.h"
#include <Windows.h>

class FScene;
class FSceneView;

struct FGpuCullingSettings
{
    bool bEnableGpuFrustumCulling = true; // TODO: not implemented yet
    bool bEnableGpuHiZOcclusionCulling = true;
    bool bEnableGpuInstanceCulling = true; // TODO: not implemented yet
    bool bEnableGpuLodSelection = true;    // TODO: not implemented yet
};

class FD3D11RendererModule
{
  public:
    void StartupModule(HWND hWnd);
    void ShutdownModule();

    void BeginFrame();
    void EndFrame();
    void OnWindowResized(int32 InWidth, int32 InHeight);

    void    SetScene(FScene *InScene) { Scene = InScene; }
    FScene *GetScene() const { return Scene; }

    void SetShowGrid(bool bEnabled) { SceneRenderer.SetShowGrid(bEnabled); }
    void SetShowWorldAxis(bool bEnabled) { SceneRenderer.SetShowWorldAxis(bEnabled); }
    void SetShowSelectedAABB(bool bEnabled) { SceneRenderer.SetShowSelectedAABB(bEnabled); }
    void SetGridSpacing(float InSpacing) { SceneRenderer.SetGridSpacing(InSpacing); }
    void SetGridRenderMode(EGridRenderMode InMode) { SceneRenderer.SetGridRenderMode(InMode); }
    void SetGpuCullingSettings(const FGpuCullingSettings &InSettings)
    {
        GpuCullingSettings = InSettings;
    }
    const FGpuCullingSettings &GetGpuCullingSettings() const { return GpuCullingSettings; }

    void RenderFrame(const FSceneView *InSceneView);

    FD3D11Device        &GetDevice() { return Device; }
    FD3D11DepthPass     &GetDepthPassRenderer() { return DepthRenderer; }
    FD3D11SceneRenderer &GetSceneRenderer() { return SceneRenderer; }
    FD3D11GizmoRenderer &GetGizmoRenderer() { return GizmoRenderer; }
    FDepthCullStats      GetLastDepthCullStats() const { return DepthRenderer.GetLastCullStats(); }

  private:
    FD3D11Device        Device;
    FD3D11DepthPass     DepthRenderer;
    FD3D11SceneRenderer SceneRenderer;
    FD3D11GizmoRenderer GizmoRenderer;
    FGpuCullingSettings GpuCullingSettings;
    FScene             *Scene = nullptr;
    ID3D11Debug        *Debug = nullptr;
};
