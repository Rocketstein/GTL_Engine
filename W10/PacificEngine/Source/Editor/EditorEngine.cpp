// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/EditorEngine.h"

#include "Core/Logging/LogMacros.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "Materials/MaterialManager.h"
#include "Engine/Platform/Paths.h"
#include "Profiling/GPUProfiler.h"
#include "Profiling/Stats.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Execute/Context/RenderCollectContext.h"
#include "Sound/SoundManager.h"

#include <algorithm>

IMPLEMENT_CLASS(UEditorEngine, UEngine)

namespace
{
void ApplyViewportPostProcessOverrides(FPostProcessSettings& Settings,
                                      const FShowFlags& ShowFlags,
                                      const FViewportRenderOptions& Opts)
{
    Settings.Vignetting.bEnabled = ShowFlags.bVignetting;
    if (ShowFlags.bVignetting)
    {
        Settings.Vignetting.Intensity = Opts.Vignetting.Intensity;
        Settings.Vignetting.Radius    = Opts.Vignetting.Radius;
        Settings.Vignetting.Softness  = Opts.Vignetting.Softness;
        Settings.Vignetting.Color     = Opts.Vignetting.Color;
    }
    else
    {
        Settings.Vignetting = FVignettingSettings{};
    }

    Settings.GammaCorrection.bEnabled = ShowFlags.bGammaCorrection;
    if (ShowFlags.bGammaCorrection)
    {
        Settings.GammaCorrection.DisplayGamma = Opts.GammaCorrection.DisplayGamma;
    }
    else
    {
        Settings.GammaCorrection.bEnabled = false;
        Settings.GammaCorrection.DisplayGamma = 2.2f;
    }
}

void PreloadDefaultObjAssets(ID3D11Device* Device)
{
    if (!Device)
    {
        return;
    }

    const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::Get().GetAvailableObjFiles();
    for (const FMeshAssetListItem& Item : ObjFiles)
    {
        if (Item.FullPath.rfind(FPaths::ContentRelativePath("Models/_Basic/"), 0) != 0)
        {
            continue;
        }

        FObjManager::Get().Load(Item.FullPath);
    }
}

void WarmUpEditorViewModeShaders(FRenderer& Renderer)
{
    const EViewMode ViewModesToWarmUp[] = {
        EViewMode::Lit_Lambert,
        EViewMode::Lit_Phong,
        EViewMode::Unlit,
        EViewMode::WorldNormal,
        EViewMode::Wireframe,
        EViewMode::SceneDepth,
    };

    for (EViewMode ViewMode : ViewModesToWarmUp)
    {
        // Startup prewarm only targets the default forward path.
        // Render shaders are compiled on demand.
        Renderer.WarmUpViewModeShaders(ViewMode);
    }
}

} // namespace

void UEditorEngine::Init(FWindowsWindow* InWindow)
{
    UE_LOG(EditorEngine, Info, "Initializing editor engine.");
    UEngine::Init(InWindow);

	ViewportInputRouter.SetOwnerWindow(InWindow->GetHWND());

	FObjManager::Get().SetDevice(Renderer.GetFD3DDevice().GetDevice());
    FSkeletalMeshManager::Get().SetDevice(Renderer.GetFD3DDevice().GetDevice());
    
	FObjManager::Get().ScanMeshCacheFiles();
	FObjManager::Get().ScanObjSourceFiles();
    FSkeletalMeshManager::Get().ScanMeshCacheFiles();
    FSkeletalMeshManager::Get().ScanFBXSourceFiles();
	FMaterialManager::Get().ScanMaterialAssets();
	UE_LOG(EditorEngine, Debug, "Asset registries scanned.");
	FMaterialManager::Get().SetDevice(Renderer.GetFD3DDevice().GetDevice());
	PreloadDefaultObjAssets(Renderer.GetFD3DDevice().GetDevice());

	FObjManager::Get().ScanMeshCacheFiles();
    FSkeletalMeshManager::Get().ScanMeshCacheFiles();

    FMaterialManager::Get().ScanMaterialAssets();

    FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());
    WarmUpEditorViewModeShaders(Renderer);

    MainPanel.Create(Window, Renderer, this);
    UE_LOG(EditorEngine, Debug, "Editor main panel created.");

    // World
    if (WorldList.empty())
    {
        CreateWorldContext(EWorldType::Editor, FName("Default"));
        //CreateWorldContext(EWorldType::Preview, FName("Skeletal"));
    }
    SetActiveWorld(WorldList[0].ContextHandle);
    GetWorld()->InitWorld();

    // Selection & Gizmo
    SelectionManager.Init();
    SelectionManager.SetWorld(GetWorld());

	

    ViewportLayout.Initialize(this, Window, Renderer, &SelectionManager);
    ViewportLayout.LoadFromSettings();
    SetActiveViewport(ViewportLayout.GetActiveViewport());
    UE_LOG(EditorEngine, Info, "Editor engine initialized successfully.");
    AssetViewerManager.Initialize(this, Renderer.GetFD3DDevice().GetDevice());
}

