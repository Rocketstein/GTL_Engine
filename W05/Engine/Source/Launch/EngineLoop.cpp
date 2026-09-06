#include "Launch/EngineLoop.h"
#include "ApplicationCore/InputState.h"
#include "ApplicationCore/Window.h"
#include "ApplicationCore/WindowsApplication.h"
#include "Core/CoreGlobals.h"
#include "Core/Misc/NameSubsystem.h"
#include "Core/Misc/Paths.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Overlay/DebugOverlayUI.h"
#include "Overlay/ImGuiLayer.h"
#include "Renderer/D3D11/D3D11RendererModule.h"
#include "Scene/Scene.h"
#include "Viewport/Viewport.h"
#include "Viewport/ViewportClient.h"
#include <windows.h>

FEngine              *GEngine = nullptr;
FD3D11RendererModule *GRenderer = nullptr;

FEngineLoop::FEngineLoop() = default;
FEngineLoop::~FEngineLoop() = default;

int32 FEngineLoop::PreInit(const TCHAR *) { return 0; }

int32 FEngineLoop::Init()
{
    Engine::Core::Misc::FNameSubsystem::Init();

    if (!FPaths::IsInitialized())
    {
        const std::filesystem::path ProjectRoot = std::filesystem::current_path();
        const std::filesystem::path EngineRoot = ProjectRoot / L"Engine";

        FPathConfig PathConfig{};
        PathConfig.EngineRoot = EngineRoot;
        PathConfig.AppRoot = EngineRoot;
        PathConfig.EngineContentDir = EngineRoot / L"Data";
        PathConfig.AppContentDir = EngineRoot / L"Data";
        PathConfig.SavedDir = ProjectRoot / L"Saved";
        PathConfig.ShaderDir = EngineRoot / L"Shaders";
        PathConfig.ShaderCacheDir = PathConfig.SavedDir / L"ShaderCache";

        if (!FPaths::Initialize(PathConfig))
        {
            return 1;
        }
        FPaths::EnsureRuntimeDirectories();
    }

    Application = new FWindowsApplication();
    Window = new FWindow();
    InputState = new FInputState();
    Renderer = new FD3D11RendererModule();
    Engine = new FEngine();
    Scene = new FScene();
    Viewport = new FViewport();
    ViewportClient = new FViewportClient();
    DebugOverlayUI = new FDebugOverlayUI();
    ImGuiLayer = new FImGuiLayer();

    GEngine = Engine;
    GRenderer = Renderer;

    Application->Initialize();
    if (!Window->Create(Application, L"AFOEngine", 1280, 720))
    {
        return 1;
    }
    Application->Initialize(Window, InputState);

    Renderer->StartupModule(Window->GetHwnd());
    Renderer->SetScene(Scene);

    if (!ImGuiLayer->Initialize(Window->GetHwnd(), Renderer->GetDevice().GetDevice(),
                                Renderer->GetDevice().GetDeviceContext()))
    {
        return 1;
    }

    Engine->Init();
    Engine->SetScene(Scene);
    Viewport->SetViewportClient(ViewportClient);
    ViewportClient->SetRenderer(Renderer);
    ViewportClient->SetScene(Scene);
    ViewportClient->SetOwnerWindow(Window->GetHwnd());
    DebugOverlayUI->SetEngine(Engine);
    DebugOverlayUI->SetViewportClient(ViewportClient);
    DebugOverlayUI->SetInputState(InputState);
    GLog = DebugOverlayUI;

    bRunning = true;
    return 0;
}

void FEngineLoop::Exit()
{
    bRunning = false;

    if (ImGuiLayer)
        ImGuiLayer->Shutdown();
    if (Renderer)
        Renderer->ShutdownModule();
    if (Window)
        Window->Destroy();
    if (Application)
        Application->Shutdown();
    if (Engine)
        Engine->Shutdown();

    delete ImGuiLayer;
    ImGuiLayer = nullptr;
    delete DebugOverlayUI;
    DebugOverlayUI = nullptr;
    GLog = nullptr;
    delete ViewportClient;
    ViewportClient = nullptr;
    delete Viewport;
    Viewport = nullptr;
    delete Scene;
    Scene = nullptr;
    delete Engine;
    Engine = nullptr;
    delete Renderer;
    Renderer = nullptr;
    delete InputState;
    InputState = nullptr;
    delete Window;
    Window = nullptr;
    delete Application;
    Application = nullptr;
    Engine::Core::Misc::FNameSubsystem::Shutdown();
    GEngine = nullptr;
    GRenderer = nullptr;
}

void FEngineLoop::Tick()
{
    if (!bRunning)
    {
        return;
    }

    if (!Application->PumpMessages())
    {
        bRunning = false;
        return;
    }

    static LARGE_INTEGER PrevCounter = {};
    static LARGE_INTEGER Frequency = {};
    if (Frequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&Frequency);
        QueryPerformanceCounter(&PrevCounter); // prime PrevCounter so first delta is ~0
        DeltaTime = 0.0;
        return;
    }

    LARGE_INTEGER CurrentCounter = {};
    QueryPerformanceCounter(&CurrentCounter);
    DeltaTime = static_cast<double>(CurrentCounter.QuadPart - PrevCounter.QuadPart) /
                static_cast<double>((Frequency.QuadPart > 0) ? Frequency.QuadPart : 1);
    PrevCounter = CurrentCounter;

    static int32 PrevWidth = -1;
    static int32 PrevHeight = -1;
    const int32  CurrentWidth = Window->GetClientWidth();
    const int32  CurrentHeight = Window->GetClientHeight();
    if (CurrentWidth > 0 && CurrentHeight > 0 &&
        (CurrentWidth != PrevWidth || CurrentHeight != PrevHeight))
    {
        Renderer->OnWindowResized(CurrentWidth, CurrentHeight);
        PrevWidth = CurrentWidth;
        PrevHeight = CurrentHeight;
    }

    InputState->BeginFrame();
    InputState->Update(*Window);
    ViewportClient->Tick(static_cast<float>(DeltaTime));

    Engine->Tick(static_cast<float>(DeltaTime));

    if (CurrentWidth <= 0 || CurrentHeight <= 0)
    {
        return;
    }

    Renderer->BeginFrame();
    ImGuiLayer->BeginFrame();
    Engine->Draw(*Viewport, *DebugOverlayUI, static_cast<float>(CurrentWidth),
                 static_cast<float>(CurrentHeight));
    ImGuiLayer->EndFrame();
    ImGuiLayer->Render();
    Renderer->EndFrame();
}

void FEngineLoop::Run()
{
    while (bRunning)
    {
        Tick();
    }
}
