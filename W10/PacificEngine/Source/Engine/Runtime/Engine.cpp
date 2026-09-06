// 런타임 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Engine/Runtime/Engine.h"

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "Profiling/Stats.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Resource/FontManager.h"
#include "Resource/SpriteVfxManager.h"
#include "Resource/TextureManager.h"
#include "Component/CameraComponent.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Sound/SoundManager.h"
#include "Mesh/ObjManager.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Texture/Texture2D.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Engine/Classes/Camera/CameraManager.h"
#include "Core/TickFunction.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Serialization/SceneSaveManager.h"
#include "Materials/MaterialManager.h"
#include "SimpleJSON/json.hpp"
#include <fstream>
#include "Input/GameInput.h"

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;

namespace
{
ELevelTick ToLevelTickType(EWorldType WorldType)
{
    switch (WorldType)
    {
    case EWorldType::Editor:
        return ELevelTick::LEVELTICK_ViewportsOnly;
    case EWorldType::PIE:
    case EWorldType::Game:
        return ELevelTick::LEVELTICK_All;
    default:
        return ELevelTick::LEVELTICK_TimeOnly;
    }
}

APlayerCameraManager* FindPlayerCameraManagerInLevel(ULevel* Level)
{
    if (!Level)
    {
        return nullptr;
    }

    for (AActor* Actor : Level->GetActors())
    {
        if (APlayerCameraManager* CameraManager = Cast<APlayerCameraManager>(Actor))
        {
            return CameraManager;
        }
    }

    return nullptr;
}

AActor* FindCameraViewTarget(UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }

    if (UCameraComponent* ActiveCamera = World->GetActiveCamera())
    {
        return ActiveCamera->GetOwner();
    }

    if (UCameraComponent::Main && UCameraComponent::Main->GetWorld() == World)
    {
        return UCameraComponent::Main->GetOwner();
    }

    return nullptr;
}

bool CanAutoCreatePlayerCameraManager(const UWorld* World)
{
    if (!World)
    {
        return false;
    }

    const EWorldType WorldType = World->GetWorldType();
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void InitializePlayerCameraManagerIfNeeded(UWorld* World, APlayerCameraManager* CameraManager)
{
    if (!CameraManager || CameraManager->HasValidCameraCache())
    {
        return;
    }

    if (AActor* ViewTarget = FindCameraViewTarget(World))
    {
        CameraManager->InitializeFor(ViewTarget);
    }
}
} // namespace

void UEngine::Init(FWindowsWindow* InWindow)
{
    UE_LOG(Engine, Info, "Initializing runtime engine.");
    Window = InWindow;

    FNamePool::Get();
    FObjectFactory::Get();

    Renderer.Create(Window->GetHWND());
    UE_LOG(Engine, Debug, "Renderer created.");

	ViewportInputRouter.SetOwnerWindow(Window->GetHWND());

    uint32 InitWidth = static_cast<uint32>(Window->GetWidth());
    uint32 InitHeight = static_cast<uint32>(Window->GetHeight());

#if !WITH_EDITOR
    // Game.ini 로드 (해상도 설정 등 - Standalone 전용)
    std::wstring GameIniPath = FPaths::GameSettingsFilePath();
    if (std::filesystem::exists(GameIniPath))
    {
        std::ifstream File(GameIniPath);
        if (File.is_open())
        {
            std::string Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
            json::JSON Config = json::JSON::Load(Content);
            if (Config.hasKey("Resolution"))
            {
                InitWidth = (uint32)Config["Resolution"][0].ToInt();
                InitHeight = (uint32)Config["Resolution"][1].ToInt();
                UE_LOG(Engine, Info, "Setting resolution from Game.ini: %dx%d", InitWidth, InitHeight);
                Renderer.GetFD3DDevice().OnResizeViewport(InitWidth, InitHeight);
            }
        }
    }
#endif

    GameViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
    UE_LOG(Engine, Debug, "GameViewportClient created.");

#if !WITH_EDITOR
    // Standalone 용 뷰포트 인프라 구축
    FViewport* MainViewport = new FViewport();
    MainViewport->Initialize(Renderer.GetFD3DDevice().GetDevice(), InitWidth, InitHeight);
    MainViewport->SetClient(GameViewportClient);
    GameViewportClient->SetViewport(MainViewport);
    UE_LOG(Engine, Debug, "Main Viewport initialized for Standalone.");
#endif

    ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
    FMeshBufferManager::Get().Initialize(Device);

    // 에셋 자동 스캔 (스탠드얼론 대응)
    FObjManager::Get().SetDevice(Device);
    FSkeletalMeshManager::Get().SetDevice(Device);
    FMaterialManager::Get().SetDevice(Device);
    FObjManager::Get().ScanMeshCacheFiles();
    FSkeletalMeshManager::Get().ScanMeshCacheFiles();
    FObjManager::Get().ScanObjSourceFiles();
    FSkeletalMeshManager::Get().ScanFBXSourceFiles();
    FMaterialManager::Get().ScanMaterialAssets();
    UE_LOG(Engine, Info, "Asset registries scanned for runtime.");

    // 리소스 매니저 초기화 및 에셋 스캔
    FTextureManager::Get().SetDevice(Device);
    FTextureManager::Get().ScanAssets();

    FFontManager::Get().SetDevice(Device);
    FFontManager::Get().ScanAssets();
    FFontManager::Get().LoadGPUResources(Device);

    FSpriteVfxManager::Get().SetDevice(Device);
    FSpriteVfxManager::Get().ScanAssets();
    FSpriteVfxManager::Get().LoadGPUResources(Device);

    UE_LOG(Engine, Info, "Runtime engine initialization completed.");

    if (!FSoundManager::Get().Initialize())
    {
        UE_LOG(Engine, Warning, "Sound manager initialization failed. Continuing without audio.");
    }

	ScriptSystem.Initialize();
}

