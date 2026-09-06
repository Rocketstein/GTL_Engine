// Output Log 패널과 drawer 콘텐츠를 렌더링합니다.
#pragma once

#include "Core/CoreTypes.h"

#include <array>
#include <functional>
#include <sstream>

#include "Core/Logging/LogOutputDevice.h"
#include "Editor/UI/EditorPanel.h"
#include "ImGui/imgui.h"

class FLogBuffer;
struct FLogEntry;

class FEditorOutputLogPanel : public FEditorPanel
{
public:
    void Initialize(UEditorEngine* InEditorEngine, FLogBuffer* InLogBuffer);
    virtual void Initialize(UEditorEngine* InEditorEngine) override;
    virtual void Render(float DeltaTime) override;

    void RenderContent(float DeltaTime);
    void Clear();
    void RequestCommandInputFocus() { bReclaimFocus = true; }

private:
    std::array<char, 256> InputBuf{};
    std::array<char, 256> SearchBuf{};
    TArray<FString> History;
    int32 HistoryPos = -1;
    bool AutoScroll = true;
    bool ScrollToBottom = true;
    bool bReclaimFocus = false;
    bool bShowLogs = true;
    bool bShowWarnings = true;
    bool bShowErrors = true;
    bool bHasUnreadLogs = false;
    bool bCommandSuggestionsOpen = false;
    int32 SelectedLogIndex = -1;
    int32 LastLogEntryCount = 0;
    FLogBuffer* LogBuffer = nullptr;
    TMap<FString, bool> CategoryVisibility;

    using CommandFn = std::function<void(const TArray<FString>& args)>;
    struct FCommandEntry
    {
        CommandFn Execute;
        FString Usage;
        FString Description;
    };
    TMap<FString, FCommandEntry> Commands;

    bool ShouldDisplayEntry(ELogLevel Level, const FString& Category, const FString& Text) const;
    void SyncCategories();
    void DrawToolbar();
    void DrawFiltersPopup();
    void DrawLogOutput();
    void DrawLogEntryRow(const FLogEntry& Entry, int32 EntryIndex, const FString& SearchText);
    void DrawInputRow();
    void DrawCommandSuggestions(bool bInputFocused);
    void RegisterCommand(const FString& Name, CommandFn Fn, const FString& Usage, const FString& Description);
    void PrintHelp(const TArray<FString>& Args) const;
    void ExecCommand(const char* CommandLine);
    FString GetCurrentCommandPrefix() const;
    TArray<FString> BuildCommandCandidates(const FString& Prefix) const;
    FString BuildLogLine(const FLogEntry& Entry) const;
    void CopyVisibleLogsToClipboard() const;
    void CopyAllLogsToClipboard() const;
    int32 CountVisibleEntries() const;
    int32 CountDisabledFilters() const;
    static int32 TextEditCallback(ImGuiInputTextCallbackData* Data);
};
