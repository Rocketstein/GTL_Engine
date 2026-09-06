#pragma once

#include "Core/Platform/PlatformTypes.h"

class FViewportClient;

class FStatsOverlay
{
  public:
    enum class EStatMode : uint8
    {
        None,
        Fps,
        Memory,
        All,
    };

    void SetViewportClient(FViewportClient *InViewportClient) { ViewportClient = InViewportClient; }
    void SetStatMode(EStatMode InMode) { StatMode = InMode; }
    EStatMode GetStatMode() const { return StatMode; }
    void      Render();

  private:
    bool IsFpsEnabled() const { return StatMode == EStatMode::Fps || StatMode == EStatMode::All; }
    bool IsMemoryEnabled() const
    {
        return StatMode == EStatMode::Memory || StatMode == EStatMode::All;
    }

  private:
    FViewportClient *ViewportClient = nullptr;
    EStatMode        StatMode = EStatMode::Fps;
};
