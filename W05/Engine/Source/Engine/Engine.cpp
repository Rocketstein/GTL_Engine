#include "Engine/Engine.h"
#include "Overlay/DebugOverlayUI.h"
#include "Viewport/Viewport.h"
#include "Viewport/ViewportClient.h"

bool FEngine::Init() { return true; }

void FEngine::Shutdown() {}

void FEngine::Tick(float DeltaTime) { (void)DeltaTime; }

void FEngine::Draw(FViewport &Viewport, FDebugOverlayUI &DebugOverlayUI, float Width, float Height)
{
    Viewport.GetViewportClient()->Draw(Width, Height);
    DebugOverlayUI.Render();
}
