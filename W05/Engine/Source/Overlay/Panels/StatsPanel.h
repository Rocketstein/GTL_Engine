#pragma once

#include "Core/Platform/PlatformTypes.h"
#include <deque>

class FViewportClient;

class FStatsPanel
{
  public:
    enum class EStatMode : uint8
    {
        None,
        Fps,
        Memory,
        Timing,
        Culling,
        All,
    };

    void SetViewportClient(FViewportClient *InViewportClient) { ViewportClient = InViewportClient; }
    void SetStatMode(EStatMode InMode) { StatMode = InMode; }
    EStatMode GetStatMode() const { return StatMode; }
    void      SetRightPanelVisible(bool bInVisible) { bShowRightPanel = bInVisible; }
    bool      IsRightPanelVisible() const { return bShowRightPanel; }
    void      ToggleRightPanelVisible() { bShowRightPanel = !bShowRightPanel; }
    void      Render();

  private:
    void UpdateRecentFps(double LastFrameTimeMs);

  private:
    bool IsFpsEnabled() const { return StatMode == EStatMode::Fps || StatMode == EStatMode::All; }
    bool IsMemoryEnabled() const
    {
        return StatMode == EStatMode::Memory || StatMode == EStatMode::All;
    }
    bool IsTimingEnabled() const
    {
        return StatMode == EStatMode::Timing || StatMode == EStatMode::All;
    }
    bool IsCullingEnabled() const
    {
        return StatMode == EStatMode::Culling || StatMode == EStatMode::All;
    }
  private:
    FViewportClient *ViewportClient = nullptr;
    EStatMode        StatMode = EStatMode::Fps;
    bool             bShowRightPanel = true;
    std::deque<double> RecentFrameTimesMs;
    double             RecentFrameTimesTotalMs = 0.0;
};