void UEditorEngine::Shutdown()
{
    UE_LOG(EditorEngine, Info, "Shutting down editor engine.");
    ViewportLayout.SaveToSettings();
    FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
    CloseScene();
    SelectionManager.Shutdown();
    MainPanel.Release();
    GPUOcclusion.Release();

    ViewportLayout.Release();
    AssetViewerManager.Release();

    UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
}

void UEditorEngine::Tick(float DeltaTime)
{
    // PIE or Gameplay �߿��� ���̴� �� ���ε带 ��Ȱ��ȭ�Ͽ� �����͸� ����
    FShaderManager::SetHotReloadEnabled(!IsPlayingInEditor());
    if (!IsPlayingInEditor())
    {
        GetScriptSystem().TickEditor(DeltaTime);
    }

    if (bRequestEndPlayMapQueued)
    {
        bRequestEndPlayMapQueued = false;
        EndPlayMap();
    }
    if (PlaySessionRequest.has_value())
    {
        StartQueuedPlaySessionRequest();
    }

    MainPanel.Update();
    AssetViewerManager.Tick(DeltaTime);

    InputSystem::Get().Tick(Window->IsForeground());

    const FInputSnapshot& Input = InputSystem::Get().GetSnapshot();
    FInputSnapshot ViewportInput = Input;
    MainPanel.HandleShortcuts(Input, ViewportInput);

    const FGuiInputCaptureState& GuiCapture = MainPanel.GetGuiInputCaptureState();

	ViewportInputRouter.SetGuiCaptureState(GuiCapture);

	/*
     * note: RegisterViewportInputTargets���� ���Ǵ� ViewportClient�� Rect�� ���� �������� �ƴ� ���� �����ӿ��� ĳ�õ� ViewportScreenRect��
     *       ViewportScreenRect�� FLevelViewportLayout::RenderViewportUI ���� ImGui ���� �������� ������ �����ϱ� ������ �̷��� Ÿ�̹��� �Ұ�����
	 */

    RegisterViewportInputTargets();

    TArray<FViewportClient*> AllViewportClients;
    for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
    {
        if (VC)
        {
            AllViewportClients.push_back(VC);
        }
    }
    if (IsPlayingInEditor() && GameViewportClient)
    {
        AllViewportClients.push_back(GameViewportClient);
    }
    AssetViewerManager.ForEachOpenViewer(
        [&AllViewportClients](IAssetViewer& Viewer)
        {
            if (FViewportClient* Client = Viewer.GetViewportClient())
            {
                AllViewportClients.push_back(Client);
            }
        });

    for (FViewportClient* VC : AllViewportClients)
    {
        VC->BeginInputFrame();
    }

    ViewportInputRouter.Tick(ViewportInput, DeltaTime);
    ViewportLayout.SyncActiveViewportFromKeyTargetViewport(ViewportInputRouter.GetKeyTargetViewport());

    for (FViewportClient* VC : AllViewportClients)
    {
        VC->Tick(DeltaTime);
    }

    const bool bPIEPaused = IsPausedInEditor();
    const bool bHasPIEWorld = IsPlayingInEditor();
    for (FWorldContext& Ctx : WorldList)
    {
        UWorld* World = Ctx.World;
        if (!World)
        {
            continue;
        }

        if (bHasPIEWorld && Ctx.WorldType == EWorldType::Editor)
        {
            continue;
        }

        ELevelTick TickType = ELevelTick::LEVELTICK_TimeOnly;
        switch (Ctx.WorldType)
        {
        case EWorldType::Editor:
            TickType = ELevelTick::LEVELTICK_ViewportsOnly;
            break;
        case EWorldType::PIE:
        case EWorldType::Game:
            TickType = bPIEPaused ? ELevelTick::LEVELTICK_PauseTick : ELevelTick::LEVELTICK_All;
            break;
        default:
            TickType = ELevelTick::LEVELTICK_TimeOnly;
            break;
        }

        World->Tick(DeltaTime, TickType);
    }

    if (IsPlayingInEditor() && GameViewportClient)
    {
        GameViewportClient->UpdateCameraShakes(DeltaTime);
    }

    FSoundManager::Get().Tick(DeltaTime);

    Render(DeltaTime);
    SelectionManager.Tick();
}

