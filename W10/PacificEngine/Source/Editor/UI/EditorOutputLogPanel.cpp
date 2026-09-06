// Output Log 패널과 drawer 콘텐츠를 구현합니다.
#include "Editor/UI/EditorOutputLogPanel.h"

#include "Core/Logging/LogBuffer.h"
#include "Core/Logging/LogMacros.h"
#include "Editor/EditorEngine.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Platform/Paths.h"
#include "Platform/PlatformProcess.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Resources/Shadows/ShadowMapSettings.h"
#include "Render/Resources/Shaders/MeshPassOverrideSettings.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

namespace
{
struct FOutputLogFilterCounts
{
    int32 Logs = 0;
    int32 Warnings = 0;
    int32 Errors = 0;
};

struct FCategoryFilterEntry
{
    FString Name;
    int32 Count = 0;
};

constexpr float SeverityColumnWidth = 78.0f;
constexpr float CategoryColumnWidth = 138.0f;
constexpr float RowHorizontalPadding = 7.0f;

ImVec4 GetLogTextColor(ELogLevel Level)
{
    switch (Level)
    {
    case ELogLevel::Verbose:
        return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
    case ELogLevel::Debug:
        return ImVec4(0.86f, 0.86f, 0.86f, 1.0f);
    case ELogLevel::Info:
        return ImVec4(0.72f, 0.86f, 1.0f, 1.0f);
    case ELogLevel::Warning:
        return ImVec4(1.00f, 0.77f, 0.28f, 1.0f);
    case ELogLevel::Error:
        return ImVec4(1.00f, 0.34f, 0.34f, 1.0f);
    default:
        return ImVec4(1, 1, 1, 1);
    }
}

ImVec4 GetLogFilterColor(ELogLevel Level)
{
    switch (Level)
    {
    case ELogLevel::Warning:
        return ImVec4(0.84f, 0.58f, 0.12f, 1.0f);
    case ELogLevel::Error:
        return ImVec4(0.78f, 0.18f, 0.18f, 1.0f);
    case ELogLevel::Verbose:
    case ELogLevel::Debug:
    case ELogLevel::Info:
        return ImVec4(0.22f, 0.44f, 0.72f, 1.0f);
    default:
        return ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    }
}

ImVec4 Dim(const ImVec4& Color, float Factor)
{
    return ImVec4(Color.x * Factor, Color.y * Factor, Color.z * Factor, Color.w);
}

ImVec4 Brighten(const ImVec4& Color, float Amount)
{
    return ImVec4(
        (std::min)(1.0f, Color.x + Amount),
        (std::min)(1.0f, Color.y + Amount),
        (std::min)(1.0f, Color.z + Amount),
        Color.w);
}

bool IsLogMessage(ELogLevel Level)
{
    return Level != ELogLevel::Warning && Level != ELogLevel::Error;
}

const char* GetOutputLogSeverityLabel(ELogLevel Level)
{
    if (Level == ELogLevel::Error)
    {
        return "Error";
    }
    if (Level == ELogLevel::Warning)
    {
        return "Warning";
    }
    return "Log";
}

FString GetSearchableLogText(const FLogEntry& Entry)
{
    FString Text = Entry.Category;
    Text += " ";
    Text += Entry.Message;
    Text += " ";
    Text += Entry.FormattedMessage;
    return Text;
}

FOutputLogFilterCounts CountLogEntries(const FLogBuffer* LogBuffer)
{
    FOutputLogFilterCounts Counts;
    if (!LogBuffer)
    {
        return Counts;
    }

    for (const FLogEntry& Entry : LogBuffer->GetEntries())
    {
        if (Entry.Level == ELogLevel::Error)
        {
            ++Counts.Errors;
        }
        else if (Entry.Level == ELogLevel::Warning)
        {
            ++Counts.Warnings;
        }
        else
        {
            ++Counts.Logs;
        }
    }
    return Counts;
}

void DrawMessageFilterCheckbox(const char* Label, int32 Count, bool& bEnabled)
{
    char CheckboxLabel[64];
    std::snprintf(CheckboxLabel, sizeof(CheckboxLabel), "%s (%d)", Label, Count);
    ImGui::Checkbox(CheckboxLabel, &bEnabled);
}

TArray<FCategoryFilterEntry> BuildCategoryEntries(const FLogBuffer* LogBuffer)
{
    TMap<FString, int32> Counts;
    if (LogBuffer)
    {
        for (const FLogEntry& Entry : LogBuffer->GetEntries())
        {
            ++Counts[Entry.Category];
        }
    }

    TArray<FCategoryFilterEntry> Entries;
    Entries.reserve(Counts.size());
    for (const auto& Pair : Counts)
    {
        Entries.push_back({ Pair.first, Pair.second });
    }

    std::sort(Entries.begin(), Entries.end(), [](const FCategoryFilterEntry& A, const FCategoryFilterEntry& B)
              {
                  return A.Name < B.Name;
              });
    return Entries;
}

FString ToLowerAsciiCopy(const FString& Value)
{
    FString Lower = Value;
    std::transform(Lower.begin(), Lower.end(), Lower.begin(),
                   [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
    return Lower;
}

bool StartsWith(const FString& Value, const FString& Prefix)
{
    return Value.size() >= Prefix.size() && Value.compare(0, Prefix.size(), Prefix) == 0;
}

bool ContainsCaseInsensitive(const FString& Value, const FString& Needle)
{
    if (Needle.empty())
    {
        return true;
    }

    return ToLowerAsciiCopy(Value).find(ToLowerAsciiCopy(Needle)) != FString::npos;
}

FString FindCommonPrefix(const TArray<FString>& Values)
{
    if (Values.empty())
    {
        return "";
    }

    FString Prefix = Values[0];
    for (size_t Index = 1; Index < Values.size(); ++Index)
    {
        const FString& Value = Values[Index];
        size_t MatchLength = 0;
        while (MatchLength < Prefix.size() &&
               MatchLength < Value.size() &&
               Prefix[MatchLength] == Value[MatchLength])
        {
            ++MatchLength;
        }
        Prefix.resize(MatchLength);
    }
    return Prefix;
}

void DrawHighlightedText(ImDrawList* DrawList, ImVec2 Pos, const FString& Text, const FString& SearchText, ImU32 TextColor)
{
    if (Text.empty())
    {
        return;
    }

    const FString LowerText = ToLowerAsciiCopy(Text);
    const FString LowerSearch = ToLowerAsciiCopy(SearchText);
    if (LowerSearch.empty() || LowerText.find(LowerSearch) == FString::npos)
    {
        DrawList->AddText(Pos, TextColor, Text.c_str());
        return;
    }

    const ImU32 HighlightBg = ImGui::GetColorU32(ImVec4(0.95f, 0.74f, 0.22f, 0.42f));
    size_t Cursor = 0;
    float X = Pos.x;

    while (Cursor < Text.size())
    {
        const size_t Match = LowerText.find(LowerSearch, Cursor);
        if (Match == FString::npos)
        {
            const FString Tail = Text.substr(Cursor);
            DrawList->AddText(ImVec2(X, Pos.y), TextColor, Tail.c_str());
            break;
        }

        if (Match > Cursor)
        {
            const FString Before = Text.substr(Cursor, Match - Cursor);
            DrawList->AddText(ImVec2(X, Pos.y), TextColor, Before.c_str());
            X += ImGui::CalcTextSize(Before.c_str()).x;
        }

        const FString MatchedText = Text.substr(Match, LowerSearch.size());
        const ImVec2 MatchSize = ImGui::CalcTextSize(MatchedText.c_str());
        DrawList->AddRectFilled(
            ImVec2(X - 1.0f, Pos.y - 1.0f),
            ImVec2(X + MatchSize.x + 1.0f, Pos.y + MatchSize.y + 1.0f),
            HighlightBg,
            2.0f);
        DrawList->AddText(ImVec2(X, Pos.y), TextColor, MatchedText.c_str());
        X += MatchSize.x;
        Cursor = Match + LowerSearch.size();
    }
}

int32 CountTextLines(const FString& Text)
{
    if (Text.empty())
    {
        return 1;
    }

    int32 LineCount = 1;
    for (const char Ch : Text)
    {
        if (Ch == '\n')
        {
            ++LineCount;
        }
    }
    return LineCount;
}

float CalculateLogEntryRowHeight(const FLogEntry& Entry)
{
    const int32 MessageLineCount = CountTextLines(Entry.Message);
    return ImGui::GetTextLineHeight() * static_cast<float>(MessageLineCount) +
           ImGui::GetStyle().ItemSpacing.y * static_cast<float>((std::max)(0, MessageLineCount - 1)) +
           6.0f;
}

float CalculateLogEntryRowWidth(const FLogEntry& Entry, float AvailableWidth)
{
    const float MessageWidth = ImGui::CalcTextSize(Entry.Message.c_str()).x;
    return (std::max)(
        AvailableWidth,
        SeverityColumnWidth + CategoryColumnWidth + MessageWidth + RowHorizontalPadding * 4.0f);
}

void DrawHighlightedMultilineText(ImDrawList* DrawList, ImVec2 Pos, const FString& Text, const FString& SearchText, ImU32 TextColor)
{
    const float LineStride = ImGui::GetTextLineHeightWithSpacing();
    size_t LineStart = 0;
    int32 LineIndex = 0;

    while (LineStart <= Text.size())
    {
        const size_t LineEnd = Text.find('\n', LineStart);
        FString Line = Text.substr(LineStart, LineEnd == FString::npos ? FString::npos : LineEnd - LineStart);
        if (!Line.empty() && Line.back() == '\r')
        {
            Line.pop_back();
        }

        DrawHighlightedText(DrawList, ImVec2(Pos.x, Pos.y + LineStride * static_cast<float>(LineIndex)), Line, SearchText, TextColor);

        if (LineEnd == FString::npos)
        {
            break;
        }
        LineStart = LineEnd + 1;
        ++LineIndex;
    }
}

bool IsLuaPathDelimiter(char Ch)
{
    switch (Ch)
    {
    case '\'':
    case '"':
    case '`':
    case '<':
    case '>':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '\r':
    case '\n':
    case '\t':
    case ' ':
        return true;
    default:
        return false;
    }
}

FString TrimLuaPathCandidate(FString Value)
{
    while (!Value.empty() && (Value.front() == '@' || IsLuaPathDelimiter(Value.front())))
    {
        Value.erase(Value.begin());
    }
    while (!Value.empty() && IsLuaPathDelimiter(Value.back()))
    {
        Value.pop_back();
    }
    return Value;
}

FString ExtractLuaPathCandidate(const FString& Text)
{
    const FString LowerText = ToLowerAsciiCopy(Text);
    size_t LuaExtension = LowerText.find(".lua");
    while (LuaExtension != FString::npos)
    {
        size_t Begin = LuaExtension;
        while (Begin > 0 && !IsLuaPathDelimiter(Text[Begin - 1]))
        {
            --Begin;
        }

        FString Candidate = Text.substr(Begin, LuaExtension + 4 - Begin);
        Candidate = TrimLuaPathCandidate(Candidate);
        if (!Candidate.empty())
        {
            return Candidate;
        }

        LuaExtension = LowerText.find(".lua", LuaExtension + 4);
    }
    return "";
}

std::filesystem::path ResolveLuaScriptPath(const FString& PathCandidate)
{
    if (PathCandidate.empty())
    {
        return {};
    }

    auto TryExistingPath = [](const std::filesystem::path& Path) -> std::filesystem::path
    {
        const std::filesystem::path Normalized = Path.lexically_normal();
        std::error_code Ec;
        if (std::filesystem::exists(Normalized, Ec) && !Ec)
        {
            return Normalized;
        }
        return {};
    };

    std::filesystem::path Candidate = FPaths::ToPath(PathCandidate);
    if (Candidate.is_absolute())
    {
        return TryExistingPath(Candidate);
    }

    if (std::filesystem::path Resolved = TryExistingPath(std::filesystem::path(FPaths::ScriptsDir()) / Candidate);
        !Resolved.empty())
    {
        return Resolved;
    }

    if (std::filesystem::path Resolved = TryExistingPath(std::filesystem::path(FPaths::RootDir()) / Candidate);
        !Resolved.empty())
    {
        return Resolved;
    }

    return TryExistingPath(std::filesystem::current_path() / Candidate);
}

std::filesystem::path ResolveLuaScriptPath(const FLogEntry& Entry)
{
    const FString Category = ToLowerAsciiCopy(Entry.Category);
    const FString SearchText = Entry.Message + "\n" + Entry.FormattedMessage;
    if (Category.find("lua") == FString::npos &&
        ToLowerAsciiCopy(SearchText).find(".lua") == FString::npos)
    {
        return {};
    }

    return ResolveLuaScriptPath(ExtractLuaPathCandidate(SearchText));
}
} // namespace

void FEditorOutputLogPanel::Initialize(UEditorEngine* InEditorEngine, FLogBuffer* InLogBuffer)
{
    FEditorPanel::Initialize(InEditorEngine);
    LogBuffer = InLogBuffer;
    InputBuf.fill('\0');
    SearchBuf.fill('\0');

    RegisterCommand("help", [this](const TArray<FString>& Args)
                    {
                        PrintHelp(Args);
                    },
                    "help [command]",
                    "Lists commands or shows detailed usage for one command.");

    RegisterCommand("clear", [this](const TArray<FString>& Args)
                    {
                        (void)Args;
                        Clear();
                    },
                    "clear",
                    "Clears the Output Log.");

    auto AutoScrollCommand = [this](const TArray<FString>& Args)
    {
        if (Args.size() < 2)
        {
            UE_LOG(OutputLog, Info, "Auto-scroll is %s.", AutoScroll ? "on" : "off");
            UE_LOG(OutputLog, Info, "Usage: auto_scroll <on|off|toggle>");
            return;
        }

        const FString Option = ToLowerAsciiCopy(Args[1]);
        if (Option == "on" || Option == "true" || Option == "1")
        {
            AutoScroll = true;
        }
        else if (Option == "off" || Option == "false" || Option == "0")
        {
            AutoScroll = false;
        }
        else if (Option == "toggle")
        {
            AutoScroll = !AutoScroll;
        }
        else
        {
            UE_LOG(OutputLog, Warning, "Usage: auto_scroll <on|off|toggle>");
            return;
        }

        UE_LOG(OutputLog, Info, "Auto-scroll %s.", AutoScroll ? "enabled" : "disabled");
    };

    RegisterCommand("auto_scroll", AutoScrollCommand,
                    "auto_scroll <on|off|toggle>",
                    "Controls Output Log auto-scroll.");
    RegisterCommand("autoscroll", AutoScrollCommand,
                    "autoscroll <on|off|toggle>",
                    "Alias for auto_scroll.");

    RegisterCommand("stat", [this](const TArray<FString>& Args)
                    {
                        if (EditorEngine == nullptr)
                        {
                            UE_LOG(OutputLog, Error, "EditorEngine is null.");
                            return;
                        }

                        if (Args.size() < 2)
                        {
                            UE_LOG(OutputLog, Warning, "Usage: stat fps | stat memory | stat shadow | stat lightcull | stat none");
                            return;
                        }

                        FOverlayStatSystem& StatSystem = EditorEngine->GetOverlayStatSystem();
                        const FString& SubCommand = Args[1];

                        if (SubCommand == "fps")
                        {
                            StatSystem.ShowFPS(true);
                            UE_LOG(OutputLog, Info, "Overlay stat enabled: fps");
                        }
                        else if (SubCommand == "memory")
                        {
                            StatSystem.ShowMemory(true);
                            UE_LOG(OutputLog, Info, "Overlay stat enabled: memory");
                        }
                        else if (SubCommand == "shadow")
                        {
                            StatSystem.ShowShadow(true);
                            UE_LOG(OutputLog, Info, "Overlay stat enabled: shadow");
                        }
                        else if (SubCommand == "lightcull")
                        {
                            StatSystem.ShowLightCull(true);
                            UE_LOG(OutputLog, Info, "Overlay stat enabled: lightcull");
                        }
                        else if (SubCommand == "none")
                        {
                            StatSystem.HideAll();
                            UE_LOG(OutputLog, Info, "Overlay stat disabled: all");
                        }
                        else
                        {
                            UE_LOG(OutputLog, Error, "Unknown stat command: '%s'", SubCommand.c_str());
                            UE_LOG(OutputLog, Warning, "Usage: stat fps | stat memory | stat shadow | stat lightcull | stat none");
                        }
                    },
                    "stat <fps|memory|shadow|lightcull|none>",
                    "Controls editor overlay stat display.");

    RegisterCommand("shadow_filter", [this](const TArray<FString>& Args)
                    {
                        if (Args.size() < 2)
                        {
                            UE_LOG(OutputLog, Warning, "Usage: shadow_filter NONE | PCF | VSM | ESM");
                            UE_LOG(OutputLog, Info, "Current shadow filter: %s", GetShadowFilterMethodName(GetShadowFilterMethod()));
                            return;
                        }

                        FString FilterName = Args[1];
                        std::transform(FilterName.begin(), FilterName.end(), FilterName.begin(),
                                       [](unsigned char Ch) { return static_cast<char>(std::toupper(Ch)); });

                        if (FilterName == "NONE")
                        {
                            SetShadowFilterMethod(EShadowFilterMethod::None);
                        }
                        else if (FilterName == "PCF")
                        {
                            SetShadowFilterMethod(EShadowFilterMethod::PCF);
                        }
                        else if (FilterName == "VSM")
                        {
                            SetShadowFilterMethod(EShadowFilterMethod::VSM);
                        }
                        else if (FilterName == "ESM")
                        {
                            SetShadowFilterMethod(EShadowFilterMethod::ESM);
                        }
                        else
                        {
                            UE_LOG(OutputLog, Error, "Unknown shadow filter: '%s'", Args[1].c_str());
                            UE_LOG(OutputLog, Warning, "Usage: shadow_filter NONE | PCF | VSM | ESM");
                            return;
                        }

                        UE_LOG(OutputLog, Info, "Shadow filter changed to: %s", GetShadowFilterMethodName(GetShadowFilterMethod()));
                    },
                    "shadow_filter <NONE|PCF|VSM|ESM>",
                    "Changes the active shadow filtering method.");

    RegisterCommand("shadow_mode", [this](const TArray<FString>& Args)
    {
        if (Args.size() < 2)
        {
            UE_LOG(OutputLog, Warning, "Usage: shadow_mode STANDARD | PSM | CASCADE");
            UE_LOG(OutputLog, Info, "Current shadow mode: %s", GetShadowMapMethodName(GetShadowMapMethod()));
            return;
        }

        FString MethodName = Args[1];
        std::transform(MethodName.begin(), MethodName.end(), MethodName.begin(),
                       [](unsigned char Ch) { return static_cast<char>(std::toupper(Ch)); });

        if (MethodName == "STANDARD" || MethodName == "SSM")
        {
            SetShadowMapMethod(EShadowMapMethod::Standard);
        }
        else if (MethodName == "PSM")
        {
            SetShadowMapMethod(EShadowMapMethod::PSM);
        }
        else if (MethodName == "CASCADE" || MethodName == "CSM")
        {
            SetShadowMapMethod(EShadowMapMethod::Cascade);
        }
        else
        {
            UE_LOG(OutputLog, Error, "Unknown shadow map method: '%s'", Args[1].c_str());
            UE_LOG(OutputLog, Warning, "Usage: shadow_mode STANDARD | PSM | CASCADE");
            return;
        }

        UE_LOG(OutputLog, Info, "Shadow map method changed to: %s", GetShadowMapMethodName(GetShadowMapMethod()));
    },
    "shadow_mode <STANDARD|PSM|CASCADE>",
    "Changes the active shadow map method.");

    RegisterCommand("opaque_legacy_override_bypass", [this](const TArray<FString>& Args)
    {
        if (Args.size() < 2)
        {
            UE_LOG(OutputLog, Warning, "Usage: opaque_legacy_override_bypass <on|off|toggle>");
            UE_LOG(OutputLog, Info, "Current opaque legacy override bypass: %s", GetLegacyOpaqueViewModeOverrideBypassStateName(IsLegacyOpaqueViewModeOverrideBypassed()));
            return;
        }

        FString Mode = Args[1];
        std::transform(Mode.begin(), Mode.end(), Mode.begin(),
                       [](unsigned char Ch) { return static_cast<char>(std::toupper(Ch)); });

        bool bNewState = IsLegacyOpaqueViewModeOverrideBypassed();
        if (Mode == "ON")
        {
            bNewState = true;
        }
        else if (Mode == "OFF")
        {
            bNewState = false;
        }
        else if (Mode == "TOGGLE")
        {
            bNewState = !bNewState;
        }
        else
        {
            UE_LOG(OutputLog, Error, "Unknown opaque legacy override bypass mode: '%s'", Args[1].c_str());
            UE_LOG(OutputLog, Warning, "Usage: opaque_legacy_override_bypass <on|off|toggle>");
            return;
        }

        SetLegacyOpaqueViewModeOverrideBypassed(bNewState);
        UE_LOG(
            OutputLog,
            Info,
            "Opaque legacy override bypass %s. Current state: %s",
            bNewState ? "enabled" : "disabled",
            GetLegacyOpaqueViewModeOverrideBypassStateName(IsLegacyOpaqueViewModeOverrideBypassed()));
    },
    "opaque_legacy_override_bypass <on|off|toggle>",
    "Bypasses the legacy opaque view-mode shader override path.");

    UE_LOG(OutputLog, Info, "Output Log initialized.");
}

void FEditorOutputLogPanel::Initialize(UEditorEngine* InEditorEngine)
{
    Initialize(InEditorEngine, nullptr);
}

void FEditorOutputLogPanel::Clear()
{
    if (LogBuffer)
    {
        LogBuffer->Clear();
    }
    CategoryVisibility.clear();

    ScrollToBottom = true;
}

bool FEditorOutputLogPanel::ShouldDisplayEntry(ELogLevel Level, const FString& Category, const FString& Text) const
{
    if (Level == ELogLevel::Error)
    {
        if (!bShowErrors)
        {
            return false;
        }
    }
    else if (Level == ELogLevel::Warning)
    {
        if (!bShowWarnings)
        {
            return false;
        }
    }
    else if (IsLogMessage(Level) && !bShowLogs)
    {
        return false;
    }

    auto CategoryIt = CategoryVisibility.find(Category);
    if (CategoryIt != CategoryVisibility.end() && !CategoryIt->second)
    {
        return false;
    }

    const FString SearchText = ToLowerAsciiCopy(SearchBuf.data());
    if (SearchText.empty())
    {
        return true;
    }

    return ContainsCaseInsensitive(Text, SearchText);
}

void FEditorOutputLogPanel::SyncCategories()
{
    if (!LogBuffer)
    {
        return;
    }

    for (const FLogEntry& Entry : LogBuffer->GetEntries())
    {
        if (CategoryVisibility.find(Entry.Category) == CategoryVisibility.end())
        {
            CategoryVisibility[Entry.Category] = true;
        }
    }
}

void FEditorOutputLogPanel::DrawToolbar()
{
    SyncCategories();

    const FOutputLogFilterCounts Counts = CountLogEntries(LogBuffer);
    const int32 DisabledFilterCount = CountDisabledFilters();

    char FilterLabel[32];
    if (DisabledFilterCount > 0)
    {
        std::snprintf(FilterLabel, sizeof(FilterLabel), "Filters (%d)", DisabledFilterCount);
    }
    else
    {
        std::snprintf(FilterLabel, sizeof(FilterLabel), "Filters");
    }

    const float FilterWidth = ImGui::CalcTextSize(FilterLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f +
                              ImGui::GetStyle().ItemSpacing.x * 3.0f;
    const float SearchWidth = (std::max)(220.0f, ImGui::GetContentRegionAvail().x - FilterWidth);

    ImGui::SetNextItemWidth(SearchWidth);
    ImGui::InputTextWithHint("##OutputLogSearch", "Search Output Log", SearchBuf.data(), SearchBuf.size());

    ImGui::SameLine();
    if (ImGui::Button(FilterLabel))
    {
        ImGui::OpenPopup("OutputLogFiltersPopup");
    }
    DrawFiltersPopup();

    const int32 TotalCount = Counts.Logs + Counts.Warnings + Counts.Errors;
    const int32 VisibleCount = CountVisibleEntries();
    ImGui::TextDisabled("Visible %d / Total %d    Logs %d    Warnings %d    Errors %d    Auto-scroll %s",
                        VisibleCount,
                        TotalCount,
                        Counts.Logs,
                        Counts.Warnings,
                        Counts.Errors,
                        AutoScroll ? "On" : "Off");
    if (bHasUnreadLogs)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.26f, 1.0f), "New output");
    }
}

