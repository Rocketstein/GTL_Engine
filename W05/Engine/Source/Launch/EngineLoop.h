#pragma once

#include "Core/Platform/PlatformTypes.h"

class FEngine;
class FViewport;
class FViewportClient;
class FDebugOverlayUI;
class FImGuiLayer;
class FWindow;
class FWindowsApplication;
class FInputState;
class FD3D11RendererModule;
class FScene;

class FEngineLoop
{
  public:
    FEngineLoop();
    ~FEngineLoop();

    int32 PreInit(const TCHAR *CmdLine);
    int32 Init();
    void  Exit();
    void  Tick();
    void  Run();

    void   SetDeltaTime(double InDeltaTime) { DeltaTime = InDeltaTime; }
    double GetDeltaTime() const { return DeltaTime; }

  private:
    bool                  bRunning = false;
    double                DeltaTime = 0.0;
    FWindowsApplication  *Application = nullptr;
    FWindow              *Window = nullptr;
    FInputState          *InputState = nullptr;
    FD3D11RendererModule *Renderer = nullptr;
    FEngine              *Engine = nullptr;
    FScene               *Scene = nullptr;
    FViewport            *Viewport = nullptr;
    FViewportClient      *ViewportClient = nullptr;
    FDebugOverlayUI      *DebugOverlayUI = nullptr;
    FImGuiLayer          *ImGuiLayer = nullptr;
};