UCameraComponent* UEditorEngine::GetCamera() const
{
    if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
    {
        return ActiveVC->GetCamera();
    }
    return nullptr;
}

void UEditorEngine::SetActiveViewport(FLevelEditorViewportClient* InClient)
{
    ViewportLayout.SetActiveViewport(InClient);
    ViewportInputRouter.SetKeyTargetViewport(InClient ? InClient->GetViewport() : nullptr);
}

void UEditorEngine::ResetViewportInputRouting()
{
    ViewportInputRouter.ResetRoutingState();

    if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
    {
        ViewportInputRouter.SetKeyTargetViewport(ActiveVC->GetViewport());
    }
}

void UEditorEngine::RenderUI(float DeltaTime)
{
    MainPanel.Render(DeltaTime);
}


void UEditorEngine::RequestPlaySession(const FRequestPlaySessionParams& InParams)
{
    PlaySessionRequest = InParams;
}

void UEditorEngine::CancelRequestPlaySession()
{
    PlaySessionRequest.reset();
}

void UEditorEngine::RequestEndPlayMap()
{
    if (!PlayInEditorSessionInfo.has_value())
    {
        return;
    }
    bRequestEndPlayMapQueued = true;
}

bool UEditorEngine::IsPausedInEditor() const
{
    return PlayInEditorSessionInfo.has_value() && PlayInEditorSessionInfo->bIsPaused;
}

void UEditorEngine::PausePlayInEditor()
{
    if (!PlayInEditorSessionInfo.has_value() || PlayInEditorSessionInfo->bIsPaused)
    {
        return;
    }

    PlayInEditorSessionInfo->bIsPaused = true;
    if (FLevelEditorViewportClient* PIEViewportClient = PlayInEditorSessionInfo->DestinationViewportClient)
    {
        PIEViewportClient->SetPlayState(EEditorViewportPlayState::Paused);
    }
}

void UEditorEngine::ResumePlayInEditor()
{
    if (!PlayInEditorSessionInfo.has_value() || !PlayInEditorSessionInfo->bIsPaused)
    {
        return;
    }

    PlayInEditorSessionInfo->bIsPaused = false;
    if (FLevelEditorViewportClient* PIEViewportClient = PlayInEditorSessionInfo->DestinationViewportClient)
    {
        PIEViewportClient->SetPlayState(EEditorViewportPlayState::Playing);
    }
}

void UEditorEngine::TogglePausePlayInEditor()
{
    if (!PlayInEditorSessionInfo.has_value())
    {
        return;
    }

    if (PlayInEditorSessionInfo->bIsPaused)
    {
        ResumePlayInEditor();
    }
    else
    {
        PausePlayInEditor();
    }
}