void UEngine::Shutdown()
{
    UE_LOG(Engine, Info, "Shutting down runtime engine.");

    if (GameViewportClient && GameViewportClient->GetViewport())
    {
        FViewport* VP = GameViewportClient->GetViewport();
        GameViewportClient->SetViewport(nullptr);
        delete VP;
    }

    FFontManager::Get().ReleaseGPUResources();
    FSpriteVfxManager::Get().ReleaseGPUResources();
    FTextureManager::Get().ReleaseGPUResources();
    UTexture2D::ReleaseAllGPU();
    FObjManager::Get().ReleaseAllGPU();
    FSkeletalMeshManager::Get().ReleaseAllGPU();
    FMeshBufferManager::Get().Release();
    Renderer.Release();

    FSoundManager::Get().Shutdown();

	ScriptSystem.Shutdown();
}

void UEngine::BeginPlay()
{
    // Startup Scene 로드
    std::wstring GameIniPath = FPaths::GameSettingsFilePath();
    FString StartupScenePath;
    if (std::filesystem::exists(GameIniPath))
    {
        std::ifstream File(GameIniPath);
        if (File.is_open())
        {
            std::string Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
            json::JSON Config = json::JSON::Load(Content);
            if (Config.hasKey("StartupScene"))
            {
                StartupScenePath = Config["StartupScene"].ToString();
            }
        }
    }

    if (!StartupScenePath.empty())
    {
        UE_LOG(Engine, Info, "Loading startup scene: %s", StartupScenePath.c_str());
        
        FWorldContext LoadCtx;
        FPerspectiveCameraData DummyCam;
        FSceneSaveManager::LoadSceneFromJSON(FPaths::ToUtf8(FPaths::ContentDir()) + "Scene/" + StartupScenePath, LoadCtx, DummyCam);
        
        if (LoadCtx.World)
        {
            LoadCtx.WorldType = EWorldType::Game;
            LoadCtx.World->SetWorldType(EWorldType::Game);
            WorldList.push_back(LoadCtx);
            SetActiveWorld(LoadCtx.ContextHandle);
        }
        else
        {
            UE_LOG(Engine, Error, "Failed to load startup scene. Creating default world.");
            FWorldContext& Context = CreateWorldContext(EWorldType::Game, FName("GameWorld"));
            SetActiveWorld(Context.ContextHandle);
        }
    }
    else
    {
        // 지정된 시작 씬이 없으면 빈 기본 월드 생성
        FWorldContext& Context = CreateWorldContext(EWorldType::Game, FName("GameWorld"));
        SetActiveWorld(Context.ContextHandle);
    }

    FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
    if (Context && Context->World)
    {
        if (Context->WorldType == EWorldType::Game || Context->WorldType == EWorldType::PIE)
        {
            Context->World->BeginPlay();
        }
    }
}

void UEngine::Tick(float DeltaTime)
{
    InputSystem::Get().Tick(Window->IsForeground());
    const FInputSnapshot& Snapshot = InputSystem::Get().GetSnapshot();

    ViewportInputRouter.ClearTargets();
    if (GameViewportClient && GameViewportClient->GetViewport())
    {
        FViewport* VP = GameViewportClient->GetViewport();

        // Standalone 모드에서는 메인 윈도우 전체를 타겟으로 등록
        ViewportInputRouter.RegisterTarget(
            VP,
            GameViewportClient,
            [this](FRect& OutRect)
            {
                OutRect = FRect{ 0.0f, 0.0f, static_cast<float>(Window->GetWidth()), static_cast<float>(Window->GetHeight()) };
                return true;
            });
        
        // 키보드 입력을 받을 기본 타겟으로 설정
        ViewportInputRouter.SetKeyTargetViewport(VP);

        GameViewportClient->BeginInputFrame();
    }

    ViewportInputRouter.Tick(Snapshot, DeltaTime);

    if (GameViewportClient)
    {
        GameViewportClient->Tick(DeltaTime);
    }

    WorldTick(DeltaTime);
    if (GameViewportClient)
    {
        GameViewportClient->UpdateCameraShakes(DeltaTime);
    }
    FSoundManager::Get().Tick(DeltaTime);
    Render(DeltaTime);
}

