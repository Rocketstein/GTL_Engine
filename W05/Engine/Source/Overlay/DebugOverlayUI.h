#pragma once

#include "Core/Containers/String.h"
#include "Core/Logging/LogOutputDevice.h"
#include "Core/Platform/PlatformTypes.h"
#include "Overlay/Panels/ConsolePanel.h"
#include "Overlay/Panels/ControlPanel.h"
#include "Overlay/Panels/StatsPanel.h"
#include <array>
#include <deque>
#include <filesystem>

class FEngine;
class FViewportClient;
class FScene;
class FInputState;

struct FLogEntry
{
    ELogLevel Level = ELogLevel::Info;
    FString   Message;
};

class FDebugOverlayUI : public ILogOutputDevice
{
  public:
    void SetEngine(FEngine *InEngine) { Engine = InEngine; }
    void SetViewportClient(FViewportClient *InViewportClient)
    {
        ViewportClient = InViewportClient;
        StatsPanel.SetViewportClient(InViewportClient);
    }
    void SetInputState(FInputState *InInputState) { InputState = InInputState; }

    void Render();

    void Log(ELogLevel Level, const char *Message) override;

  private:
    friend class FControlPanel;
    friend class FConsolePanel;

    void HandleHotkeys();
    void RenderWorldGuides();
    void PreloadSceneAssetsForLogging(FScene *InScene);
    void ExecuteConsoleCommand(const FString &CommandText);
    bool OpenSceneFileDialog(bool bSaveDialog, std::filesystem::path &OutPath) const;

  private:
    FEngine         *Engine = nullptr;
    FViewportClient *ViewportClient = nullptr;
    FInputState     *InputState = nullptr;

    FStatsPanel   StatsPanel;
    FControlPanel ControlPanel;
    FConsolePanel ConsolePanel;

    FString               SceneName = "Default";
    std::filesystem::path CurrentSceneFilePath;
    std::deque<FLogEntry> LogEntries;
    std::array<bool, 5>   LevelFilter = {true, true, true, true, true};
    int32                 SelectedMinimumLevel = static_cast<int32>(ELogLevel::Verbose);
    char                  SearchBuffer[128] = {};
    char                  CommandBuffer[128] = {};
    bool                  bAutoScroll = true;
    bool                  bScrollToBottom = false;
    bool                  bShowControlPanel = true;
    bool                  bShowConsolePanel = true;
};