void UEditorEngine::StartQueuedPlaySessionRequest()
{
    if (!PlaySessionRequest.has_value())
    {
        return;
    }

    const FRequestPlaySessionParams Params = *PlaySessionRequest;
    PlaySessionRequest.reset();

    if (PlayInEditorSessionInfo.has_value())
    {
        EndPlayMap();
    }

    switch (Params.SessionDestination)
    {
    case EPIESessionDestination::InProcess:
        StartPlayInEditorSession(Params);
        break;
    }
}

void UEditorEngine::StartPlayInEditorSession(const FRequestPlaySessionParams& Params)
{
    UWorld* EditorWorld = GetWorld();
    if (!EditorWorld)
    {
        UE_LOG(EditorEngine, Error, "Failed to start PIE because editor world is null.");
        return;
    }
    UWorld* PIEWorld = Cast<UWorld>(EditorWorld->Duplicate(nullptr));
    if (!PIEWorld)
    {
        UE_LOG(EditorEngine, Error, "Failed to duplicate editor world for PIE.");
        return;
    }

    FWorldContext Ctx;
    Ctx.WorldType = EWorldType::PIE;
    Ctx.ContextHandle = FName("PIE");
    Ctx.ContextName = "PIE";
    Ctx.World = PIEWorld;
    if (PIEWorld)
    {
        PIEWorld->SetWorldType(EWorldType::PIE);
    }
    WorldList.push_back(Ctx);

    FLevelEditorViewportClient* PIEViewportClient = Params.DestinationViewportClient ? Params.DestinationViewportClient : ViewportLayout.GetActiveViewport();
    if (!PIEViewportClient)
    {
        UE_LOG(EditorEngine, Error, "Failed to start PIE because destination viewport is null.");
        return;
    }

    FPlayInEditorSessionInfo Info;
    Info.OriginalRequestParams = Params;
    Info.PIEStartTime = 0.0;
    Info.PreviousActiveWorldHandle = GetActiveWorldHandle();
    Info.DestinationViewportClient = PIEViewportClient;
    if (UCameraComponent* VCCamera = PIEViewportClient->GetCamera())
    {
        Info.SavedViewportCamera.Location = VCCamera->GetWorldLocation();
        Info.SavedViewportCamera.Rotation = VCCamera->GetRelativeRotation();
        Info.SavedViewportCamera.CameraState = VCCamera->GetCameraState();
        Info.SavedViewportCamera.bValid = true;
    }
    PlayInEditorSessionInfo = Info;

    for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
    {
        if (VC)
        {
            VC->SetPlayState(VC == PIEViewportClient ? EEditorViewportPlayState::Playing : EEditorViewportPlayState::Stopped);
        }
    }

    SetActiveWorld(FName("PIE"));

    if (GameViewportClient && PIEViewportClient && PIEViewportClient->GetViewport())
    {
        GameViewportClient->SetViewport(PIEViewportClient->GetViewport());
        GameViewportClient->SetFallbackCamera(PIEViewportClient->GetCamera());
        GameViewportClient->SetOverlayStatSystem(&GetOverlayStatSystem());
        ViewportInputRouter.SetKeyTargetViewport(PIEViewportClient->GetViewport());
        PIEViewportClient->GetViewport()->SetClient(GameViewportClient);
    }

    OnRenderSceneCleared();

    if (UCameraComponent* VCCamera = PIEViewportClient->GetCamera())
    {
        PIEWorld->SetActiveCamera(VCCamera);
    }

    SelectionManager.ClearSelection();
    SelectionManager.SetGizmoEnabled(false);
    SelectionManager.SetWorld(PIEWorld);

    ViewportLayout.DisableWorldAxisForPIE();

    PIEWorld->BeginPlay();
    UE_LOG(EditorEngine, Info, "Play In Editor session started.");
}

