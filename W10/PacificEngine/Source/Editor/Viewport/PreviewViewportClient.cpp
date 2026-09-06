#include "Viewport/PreviewViewportClient.h"

#include "EditorEngine.h"
#include "Preview/PreviewSceneContext.h"
#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Renderer.h"
#include "Render/Scene/Scene.h"
#include "Settings/EditorSettings.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

FPreviewViewportClient::FPreviewViewportClient() = default;
FPreviewViewportClient::~FPreviewViewportClient() = default;

void FPreviewViewportClient::Initialize(
    UEditorEngine* InEditorEngine,
    ID3D11Device* InDevice,
    FPreviewSceneContext* InPreviewContext)
{
    Viewport = new FViewport();
    Viewport->Initialize(InDevice, static_cast<uint32>(ViewportWidth), static_cast<uint32>(ViewportHeight));
    Viewport->SetClient(this);

    EditorEngine = InEditorEngine;
    PreviewContext = InPreviewContext;
    ResetCamera();
}

void FPreviewViewportClient::Release()
{
    if (Viewport)
    {
        Viewport->Release();
        delete Viewport;
        Viewport = nullptr;
    }

    PreviewContext = nullptr;
    EditorEngine = nullptr;
}

void FPreviewViewportClient::Tick(float DeltaTime)
{
    (void)DeltaTime;
}

void FPreviewViewportClient::ResetCamera()
{
    OrbitPivot = FVector(0.0f, 0.0f, 0.0f);
    OrbitDistance = 8.0f;
    OrbitYaw = -135.0f;
    OrbitPitch = 23.0f;

    ApplyOrbitToCamera();
}

void FPreviewViewportClient::FocusOnBounds(const FBoundingBox& Bounds, float MaxDistance)
{
    if (!Bounds.IsValid())
    {
        ResetCamera();
        return;
    }

    OrbitPivot = Bounds.GetCenter();

    // 완전 정면이 아니라 약간 위 + 측면에서 보는 기본 각도
    OrbitYaw = 0.0f;
    OrbitPitch = 180.0f;

    const FVector Extent = Bounds.GetExtent();
    const float Radius = (std::max)(Extent.Length(), 0.5f);

    float DesiredDistance = Radius * 1.35f;

    if (UCameraComponent* Camera = GetCamera())
    {
        const float HalfFov = (std::max)(Camera->GetFOV() * 0.5f, 0.01f);

        DesiredDistance = Radius / (std::max)(std::tan(HalfFov), 0.01f);
        DesiredDistance *= 1.35f;
    }

    const float ClampedMaxDistance = MaxDistance > 0.0f ? MaxDistance : DesiredDistance;
    OrbitDistance = std::clamp(DesiredDistance, 1.0f, ClampedMaxDistance);

    ApplyOrbitToCamera();
}

void FPreviewViewportClient::ApplyOrbitToCamera()
{
    if (!PreviewContext)
    {
        return;
    }

    UCameraComponent* Camera = PreviewContext->GetCamera();
    if (!Camera)
    {
        return;
    }

    const FRotator ViewRotation(OrbitPitch, OrbitYaw, 0.0f);
    const FVector Forward = ViewRotation.GetForwardVector();
    const FVector ViewLocation = OrbitPivot - Forward * OrbitDistance;

    Camera->SetWorldLocation(ViewLocation);
    Camera->LookAt(OrbitPivot);
}

void FPreviewViewportClient::SetViewportRect(float X, float Y, float Width, float Height)
{
    ViewportX = X;
    ViewportY = Y;
    ViewportWidth = Width;
    ViewportHeight = Height;

    if (!Viewport)
    {
        return;
    }

    const uint32 W = static_cast<uint32>(Width > 1.0f ? Width : 1.0f);
    const uint32 H = static_cast<uint32>(Height > 1.0f ? Height : 1.0f);

    if (W != Viewport->GetWidth() || H != Viewport->GetHeight())
    {
        Viewport->RequestResize(W, H);
    }
}

bool FPreviewViewportClient::GetViewportRect(FRect& OutRect) const
{
    if (!Viewport || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        return false;
    }

    OutRect.X = ViewportX;
    OutRect.Y = ViewportY;
    OutRect.Width = ViewportWidth;
    OutRect.Height = ViewportHeight;
    return true;
}

UCameraComponent* FPreviewViewportClient::GetCamera() const
{
    return PreviewContext ? PreviewContext->GetCamera() : nullptr;
}

UGizmoComponent* FPreviewViewportClient::GetGizmo()
{
    return nullptr;
}

UWorld* FPreviewViewportClient::GetWorld() const
{
    return PreviewContext ? PreviewContext->GetWorld() : nullptr;
}

void FPreviewViewportClient::RenderViewportImage()
{
    if (!Viewport || !Viewport->GetSRV())
    {
        return;
    }

    if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        return;
    }

    ImVec2 Min(ViewportX, ViewportY);
    ImVec2 Max(ViewportX + ViewportWidth, ViewportY + ViewportHeight);

    ImGui::GetWindowDrawList()->AddImage(
        (ImTextureID)Viewport->GetSRV(),
        Min,
        Max);

    ImGui::SetCursorScreenPos(Min);
    ImGui::InvisibleButton(
        "##PreviewViewportInput",
        ImVec2(ViewportWidth, ViewportHeight),
        ImGuiButtonFlags_MouseButtonLeft |
        ImGuiButtonFlags_MouseButtonRight |
        ImGuiButtonFlags_MouseButtonMiddle);
}