APlayerCameraManager* UEngine::ResolvePlayerCameraManager(UWorld* World)
{
    if (!World)
    {
        CachedPlayerCameraManagerUUID = 0;
        return nullptr;
    }

    if (CachedPlayerCameraManagerUUID != 0)
    {
        if (UObject* CachedObject = UObjectManager::Get().FindByUUID(CachedPlayerCameraManagerUUID))
        {
            if (APlayerCameraManager* CachedCameraManager = Cast<APlayerCameraManager>(CachedObject))
            {
                if (CachedCameraManager->GetWorld() == World)
                {
                    InitializePlayerCameraManagerIfNeeded(World, CachedCameraManager);
                    return CachedCameraManager;
                }
            }
        }

        CachedPlayerCameraManagerUUID = 0;
    }

    APlayerCameraManager* CameraManager = FindPlayerCameraManagerInLevel(World->GetActiveLevel());
    if (!CameraManager)
    {
        CameraManager = FindPlayerCameraManagerInLevel(World->GetPersistentLevel());
    }

    if (!CameraManager && CanAutoCreatePlayerCameraManager(World))
    {
        CameraManager = World->SpawnPersistentActor<APlayerCameraManager>();
        if (CameraManager)
        {
            InitializePlayerCameraManagerIfNeeded(World, CameraManager);
            UE_LOG(Engine, Info, "Auto-created PlayerCameraManager.");
        }
    }

    InitializePlayerCameraManagerIfNeeded(World, CameraManager);

    CachedPlayerCameraManagerUUID = CameraManager ? CameraManager->GetUUID() : 0;
    return CameraManager;
}

APlayerCameraManager* UEngine::GetPlayerCameraManager(UWorld* World)
{
    return ResolvePlayerCameraManager(World);
}

bool UEngine::TrySetSceneViewFromPlayerCameraManager(UWorld* World, FSceneView& OutSceneView)
{
    APlayerCameraManager* PlayerCameraManager = ResolvePlayerCameraManager(World);
    if (!PlayerCameraManager)
    {
        OutSceneView.PostProcessSettings = FPostProcessSettings{};
        return false;
    }

    OutSceneView.PostProcessSettings = PlayerCameraManager->GetPostProcessSettings();

    if (!PlayerCameraManager->HasValidCameraCache())
    {
        return false;
    }

    OutSceneView.SetCameraInfo(PlayerCameraManager->GetCameraCacheView());
    return true;
}

void UEngine::LogCameraManagerFallbackOnce()
{
    if (bLoggedCameraManagerFallback)
    {
        return;
    }

    UE_LOG(Engine, Warning, "CameraManager cache is invalid. Using fallback camera.");
    bLoggedCameraManagerFallback = true;
}