void UEditorEngine::EndPlayMap()
{
    if (!PlayInEditorSessionInfo.has_value())
    {
        return;
    }
    UE_LOG(EditorEngine, Info, "Ending Play In Editor session.");

    FSoundManager::Get().StopAll();

    if (GameViewportClient)
    {
        GameViewportClient->ResetCameraShakes();
    }

    const FName PrevHandle = PlayInEditorSessionInfo->PreviousActiveWorldHandle;
    SetActiveWorld(PrevHandle);

    //
    if (UWorld* EditorWorld = GetWorld())
    {
        EditorWorld->GetScene().MarkAllPerObjectCBDirty();

        FLevelEditorViewportClient* PIEViewportClient = PlayInEditorSessionInfo->DestinationViewportClient;
        if (PIEViewportClient && PIEViewportClient->GetCamera())
        {
            UCameraComponent* VCCamera = PIEViewportClient->GetCamera();
            if (PlayInEditorSessionInfo->SavedViewportCamera.bValid)
            {
                const FPIEViewportCameraSnapshot& SavedCamera = PlayInEditorSessionInfo->SavedViewportCamera;
                VCCamera->SetWorldLocation(SavedCamera.Location);
                VCCamera->SetRelativeRotation(SavedCamera.Rotation);
                VCCamera->SetCameraState(SavedCamera.CameraState);
            }

            EditorWorld->SetActiveCamera(VCCamera);
        }
        else if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
        {
            if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
            {
                EditorWorld->SetActiveCamera(VCCamera);
            }
        }
    }

    SelectionManager.ClearSelection();
    SelectionManager.SetGizmoEnabled(true);
    SelectionManager.SetWorld(GetWorld());

    // MainPanel.RestoreEditorWindowsAfterPIE();
    ViewportLayout.RestoreWorldAxisAfterPIE();

    DestroyWorldContext(FName("PIE"));

    OnRenderSceneCleared();

    for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
    {
        if (VC)
        {
            VC->SetPlayState(EEditorViewportPlayState::Stopped);
            if (VC->GetViewport() && VC->GetViewport()->GetClient() == GameViewportClient)
            {
                VC->GetViewport()->SetClient(VC);
            }
        }
    }

	GameViewportClient->SetViewport(nullptr);

    PlayInEditorSessionInfo.reset();

    UCameraComponent::Main = nullptr;
    GetOverlayStatSystem().ShowNoCameraWarning(false);
}


void UEditorEngine::ResetViewport()
{
    ViewportLayout.ResetViewport(GetWorld());
}

void UEditorEngine::CloseScene()
{
    ClearScene();
}

void UEditorEngine::NewScene()
{
    UE_LOG(EditorEngine, Info, "Creating a new editor scene.");
    StopPlayInEditorImmediate();
    ClearScene();
    FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
    Ctx.World->InitWorld();
    SetActiveWorld(Ctx.ContextHandle);
    SelectionManager.SetWorld(GetWorld());

    ResetViewport();
}

void UEditorEngine::OpenViewer()
{
    AssetViewerManager.OpenSkeletalMeshEditor(nullptr);
}

void UEditorEngine::ClearScene()
{
    UE_LOG(EditorEngine, Info, "Clearing scene and destroying world contexts.");
    StopPlayInEditorImmediate();
    SelectionManager.ClearSelection();
    SelectionManager.SetWorld(nullptr);

    OnRenderSceneCleared();

    for (FWorldContext& Ctx : WorldList)
    {
        Ctx.World->EndPlay();
        UObjectManager::Get().DestroyObject(Ctx.World);
    }

    WorldList.clear();
    ActiveWorldHandle = FName::None;

    UCameraComponent::Main = nullptr;
    ViewportLayout.DestroyAllCameras();
}


void UEditorEngine::OnRenderSceneCleared()
{
    GPUOcclusion.InvalidateResults();
}

