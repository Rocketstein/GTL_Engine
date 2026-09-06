#include "Renderer/D3D11/D3D11RendererModule.h"
#include "Renderer/SceneView.h"
#include "Renderer/D3D11/D3D11DepthPass.h"
#include "Scene/Scene.h"

void FD3D11RendererModule::StartupModule(HWND hWnd)
{
    Device.Initialize(hWnd);
    DepthRenderer.Initialize(&Device);
    SceneRenderer.Initialize(&Device);
    GizmoRenderer.Initialize(&Device);

    if (Device.GetDevice() != nullptr)
    {
        Device.GetDevice()->QueryInterface(_uuidof(ID3D11Debug), reinterpret_cast<void **>(&Debug));
    }
}

void FD3D11RendererModule::ShutdownModule()
{
    Scene = nullptr;
    DepthRenderer.Shutdown();
    SceneRenderer.Shutdown();
    GizmoRenderer.Shutdown();
    Device.Shutdown();

    if (Debug != nullptr)
    {
        Debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        Debug->Release();
        Debug = nullptr;
    }
}

void FD3D11RendererModule::BeginFrame() { Device.BeginFrame(); }

void FD3D11RendererModule::EndFrame() { Device.EndFrame(); }

void FD3D11RendererModule::OnWindowResized(int32 InWidth, int32 InHeight)
{
    Device.Resize(InWidth, InHeight);
    DepthRenderer.OnWindowResized();
}

void FD3D11RendererModule::RenderFrame(const FSceneView *InSceneView)
{
    if (GpuCullingSettings.bEnableGpuHiZOcclusionCulling)
    {
        DepthRenderer.Render(Scene, InSceneView); // depth prepass
        DepthRenderer.BuildHZB();                      // build Hi-Z pyramid
        DepthRenderer.CullWithHZB(Scene, InSceneView); // cull
        SceneRenderer.Render(Scene, InSceneView, &DepthRenderer);
    }
    else
    {
        // GPU Hi-Z를 사용하지 않을 때는 depth prepass를 건너뛰고 단일 패스로 렌더링한다.
        // (SceneRenderer는 DepthPass==nullptr일 때 depth write 모드로 동작)
        DepthRenderer.SetAllVisible(Scene);
        SceneRenderer.Render(Scene, InSceneView, nullptr);
    }
    GizmoRenderer.Render(Scene, InSceneView);
}
