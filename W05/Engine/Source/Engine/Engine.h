#pragma once

#include "Core/Platform/PlatformTypes.h"

class FScene;
class FViewport;
class FViewportClient;
class FDebugOverlayUI;

class FEngine
{
  public:
    FEngine() = default;
    ~FEngine() = default;

    bool Init();
    void Shutdown();
    void Tick(float DeltaTime);
    void Draw(FViewport &Viewport, FDebugOverlayUI &DebugOverlayUI, float Width, float Height);

    void    SetScene(FScene *InScene) { Scene = InScene; }
    FScene *GetScene() const { return Scene; }

  private:
    FScene *Scene = nullptr;
};