void UEngine::Render(float DeltaTime)
{
    SCOPE_STAT_CAT("UEngine::Render", "2_Render");

    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
    {
        static_cast<FShadowMapPass*>(Pass)->BeginShadowFrame();
    }

    RenderTargets.Reset();
    FViewport* Viewport = GameViewportClient ? GameViewportClient->GetViewport() : nullptr;
    ID3D11DeviceContext* DeviceContext = Renderer.GetFD3DDevice().GetDeviceContext();

    UWorld* World = GetWorld();
    UCameraComponent* FallbackCamera = World ? World->GetActiveCamera() : nullptr;

    // 활성 카메라가 없으면 전역 Main 카메라로 폴백합니다.
    if (!FallbackCamera)
    {
        FallbackCamera = UCameraComponent::Main;
    }

    FScene* Scene = nullptr;
    const bool bUsingCameraManager = TrySetSceneViewFromPlayerCameraManager(World, SceneView);
    if (!bUsingCameraManager && FallbackCamera)
    {
        LogCameraManagerFallbackOnce();
        SceneView.SetCameraInfo(FallbackCamera);
    }

    if ((bUsingCameraManager || FallbackCamera) && World)
    {
        FShowFlags ShowFlags;
        EViewMode ViewMode = EViewMode::Lit_Phong;

        SceneView.SetRenderSettings(ViewMode, ShowFlags);
        SceneView.RenderPath = ERenderShadingPath::Forward;

        if (Viewport && DeviceContext)
        {
            if (Viewport->ApplyPendingResize())
            {
                if (FallbackCamera)
                {
                    FallbackCamera->OnResize(static_cast<int32>(Viewport->GetWidth()), static_cast<int32>(Viewport->GetHeight()));
                }
            }

            // Viewport 기반 렌더링 환경 설정
            RenderTargets.SetFromViewport(Viewport);
            SceneView.SetViewportInfo(Viewport);
        }

        Scene = &World->GetScene();

        Renderer.BeginCollect(SceneView, Scene->GetPrimitiveProxyCount());

        FRenderCollectContext CollectContext = {};
        CollectContext.SceneView = &SceneView;
        CollectContext.Scene = Scene;
        CollectContext.Renderer = &Renderer;
        CollectContext.ViewModePassRegistry = Renderer.GetViewModePassRegistry();
        CollectContext.ActiveViewMode = SceneView.ViewMode;

        UWorld* CameraWorld = FallbackCamera ? FallbackCamera->GetWorld() : nullptr;
        Renderer.CollectWorld(bUsingCameraManager ? World : (CameraWorld ? CameraWorld : World), CollectContext);
        Renderer.CollectDebugRender(*Scene);
    }
    else
    {
        Renderer.BeginCollect(SceneView);
    }

    {
        FRenderPipelineContext PipelineContext = Renderer.CreatePipelineContext(SceneView, &RenderTargets, Scene);

        Renderer.BuildDrawCommands(PipelineContext);
        
        // RenderFrame을 통해 BeginFrame -> RunRootPipeline -> EndFrame(Present) 과정을 완결합니다.
        Renderer.RenderFrame(ERenderPipelineType::DefaultRootPipeline, PipelineContext);
    }

    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
    {
        static_cast<FShadowMapPass*>(Pass)->EndShadowFrame();
    }
}

void UEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    if (Width == 0 || Height == 0)
    {
        return;
    }

    UE_LOG(Engine, Debug, "Window resized to %ux%u.", Width, Height);
    Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);

    if (GameViewportClient && GameViewportClient->GetViewport())
    {
        GameViewportClient->GetViewport()->Resize(Width, Height);
    }
}

void UEngine::WorldTick(float DeltaTime)
{
    SCOPE_STAT_CAT("UEngine::WorldTick", "1_WorldTick");

    bool bHasPIEWorld = false;
    for (const FWorldContext& Ctx : WorldList)
    {
        if (Ctx.WorldType == EWorldType::PIE && Ctx.World)
        {
            bHasPIEWorld = true;
            break;
        }
    }

    for (FWorldContext& Ctx : WorldList)
    {
        UWorld* World = Ctx.World;
        if (!World)
            continue;

        if (bHasPIEWorld && Ctx.WorldType == EWorldType::Editor)
        {
            continue;
        }

        const ELevelTick TickType = ToLevelTickType(Ctx.WorldType);

        World->Tick(DeltaTime, TickType);
    }
}

UWorld* UEngine::GetWorld() const
{
    const FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
    return Context ? Context->World : nullptr;
}

FWorldContext& UEngine::CreateWorldContext(EWorldType Type, const FName& Handle, const FString& Name)
{
    FWorldContext Context;
    Context.WorldType = Type;
    Context.ContextHandle = Handle;
    Context.ContextName = Name.empty() ? Handle.ToString() : Name;
    Context.World = UObjectManager::Get().CreateObject<UWorld>();
    if (Context.World)
    {
        Context.World->SetWorldType(Type);
    }
    WorldList.push_back(Context);
    return WorldList.back();
}

void UEngine::DestroyWorldContext(const FName& Handle)
{
    for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
    {
        if (it->ContextHandle == Handle)
        {
            it->World->EndPlay();
            UObjectManager::Get().DestroyObject(it->World);
            WorldList.erase(it);
            return;
        }
    }
}

FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle)
{
    for (FWorldContext& Ctx : WorldList)
    {
        if (Ctx.ContextHandle == Handle)
        {
            return &Ctx;
        }
    }
    return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle) const
{
    for (const FWorldContext& Ctx : WorldList)
    {
        if (Ctx.ContextHandle == Handle)
        {
            return &Ctx;
        }
    }
    return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* World)
{
    for (FWorldContext& Ctx : WorldList)
    {
        if (Ctx.World == World)
        {
            return &Ctx;
        }
    }
    return nullptr;
}

void UEngine::SetActiveWorld(const FName& Handle)
{
    ActiveWorldHandle = Handle;
}