void UEditorEngine::Render(float DeltaTime)
{
#if STATS
    FStatManager::Get().TakeSnapshot();
    FGPUProfiler::Get().TakeSnapshot();
    FGPUProfiler::Get().BeginFrame();
#endif

    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
    {
        static_cast<FShadowMapPass*>(Pass)->BeginShadowFrame();
    }

    for (FLevelEditorViewportClient* ViewportClient : GetLevelViewportClients())
    {
        SCOPE_STAT_CAT("RenderViewport", "2_Render");
        RenderViewport(ViewportClient);
    }

    Renderer.BeginFrame(SceneView);
    {
        SCOPE_STAT_CAT("EditorUI", "5_UI");
        RenderUI(DeltaTime);
    }

#if STATS
    FGPUProfiler::Get().EndFrame();
#endif

    {
        SCOPE_STAT_CAT("Present", "2_Render");
        Renderer.EndFrame();
    }

    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
    {
        static_cast<FShadowMapPass*>(Pass)->EndShadowFrame();
    }
}

void UEditorEngine::RenderViewport(FLevelEditorViewportClient* VC)
{
    FViewport* VP = VC->GetViewport();
    if (!VP)
        return;

    UCameraComponent* Camera = VP->GetClient() ? VP->GetClient()->GetCamera() : VC->GetCamera();

    ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
    if (!Ctx)
        return;

    UWorld* World = GetWorld();
    if (!World)
        return;

    const bool bUsePlayerCameraManager =
        IsPlayingInEditor() && VC->GetPlayState() != EEditorViewportPlayState::Stopped;
    const bool bUsingCameraManager =
        bUsePlayerCameraManager && TrySetSceneViewFromPlayerCameraManager(World, SceneView);

    if (!bUsingCameraManager && !Camera)
        return;

    if (!GPUOcclusion.IsInitialized())
        GPUOcclusion.Initialize(Renderer.GetFD3DDevice().GetDevice());

    GPUOcclusion.ReadbackResults(Ctx);

    const FViewportRenderOptions& Opts = VC->GetRenderOptions();
    FShowFlags EffectiveShowFlags = Opts.ShowFlags;
    if (VC->IsPilotingActor())
    {
        EffectiveShowFlags.bUUIDText = false;
        EffectiveShowFlags.bGrid = false;
        EffectiveShowFlags.bWorldAxis = false;
        EffectiveShowFlags.bSceneBVH = false;
        EffectiveShowFlags.bSceneOctree = false;
        EffectiveShowFlags.bWorldBound = false;
        EffectiveShowFlags.bLightDebugLines = false;
        EffectiveShowFlags.bLightHitMap = false;
    }
    const FShowFlags& ShowFlags = EffectiveShowFlags;
    EViewMode ViewMode = Opts.ViewMode;

    if (VP->ApplyPendingResize())
    {
        if (Camera)
        {
            Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
        }
    }

    VP->BeginRender(Ctx);

    RenderTargets.Reset();
    RenderTargets.SetFromViewport(VP);
    FScene& Scene = World->GetScene();
    Scene.ClearFrameData();

    if (!bUsingCameraManager)
    {
        if (bUsePlayerCameraManager)
        {
            LogCameraManagerFallbackOnce();
        }
        SceneView.SetCameraInfo(Camera);
        if (!bUsePlayerCameraManager)
        {
            SceneView.PostProcessSettings = FPostProcessSettings{};
        }
    }
    SceneView.SetRenderSettings(ViewMode, EffectiveShowFlags);
    ApplyViewportPostProcessOverrides(SceneView.PostProcessSettings, EffectiveShowFlags, Opts);
    SceneView.SetRenderOptions(Opts);
    SceneView.RenderPath = ERenderShadingPath::Forward;
    SceneView.SetViewportInfo(VP);
    SceneView.ViewportType = Opts.ViewportType;
    SceneView.OcclusionCulling = &GPUOcclusion;
    SceneView.LODContext = World->PrepareLODContext();

    Renderer.BeginCollect(SceneView, Scene.GetPrimitiveProxyCount());
    auto PipelineContext = Renderer.CreatePipelineContext(SceneView, &RenderTargets, &Scene);
    PipelineContext.ViewMode.ActiveViewMode = ViewMode;

    FRenderCollectContext CollectContext = {};
    CollectContext.SceneView = &SceneView;
    CollectContext.Scene = &Scene;
    CollectContext.Renderer = &Renderer;
    CollectContext.ViewModePassRegistry = Renderer.GetViewModePassRegistry();
    CollectContext.ActiveViewMode = ViewMode;
    CollectContext.CollectedPrimitives = const_cast<FCollectedPrimitives*>(&Renderer.GetCollectedPrimitives());

    {
        SCOPE_STAT_CAT("Collector", "3_Collect");

        const bool bIsEditorWorld = (World->GetWorldType() == EWorldType::Editor);

        Renderer.CollectWorld(World, CollectContext);
        Renderer.CollectGrid(Opts.GridSpacing, Opts.GridHalfLineCount, Scene);

        if (bIsEditorWorld)
        {
            Renderer.CollectDebugRender(Scene);

            if (ShowFlags.bSceneOctree)
            {
                Renderer.CollectOctreeDebug(World->GetOctree(), Scene);
            }

            if (ShowFlags.bSceneBVH)
            {
                World->BuildScenePrimitiveBVHNow();
                Renderer.CollectScenePrimitiveBVHDebug(World->GetScenePrimitiveBVH(), Scene);
            }

            if (ShowFlags.bWorldBound)
            {
                Renderer.CollectWorldBoundsDebug(Renderer.GetCollectedPrimitives().VisibleProxies, Scene);
            }

			if (ShowFlags.bSkeletalDebug)
            {
				Renderer.CollectSkeletalDebug(Scene);
			}
        }

        // Stat overlays are rendered by ImGui in FLevelViewportLayout.
    }

    {
        SCOPE_STAT_CAT("Renderer.Render", "4_ExecutePass");
        Renderer.BuildDrawCommands(PipelineContext);
        Renderer.RunRootPipeline(ERenderPipelineType::EditorRootPipeline, PipelineContext);
    }

    if (GPUOcclusion.IsInitialized())
    {
        SCOPE_STAT_CAT("GPUOcclusion", "4_ExecutePass");

        GPUOcclusion.DispatchOcclusionTest(
            Ctx,
            VP->GetDepthCopySRV(),
            Renderer.GetCollectedPrimitives().VisibleProxies,
            SceneView.View, SceneView.Proj,
            VP->GetWidth(), VP->GetHeight());
    }
}