void FPreviewViewportClient::Draw(FViewport* InViewport, float DeltaTime)
{
    (void)DeltaTime;

    if (!EditorEngine || !PreviewContext || !InViewport)
    {
        return;
    }

    UWorld* World = PreviewContext->GetWorld();
    UCameraComponent* Camera = PreviewContext->GetCamera();
    if (!World || !Camera)
    {
        return;
    }

    FRenderer& Renderer = EditorEngine->GetRenderer();
    ID3D11DeviceContext* Context = Renderer.GetFD3DDevice().GetDeviceContext();
    if (!Context)
    {
        return;
    }

    if (InViewport->ApplyPendingResize())
    {
        Camera->OnResize(static_cast<int32>(InViewport->GetWidth()), static_cast<int32>(InViewport->GetHeight()));
    }

    const float ClearColor[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
    InViewport->BeginRender(Context, ClearColor);

    FViewportRenderTargets RenderTargets = {};
    RenderTargets.Reset();
    RenderTargets.SetFromViewport(InViewport);

    FScene& Scene = World->GetScene();
    Scene.ClearFrameData();

    FShowFlags ShowFlags = {};
    ShowFlags.bGrid = true;
    ShowFlags.bGizmo = true;
    ShowFlags.bUUIDText = false;
    ShowFlags.bSceneBVH = false;
    ShowFlags.bSceneOctree = false;
    ShowFlags.bWorldBound = false;
    ShowFlags.bLightDebugLines = false;
    ShowFlags.bFog = false;
    ShowFlags.bVignetting = false;
    ShowFlags.bGammaCorrection = false;
    ShowFlags.bFXAA = true;

    const EViewMode ViewMode = GetViewMode();

    FSceneView PreviewSceneView = {};
    PreviewSceneView.SetCameraInfo(Camera);
    PreviewSceneView.SetRenderSettings(ViewMode, ShowFlags);
    PreviewSceneView.RenderPath = ERenderShadingPath::Forward;
    PreviewSceneView.SetViewportInfo(InViewport);
    PreviewSceneView.ViewportType = ELevelViewportType::Perspective;
    PreviewSceneView.LODContext = World->PrepareLODContext();

    Renderer.BeginCollect(PreviewSceneView, Scene.GetPrimitiveProxyCount());

    FRenderPipelineContext PipelineContext = Renderer.CreatePipelineContext(PreviewSceneView, &RenderTargets, &Scene);
    PipelineContext.ViewMode.ActiveViewMode = ViewMode;

    FRenderCollectContext CollectContext = {};
    CollectContext.SceneView = &PreviewSceneView;
    CollectContext.Scene = &Scene;
    CollectContext.Renderer = &Renderer;
    CollectContext.ViewModePassRegistry = Renderer.GetViewModePassRegistry();
    CollectContext.ActiveViewMode = ViewMode;
    CollectContext.CollectedPrimitives = const_cast<FCollectedPrimitives*>(&Renderer.GetCollectedPrimitives());

    BeforeCollect(CollectContext, Scene);
    CollectPreviewWorld(Renderer, World, CollectContext);
    CollectPreviewOverlays(Renderer, Scene);

	Renderer.CollectGrid(1.0f, 20, Scene);
    Renderer.CollectDebugRender(Scene);

    Renderer.BuildDrawCommands(PipelineContext);
    Renderer.RunRootPipeline(ERenderPipelineType::EditorRootPipeline, PipelineContext);

    ID3D11RenderTargetView* FrameRTV = Renderer.GetFD3DDevice().GetFrameBufferRTV();
    ID3D11DepthStencilView* FrameDSV = Renderer.GetFD3DDevice().GetDepthStencilView();
    Context->OMSetRenderTargets(1, &FrameRTV, FrameDSV);

    D3D11_VIEWPORT FrameViewport = Renderer.GetFD3DDevice().GetViewport();
    Context->RSSetViewports(1, &FrameViewport);
}

void FPreviewViewportClient::BeginInputFrame()
{
}

bool FPreviewViewportClient::InputKey(const FViewportKeyEvent& Event)
{
    (void)Event;
    return false;
}

bool FPreviewViewportClient::InputAxis(const FViewportAxisEvent& Event)
{
    (void)Event;
    return false;
}

bool FPreviewViewportClient::InputPointer(const FViewportPointerEvent& Event)
{
    (void)Event;
    return false;
}

void FPreviewViewportClient::ResetInputState()
{
}

void FPreviewViewportClient::ResetKeyboardInputState()
{
}

EViewMode FPreviewViewportClient::GetViewMode() const
{
    return EViewMode::Lit_Phong;
}

void FPreviewViewportClient::BeforeCollect(FRenderCollectContext& CollectContext, FScene& Scene)
{
    (void)CollectContext;
    (void)Scene;
}

void FPreviewViewportClient::CollectPreviewWorld(
    FRenderer& Renderer,
    UWorld* World,
    FRenderCollectContext& CollectContext)
{
    Renderer.CollectWorld(World, CollectContext);
}

void FPreviewViewportClient::CollectPreviewOverlays(FRenderer& Renderer, FScene& Scene)
{
    (void)Renderer;
    (void)Scene;
}