void FEditorOutputLogPanel::DrawFiltersPopup()
{
    if (!ImGui::BeginPopup("OutputLogFiltersPopup"))
    {
        return;
    }

    SyncCategories();
    const FOutputLogFilterCounts Counts = CountLogEntries(LogBuffer);

    ImGui::TextUnformatted("Messages");
    ImGui::Separator();
    DrawMessageFilterCheckbox("Logs", Counts.Logs, bShowLogs);
    DrawMessageFilterCheckbox("Warnings", Counts.Warnings, bShowWarnings);
    DrawMessageFilterCheckbox("Errors", Counts.Errors, bShowErrors);

    ImGui::Spacing();
    ImGui::TextUnformatted("Categories");
    ImGui::Separator();

    if (ImGui::SmallButton("All"))
    {
        for (auto& Pair : CategoryVisibility)
        {
            Pair.second = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("None"))
    {
        for (auto& Pair : CategoryVisibility)
        {
            Pair.second = false;
        }
    }

    const TArray<FCategoryFilterEntry> CategoryEntries = BuildCategoryEntries(LogBuffer);
    if (CategoryEntries.empty())
    {
        ImGui::TextDisabled("No categories");
    }
    else
    {
        const float CategoryListHeight = (std::min)(220.0f, 24.0f * static_cast<float>(CategoryEntries.size()) + 8.0f);
        if (ImGui::BeginChild("OutputLogCategoryFilters", ImVec2(320.0f, CategoryListHeight), true))
        {
            for (const FCategoryFilterEntry& Entry : CategoryEntries)
            {
                bool& bVisible = CategoryVisibility[Entry.Name];

                char CategoryLabel[256];
                std::snprintf(CategoryLabel, sizeof(CategoryLabel), "%s (%d)", Entry.Name.c_str(), Entry.Count);
                ImGui::Checkbox(CategoryLabel, &bVisible);
            }
        }
        ImGui::EndChild();
    }

    ImGui::EndPopup();
}

void FEditorOutputLogPanel::DrawLogOutput()
{
    const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (!ImGui::BeginChild("OutputLogScrollingRegion", ImVec2(0, -FooterHeight), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::EndChild();
        return;
    }

    const bool bWasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f;
    const float ClipMinY = ImGui::GetWindowPos().y;
    const float ClipMaxY = ClipMinY + ImGui::GetWindowSize().y;
    int32 VisibleRows = 0;

    if (LogBuffer)
    {
        const TArray<FLogEntry>& Entries = LogBuffer->GetEntries();
        const int32 EntryCount = static_cast<int32>(Entries.size());
        if (EntryCount != LastLogEntryCount)
        {
            if (EntryCount > LastLogEntryCount && !bWasAtBottom && !ScrollToBottom)
            {
                bHasUnreadLogs = true;
            }
            LastLogEntryCount = EntryCount;
        }

        for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
        {
            const FLogEntry& Entry = Entries[EntryIndex];
            if (!ShouldDisplayEntry(Entry.Level, Entry.Category, GetSearchableLogText(Entry)))
            {
                continue;
            }

            const float RowHeight = CalculateLogEntryRowHeight(Entry);
            const float RowWidth = CalculateLogEntryRowWidth(Entry, ImGui::GetContentRegionAvail().x);
            const float RowStartY = ImGui::GetCursorScreenPos().y;
            const float RowEndY = RowStartY + RowHeight;
            if (RowEndY < ClipMinY || RowStartY > ClipMaxY)
            {
                ImGui::Dummy(ImVec2(RowWidth, RowHeight));
            }
            else
            {
                DrawLogEntryRow(Entry, EntryIndex, SearchBuf.data());
            }
            ++VisibleRows;
        }
    }

    if (VisibleRows == 0)
    {
        ImGui::TextDisabled("No matching log entries.");
    }

    if (ImGui::BeginPopupContextWindow("OutputLogBackgroundContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("Copy Visible Logs"))
        {
            CopyVisibleLogsToClipboard();
        }
        if (ImGui::MenuItem("Copy All Logs"))
        {
            CopyAllLogsToClipboard();
        }
        ImGui::EndPopup();
    }

    if (ScrollToBottom || (AutoScroll && bWasAtBottom))
    {
        ImGui::SetScrollHereY(1.0f);
        bHasUnreadLogs = false;
    }
    ScrollToBottom = false;

    ImGui::EndChild();
}

void FEditorOutputLogPanel::DrawLogEntryRow(const FLogEntry& Entry, int32 EntryIndex, const FString& SearchText)
{
    const float RowHeight = CalculateLogEntryRowHeight(Entry);
    const float RowWidth = CalculateLogEntryRowWidth(Entry, ImGui::GetContentRegionAvail().x);
    const std::filesystem::path LuaScriptPath = ResolveLuaScriptPath(Entry);

    ImGui::PushID(EntryIndex);
    const bool bSelected = SelectedLogIndex == EntryIndex;
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.16f, 0.20f, 0.25f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.19f, 0.21f, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.20f, 0.23f, 0.27f, 0.78f));
    if (ImGui::Selectable("##OutputLogRow", bSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(RowWidth, RowHeight)))
    {
        SelectedLogIndex = EntryIndex;
    }
    if (!LuaScriptPath.empty() && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        if (!FPlatformProcess::OpenFile(LuaScriptPath))
        {
            const FString FilePath = FPaths::FromPath(LuaScriptPath);
            UE_LOG(OutputLog, Warning, "Failed to open Lua script: %s", FilePath.c_str());
        }
    }
    if (!LuaScriptPath.empty() && ImGui::IsItemHovered())
    {
        const FString FilePath = FPaths::FromPath(LuaScriptPath);
        ImGui::SetTooltip("Double-click to open %s", FilePath.c_str());
    }
    ImGui::PopStyleColor(3);

    if (ImGui::BeginPopupContextItem("OutputLogRowContext"))
    {
        if (!LuaScriptPath.empty() && ImGui::MenuItem("Open Lua Script"))
        {
            if (!FPlatformProcess::OpenFile(LuaScriptPath))
            {
                const FString FilePath = FPaths::FromPath(LuaScriptPath);
                UE_LOG(OutputLog, Warning, "Failed to open Lua script: %s", FilePath.c_str());
            }
        }
        if (!LuaScriptPath.empty())
        {
            ImGui::Separator();
        }
        if (ImGui::MenuItem("Copy Message"))
        {
            ImGui::SetClipboardText(Entry.Message.c_str());
        }
        if (ImGui::MenuItem("Copy Row"))
        {
            const FString Line = BuildLogLine(Entry);
            ImGui::SetClipboardText(Line.c_str());
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy Visible Logs"))
        {
            CopyVisibleLogsToClipboard();
        }
        if (ImGui::MenuItem("Copy All Logs"))
        {
            CopyAllLogsToClipboard();
        }
        ImGui::EndPopup();
    }

    const ImVec2 ItemMin = ImGui::GetItemRectMin();
    const ImVec2 ItemMax = ImGui::GetItemRectMax();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImVec4 SeverityColor = GetLogTextColor(Entry.Level);
    const ImU32 SeverityU32 = ImGui::GetColorU32(SeverityColor);
    const ImU32 MutedU32 = ImGui::GetColorU32(ImVec4(0.64f, 0.67f, 0.70f, 1.0f));
    const ImU32 MessageU32 = ImGui::GetColorU32(SeverityColor);

    DrawList->AddRectFilled(
        ImVec2(ItemMin.x + 1.0f, ItemMin.y + 3.0f),
        ImVec2(ItemMin.x + 4.0f, ItemMax.y - 3.0f),
        SeverityU32,
        1.5f);

    const float TextY = ItemMin.y + 3.0f;
    const ImVec2 SeverityPos(ItemMin.x + RowHorizontalPadding + 4.0f, TextY);
    const ImVec2 CategoryPos(ItemMin.x + SeverityColumnWidth, TextY);
    const ImVec2 MessagePos(ItemMin.x + SeverityColumnWidth + CategoryColumnWidth, TextY);

    DrawList->AddText(SeverityPos, SeverityU32, GetOutputLogSeverityLabel(Entry.Level));
    DrawHighlightedText(DrawList, CategoryPos, Entry.Category, SearchText, MutedU32);
    DrawHighlightedMultilineText(DrawList, MessagePos, Entry.Message, SearchText, MessageU32);
    ImGui::PopID();
}

void FEditorOutputLogPanel::DrawInputRow()
{
    ImGuiInputTextFlags Flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_EscapeClearsAll |
                                ImGuiInputTextFlags_CallbackHistory |
                                ImGuiInputTextFlags_CallbackCompletion;

    ImGui::TextUnformatted("Cmd");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##OutputLogCommand", "Enter command. Type help or press Tab to autocomplete.", InputBuf.data(), InputBuf.size(), Flags, &TextEditCallback, this))
    {
        ExecCommand(InputBuf.data());
        InputBuf[0] = '\0';
        bReclaimFocus = true;
    }
    const bool bInputFocused = ImGui::IsItemFocused() || ImGui::IsItemActive();

    ImGui::SetItemDefaultFocus();
    if (bReclaimFocus)
    {
        ImGui::SetKeyboardFocusHere(-1);
        bReclaimFocus = false;
    }

    DrawCommandSuggestions(bInputFocused);
}

void FEditorOutputLogPanel::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(860, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Output Log"))
    {
        ImGui::End();
        return;
    }

    RenderContent(DeltaTime);
    ImGui::End();
}

void FEditorOutputLogPanel::RenderContent(float DeltaTime)
{
    (void)DeltaTime;

    DrawToolbar();
    ImGui::Separator();
    DrawLogOutput();
    ImGui::Separator();
    DrawInputRow();
}

void FEditorOutputLogPanel::DrawCommandSuggestions(bool bInputFocused)
{
    const FString CommandLine(InputBuf.data());
    const size_t FirstNonWhitespace = CommandLine.find_first_not_of(" \t");
    const bool bOnlyEditingCommandName =
        FirstNonWhitespace != FString::npos &&
        CommandLine.find_first_of(" \t", FirstNonWhitespace) == FString::npos;
    const FString Prefix = bOnlyEditingCommandName ? GetCurrentCommandPrefix() : "";
    const TArray<FString> Candidates = !Prefix.empty() ? BuildCommandCandidates(Prefix) : TArray<FString>();
    if (bInputFocused && !Candidates.empty())
    {
        bCommandSuggestionsOpen = true;
    }
    else if (Prefix.empty() || Candidates.empty())
    {
        bCommandSuggestionsOpen = false;
    }

    if (!bCommandSuggestionsOpen)
    {
        return;
    }

    const ImVec2 InputMin = ImGui::GetItemRectMin();
    const ImVec2 InputSize = ImGui::GetItemRectSize();
    const int32 MaxVisibleCandidates = 6;
    const int32 VisibleCandidateCount = (std::max)(1, (std::min)(MaxVisibleCandidates, static_cast<int32>(Candidates.size())));
    const float PopupHeight = ImGui::GetStyle().WindowPadding.y * 2.0f +
                              ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(VisibleCandidateCount);

    ImGui::SetNextWindowPos(ImVec2(InputMin.x, InputMin.y - PopupHeight - 4.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(InputSize.x, PopupHeight), ImGuiCond_Always);

    const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing;

    if (!ImGui::Begin("##OutputLogCommandSuggestions", nullptr, Flags))
    {
        ImGui::End();
        return;
    }

    for (int32 Index = 0; Index < VisibleCandidateCount; ++Index)
    {
        const FString& Candidate = Candidates[Index];
        const auto CommandIt = Commands.find(Candidate);
        const FString Usage = CommandIt != Commands.end() ? CommandIt->second.Usage : Candidate;
        const FString Description = CommandIt != Commands.end() ? CommandIt->second.Description : "";
        const FString Label = Usage + "##CommandSuggestion" + std::to_string(Index);

        if (ImGui::Selectable(Label.c_str(), false))
        {
            std::snprintf(InputBuf.data(), InputBuf.size(), "%s ", Candidate.c_str());
            bReclaimFocus = true;
            bCommandSuggestionsOpen = false;
        }
        if (!Description.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Description.c_str());
        }
    }

    const bool bSuggestionsHovered = ImGui::IsWindowHovered();
    ImGui::End();

    if (!bInputFocused && !bSuggestionsHovered)
    {
        bCommandSuggestionsOpen = false;
    }
}

FString FEditorOutputLogPanel::GetCurrentCommandPrefix() const
{
    const FString CommandLine(InputBuf.data());
    const size_t First = CommandLine.find_first_not_of(" \t");
    if (First == FString::npos)
    {
        return "";
    }

    const size_t Space = CommandLine.find_first_of(" \t", First);
    if (Space != FString::npos)
    {
        return "";
    }

    return CommandLine.substr(First);
}

TArray<FString> FEditorOutputLogPanel::BuildCommandCandidates(const FString& Prefix) const
{
    TArray<FString> Candidates;
    const FString LowerPrefix = ToLowerAsciiCopy(Prefix);
    for (const auto& Pair : Commands)
    {
        const FString LowerName = ToLowerAsciiCopy(Pair.first);
        if (LowerPrefix.empty() || StartsWith(LowerName, LowerPrefix))
        {
            Candidates.push_back(Pair.first);
        }
    }

    std::sort(Candidates.begin(), Candidates.end());
    return Candidates;
}

FString FEditorOutputLogPanel::BuildLogLine(const FLogEntry& Entry) const
{
    if (!Entry.FormattedMessage.empty())
    {
        return Entry.FormattedMessage;
    }

    FString Line = "[";
    Line += Entry.Category;
    Line += "] ";
    Line += Entry.Message;
    return Line;
}

void FEditorOutputLogPanel::CopyVisibleLogsToClipboard() const
{
    FString Text;
    if (LogBuffer)
    {
        for (const FLogEntry& Entry : LogBuffer->GetEntries())
        {
            if (!ShouldDisplayEntry(Entry.Level, Entry.Category, GetSearchableLogText(Entry)))
            {
                continue;
            }

            Text += BuildLogLine(Entry);
            Text += "\n";
        }
    }

    ImGui::SetClipboardText(Text.c_str());
}

void FEditorOutputLogPanel::CopyAllLogsToClipboard() const
{
    FString Text;
    if (LogBuffer)
    {
        for (const FLogEntry& Entry : LogBuffer->GetEntries())
        {
            Text += BuildLogLine(Entry);
            Text += "\n";
        }
    }

    ImGui::SetClipboardText(Text.c_str());
}

int32 FEditorOutputLogPanel::CountVisibleEntries() const
{
    if (!LogBuffer)
    {
        return 0;
    }

    int32 Count = 0;
    for (const FLogEntry& Entry : LogBuffer->GetEntries())
    {
        if (ShouldDisplayEntry(Entry.Level, Entry.Category, GetSearchableLogText(Entry)))
        {
            ++Count;
        }
    }
    return Count;
}

int32 FEditorOutputLogPanel::CountDisabledFilters() const
{
    int32 Count = 0;
    if (!bShowLogs)
    {
        ++Count;
    }
    if (!bShowWarnings)
    {
        ++Count;
    }
    if (!bShowErrors)
    {
        ++Count;
    }

    for (const auto& Pair : CategoryVisibility)
    {
        if (!Pair.second)
        {
            ++Count;
        }
    }

    return Count;
}

void FEditorOutputLogPanel::RegisterCommand(const FString& Name, CommandFn Fn, const FString& Usage, const FString& Description)
{
    Commands[Name] = { Fn, Usage, Description };
}

void FEditorOutputLogPanel::PrintHelp(const TArray<FString>& Args) const
{
    if (Args.size() >= 2)
    {
        auto It = Commands.find(Args[1]);
        if (It == Commands.end())
        {
            FString HelpText = "Unknown command: '";
            HelpText += Args[1];
            HelpText += "'\nType 'help' to list available commands.";
            UE_LOG(OutputLog, Warning, "%s", HelpText.c_str());
            return;
        }

        FString HelpText = It->second.Usage;
        HelpText += "\n  ";
        HelpText += It->second.Description;
        UE_LOG(OutputLog, Info, "%s", HelpText.c_str());
        return;
    }

    TArray<FString> Names;
    Names.reserve(Commands.size());
    for (const auto& Pair : Commands)
    {
        Names.push_back(Pair.first);
    }
    std::sort(Names.begin(), Names.end());

    FString HelpText = "Available commands:";
    for (const FString& Name : Names)
    {
        const FCommandEntry& Entry = Commands.at(Name);

        char Line[256];
        std::snprintf(Line, sizeof(Line), "\n  %-16s %s", Name.c_str(), Entry.Description.c_str());
        HelpText += Line;
    }
    HelpText += "\nUse 'help <command>' for details. Press Tab to autocomplete.";
    UE_LOG(OutputLog, Info, "%s", HelpText.c_str());
}

void FEditorOutputLogPanel::ExecCommand(const char* CommandLine)
{
    if (CommandLine == nullptr || CommandLine[0] == '\0')
    {
        return;
    }

    UE_LOG(OutputLog, Debug, "> %s", CommandLine);
    History.push_back(CommandLine);
    HistoryPos = -1;
    ScrollToBottom = true;

    TArray<FString> Tokens;
    std::istringstream Iss(CommandLine);
    FString Token;
    while (Iss >> Token)
    {
        Tokens.push_back(Token);
    }

    if (Tokens.empty())
    {
        return;
    }

    auto It = Commands.find(Tokens[0]);
    if (It != Commands.end())
    {
        It->second.Execute(Tokens);
    }
    else
    {
        UE_LOG(OutputLog, Error, "Unknown command: '%s'", Tokens[0].c_str());
        UE_LOG(OutputLog, Info, "Type 'help' to list available commands.");
    }
}

int32 FEditorOutputLogPanel::TextEditCallback(ImGuiInputTextCallbackData* Data)
{
    FEditorOutputLogPanel* OutputLog = static_cast<FEditorOutputLogPanel*>(Data->UserData);

    if (Data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        const int32 PrevPos = OutputLog->HistoryPos;
        if (Data->EventKey == ImGuiKey_UpArrow)
        {
            if (OutputLog->HistoryPos == -1)
            {
                OutputLog->HistoryPos = static_cast<int32>(OutputLog->History.size()) - 1;
            }
            else if (OutputLog->HistoryPos > 0)
            {
                OutputLog->HistoryPos--;
            }
        }
        else if (Data->EventKey == ImGuiKey_DownArrow)
        {
            if (OutputLog->HistoryPos != -1 && ++OutputLog->HistoryPos >= static_cast<int32>(OutputLog->History.size()))
            {
                OutputLog->HistoryPos = -1;
            }
        }

        if (PrevPos != OutputLog->HistoryPos)
        {
            const char* HistoryStr = (OutputLog->HistoryPos >= 0) ? OutputLog->History[OutputLog->HistoryPos].c_str() : "";
            Data->DeleteChars(0, Data->BufTextLen);
            Data->InsertChars(0, HistoryStr);
        }
    }

    if (Data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
    {
        const char* WordEnd = Data->Buf + Data->CursorPos;
        const char* WordStart = WordEnd;
        while (WordStart > Data->Buf && WordStart[-1] != ' ')
        {
            WordStart--;
        }

        const int32 WordStartOffset = static_cast<int32>(WordStart - Data->Buf);
        const int32 WordLength = static_cast<int32>(WordEnd - WordStart);
        const FString Prefix(WordStart, static_cast<size_t>(WordLength));

        TArray<FString> Candidates = OutputLog->BuildCommandCandidates(Prefix);

        if (Candidates.size() == 1)
        {
            FString Completion = Candidates[0] + " ";
            Data->DeleteChars(WordStartOffset, WordLength);
            Data->InsertChars(WordStartOffset, Completion.c_str());
        }
        else if (Candidates.size() > 1)
        {
            const FString CommonPrefix = FindCommonPrefix(Candidates);
            if (CommonPrefix.size() > Prefix.size())
            {
                Data->DeleteChars(WordStartOffset, WordLength);
                Data->InsertChars(WordStartOffset, CommonPrefix.c_str());
            }

            FString CompletionText = "Possible completions:";
            for (const FString& Candidate : Candidates)
            {
                const auto It = OutputLog->Commands.find(Candidate);
                const FString Usage = It != OutputLog->Commands.end() ? It->second.Usage : Candidate;
                CompletionText += "\n  ";
                CompletionText += Usage;
            }
            UE_LOG(OutputLog, Info, "%s", CompletionText.c_str());
            OutputLog->ScrollToBottom = true;
        }
        else
        {
            UE_LOG(OutputLog, Info, "No completion candidates. Type 'help' to list commands.");
            OutputLog->ScrollToBottom = true;
        }
    }

    return 0;
}