void UEditorEngine::RegisterViewportInputTargets()
{
    ViewportInputRouter.ClearTargets();

    for (FLevelEditorViewportClient* VC: ViewportLayout.GetLevelViewportClients())
    {
        if (!VC || !VC->GetViewport())
        {
            continue;
        }

        FViewportClient* ClientToRegister = VC;
        if (IsPlayingInEditor() && VC->GetPlayState() == EEditorViewportPlayState::Playing && GameViewportClient)
        {
            ClientToRegister = GameViewportClient;
        }

        ViewportInputRouter.RegisterTarget(
            VC->GetViewport(),
            ClientToRegister,
            [VC](FRect& OutRect)
            {
                const FRect& Rect = VC->GetViewportScreenRect();
                if (Rect.Width <= 0.0f || Rect.Height <= 0.0f)
                {
                    return false;
                }

                OutRect = Rect;
                return true;
            });
    }

    AssetViewerManager.ForEachOpenViewer(
        [this](IAssetViewer& Viewer)
        {
            FViewportClient* Client = Viewer.GetViewportClient();
            if (!Client || !Client->GetViewport())
            {
                return;
            }

            IAssetViewer* ViewerPtr = &Viewer;
            ViewportInputRouter.RegisterTarget(
                Client->GetViewport(),
                Client,
                [ViewerPtr](FRect& OutRect)
                {
                    return ViewerPtr->GetViewportRect(OutRect);
                },
                true);
        });
}
