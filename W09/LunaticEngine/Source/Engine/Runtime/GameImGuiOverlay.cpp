#include "Engine/Runtime/GameImGuiOverlay.h"

#include "Audio/AudioManager.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/AsciiUtils.h"
#include "UI/SWindow.h"
#include "Render/Pipeline/Renderer.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "SimpleJSON/json.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
	constexpr ImVec4 ScorePopupSurface = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
	constexpr ImVec4 ScorePopupTitle = ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
	constexpr ImVec4 ScorePopupBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
	constexpr ImVec4 ScorePopupDim = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

	constexpr ImVec4 ScoreboardWindowBg = ImVec4(10.0f / 255.0f, 16.0f / 255.0f, 14.0f / 255.0f, 0.98f);
	constexpr ImVec4 ScoreboardTitleBg = ImVec4(16.0f / 255.0f, 27.0f / 255.0f, 24.0f / 255.0f, 1.0f);
	constexpr ImVec4 ScoreboardBorder = ImVec4(42.0f / 255.0f, 98.0f / 255.0f, 78.0f / 255.0f, 0.85f);
	constexpr ImVec4 ScoreboardPanel = ImVec4(12.0f / 255.0f, 21.0f / 255.0f, 18.0f / 255.0f, 0.96f);
	constexpr ImVec4 ScoreboardPanelAlt = ImVec4(15.0f / 255.0f, 29.0f / 255.0f, 25.0f / 255.0f, 0.96f);
	constexpr ImVec4 ScoreboardAccent = ImVec4(112.0f / 255.0f, 241.0f / 255.0f, 170.0f / 255.0f, 1.0f);
	constexpr ImVec4 ScoreboardAccentSoft = ImVec4(112.0f / 255.0f, 241.0f / 255.0f, 170.0f / 255.0f, 0.25f);
	constexpr ImVec4 ScoreboardText = ImVec4(214.0f / 255.0f, 239.0f / 255.0f, 226.0f / 255.0f, 1.0f);
	constexpr ImVec4 ScoreboardTextDim = ImVec4(129.0f / 255.0f, 171.0f / 255.0f, 153.0f / 255.0f, 1.0f);
	constexpr ImVec4 ScoreboardWarning = ImVec4(255.0f / 255.0f, 145.0f / 255.0f, 102.0f / 255.0f, 1.0f);
	constexpr ImVec4 ScoreboardDanger = ImVec4(255.0f / 255.0f, 97.0f / 255.0f, 97.0f / 255.0f, 1.0f);
	constexpr ImVec4 CreditsAccent = ImVec4(151.0f / 255.0f, 210.0f / 255.0f, 1.0f, 1.0f);

	ImVec4 GetScoreboardRankColor(size_t Index)
	{
		if (Index == 0)
		{
			return ImVec4(255.0f / 255.0f, 209.0f / 255.0f, 102.0f / 255.0f, 1.0f);
		}

		if (Index == 1)
		{
			return ImVec4(186.0f / 255.0f, 204.0f / 255.0f, 214.0f / 255.0f, 1.0f);
		}

		if (Index == 2)
		{
			return ImVec4(218.0f / 255.0f, 158.0f / 255.0f, 107.0f / 255.0f, 1.0f);
		}

		return ScoreboardText;
	}
}

bool FGameImGuiOverlay::IsAlphabetCharacter(char Character)
{
	return (Character >= 'A' && Character <= 'Z') || (Character >= 'a' && Character <= 'z');
}

void FGameImGuiOverlay::Initialize(FWindowsWindow* InWindow, FRenderer& InRenderer)
{
	if (bInitialized || !InWindow)
	{
		return;
	}

	ImGui::CreateContext();
	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.IniFilename = nullptr;
	ImGui::StyleColorsDark();

	const FString FontPath = FResourceManager::Get().ResolvePath(FName("Default.Font.UI"));
	const std::filesystem::path UIFontPath = std::filesystem::path(FPaths::RootDir()) / FPaths::ToWide(FontPath);
	const FString UIFontPathAbsolute = FPaths::ToUtf8(UIFontPath.lexically_normal().wstring());
	if (std::filesystem::exists(UIFontPath))
	{
		KoreanFont = IO.Fonts->AddFontFromFileTTF(UIFontPathAbsolute.c_str(), 20.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
	}

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(
		InRenderer.GetFD3DDevice().GetDevice(),
		InRenderer.GetFD3DDevice().GetDeviceContext());

	bInitialized = true;
}

void FGameImGuiOverlay::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	KoreanFont = nullptr;
	ScoreSavePopup = FScoreSavePopupState();
	MessagePopup = FMessagePopupState();
	ScoreboardPopup = FScoreboardPopupState();
	OptionsPopup = FOptionsPopupState();
	CreditsPopup = FCreditsPopupState();
	bInitialized = false;
}

void FGameImGuiOverlay::Render(FRenderer& InRenderer)
{
	(void)InRenderer;
	if (!bInitialized)
	{
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	RenderScoreSavePopup();
	RenderMessagePopup();
	RenderScoreboardPopup();
	RenderOptionsPopup();
	RenderCreditsPopup();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FGameImGuiOverlay::RenderWithinCurrentFrame(const FRect* AnchorRect)
{
	RenderScoreSavePopup(AnchorRect);
	RenderMessagePopup(AnchorRect);
	RenderScoreboardPopup(AnchorRect);
	RenderOptionsPopup(AnchorRect);
	RenderCreditsPopup(AnchorRect);
}

void FGameImGuiOverlay::OpenScoreSavePopup(int32 InScore)
{
	ScoreSavePopup.Score = InScore;
	ScoreSavePopup.bSubmitted = false;
	ScoreSavePopup.PendingNickname.clear();
	ScoreSavePopup.bRequestOpen = true;
	ScoreSavePopup.bPopupOpen = true;
	ScoreSavePopup.bFocusInput = true;
	ResetNicknameBuffer();
}

bool FGameImGuiOverlay::ConsumeScoreSavePopupResult(FString& OutNickname)
{
	if (!ScoreSavePopup.bSubmitted)
	{
		return false;
	}

	OutNickname = ScoreSavePopup.PendingNickname;
	ScoreSavePopup.PendingNickname.clear();
	ScoreSavePopup.bSubmitted = false;
	return !OutNickname.empty();
}

void FGameImGuiOverlay::OpenMessagePopup(const FString& InMessage)
{
	MessagePopup.Message = InMessage;
	MessagePopup.bConfirmed = false;
	MessagePopup.bRequestOpen = true;
	MessagePopup.bPopupOpen = true;
}

bool FGameImGuiOverlay::ConsumeMessagePopupConfirmed()
{
	if (!MessagePopup.bConfirmed)
	{
		return false;
	}

	MessagePopup.bConfirmed = false;
	return true;
}

void FGameImGuiOverlay::OpenScoreboardPopup(const FString& InFilePath)
{
	ScoreboardPopup.FilePath = InFilePath;
	ScoreboardPopup.ErrorMessage.clear();
	ScoreboardPopup.Entries.clear();
	ScoreboardPopup.bRequestOpen = true;
	ScoreboardPopup.bPopupOpen = true;
}

void FGameImGuiOverlay::OpenTitleOptionsPopup()
{
	OptionsPopup.bRequestOpen = true;
	OptionsPopup.bPopupOpen = true;
}

void FGameImGuiOverlay::OpenTitleCreditsPopup()
{
	CreditsPopup.bRequestOpen = true;
	CreditsPopup.bPopupOpen = true;
}

bool FGameImGuiOverlay::IsScoreSavePopupOpen() const
{
	return
		ScoreSavePopup.bPopupOpen ||
		ScoreSavePopup.bRequestOpen ||
		MessagePopup.bPopupOpen ||
		MessagePopup.bRequestOpen ||
		ScoreboardPopup.bPopupOpen ||
		ScoreboardPopup.bRequestOpen ||
		OptionsPopup.bPopupOpen ||
		OptionsPopup.bRequestOpen ||
		CreditsPopup.bPopupOpen ||
		CreditsPopup.bRequestOpen;
}

void FGameImGuiOverlay::ResetNicknameBuffer()
{
	std::memset(ScoreSavePopup.NicknameBuffer, 0, sizeof(ScoreSavePopup.NicknameBuffer));
}

void FGameImGuiOverlay::SanitizeNicknameBuffer()
{
	char Sanitized[7] = {};
	int32 WriteIndex = 0;
	for (int32 ReadIndex = 0; ReadIndex < 6 && ScoreSavePopup.NicknameBuffer[ReadIndex] != '\0'; ++ReadIndex)
	{
		char Character = ScoreSavePopup.NicknameBuffer[ReadIndex];
		if (!IsAlphabetCharacter(Character))
		{
			continue;
		}

		Sanitized[WriteIndex++] = AsciiUtils::ToUpper(Character);
		if (WriteIndex >= 6)
		{
			break;
		}
	}

	std::memcpy(ScoreSavePopup.NicknameBuffer, Sanitized, sizeof(Sanitized));
}

void FGameImGuiOverlay::RenderScoreSavePopup(const FRect* AnchorRect)
{
	if (ScoreSavePopup.bRequestOpen)
	{
		ImGui::OpenPopup("Save Score");
		ScoreSavePopup.bRequestOpen = false;
		ScoreSavePopup.bPopupOpen = true;
		ScoreSavePopup.bFocusInput = true;
	}

	ImVec2 PopupCenter;
	if (AnchorRect && AnchorRect->Width > 0.0f && AnchorRect->Height > 0.0f)
	{
		PopupCenter = ImVec2(
			AnchorRect->X + AnchorRect->Width * 0.5f,
			AnchorRect->Y + AnchorRect->Height * 0.5f);
	}
	else
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		PopupCenter = MainViewport->GetCenter();
	}

	ImGui::SetNextWindowPos(PopupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ScorePopupSurface);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_Border, ScorePopupBorder);
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ScorePopupDim);

	if (ImGui::BeginPopupModal("Save Score", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Final Score: %d", ScoreSavePopup.Score);
		ImGui::Spacing();
		if (ScoreSavePopup.bFocusInput)
		{
			ImGui::SetKeyboardFocusHere();
			ScoreSavePopup.bFocusInput = false;
		}

		ImGui::InputText("##Nickname", ScoreSavePopup.NicknameBuffer, IM_ARRAYSIZE(ScoreSavePopup.NicknameBuffer));
		SanitizeNicknameBuffer();
		ImGui::TextDisabled("Alphabet only, max 6 characters");
		ImGui::Spacing();

		const bool bCanSave = ScoreSavePopup.NicknameBuffer[0] != '\0';
		if (!bCanSave)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Save", ImVec2(140.0f, 0.0f)))
		{
			ScoreSavePopup.PendingNickname = ScoreSavePopup.NicknameBuffer;
			ScoreSavePopup.bSubmitted = true;
			ScoreSavePopup.bPopupOpen = false;
			ResetNicknameBuffer();
			ImGui::CloseCurrentPopup();
		}

		if (!bCanSave)
		{
			ImGui::EndDisabled();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(140.0f, 0.0f)))
		{
			ScoreSavePopup.bPopupOpen = false;
			ResetNicknameBuffer();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(6);
	ImGui::PopStyleVar(2);

	if (ScoreSavePopup.bPopupOpen && !ImGui::IsPopupOpen("Save Score"))
	{
		ScoreSavePopup.bPopupOpen = false;
		ResetNicknameBuffer();
	}
}

void FGameImGuiOverlay::RenderMessagePopup(const FRect* AnchorRect)
{
	if (MessagePopup.bRequestOpen)
	{
		ImGui::OpenPopup("Notice");
		MessagePopup.bRequestOpen = false;
		MessagePopup.bPopupOpen = true;
	}

	ImVec2 PopupCenter;
	if (AnchorRect && AnchorRect->Width > 0.0f && AnchorRect->Height > 0.0f)
	{
		PopupCenter = ImVec2(
			AnchorRect->X + AnchorRect->Width * 0.5f,
			AnchorRect->Y + AnchorRect->Height * 0.5f);
	}
	else
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		PopupCenter = MainViewport->GetCenter();
	}

	ImGui::SetNextWindowPos(PopupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ScorePopupSurface);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_Border, ScorePopupBorder);
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ScorePopupDim);

	if (ImGui::BeginPopupModal("Notice", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("%s", MessagePopup.Message.c_str());
		ImGui::Spacing();

		if (ImGui::Button("OK", ImVec2(140.0f, 0.0f)))
		{
			MessagePopup.bConfirmed = true;
			MessagePopup.bPopupOpen = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(6);
	ImGui::PopStyleVar(2);

	if (MessagePopup.bPopupOpen && !ImGui::IsPopupOpen("Notice"))
	{
		MessagePopup.bPopupOpen = false;
	}
}

void FGameImGuiOverlay::LoadScoreboardEntries()
{
	ScoreboardPopup.ErrorMessage.clear();
	ScoreboardPopup.Entries.clear();

	const std::filesystem::path InputPath = FPaths::ToWide(ScoreboardPopup.FilePath);
	const std::filesystem::path AbsolutePath = InputPath.is_absolute()
		? InputPath.lexically_normal()
		: (std::filesystem::path(FPaths::RootDir()) / InputPath).lexically_normal();

	std::ifstream File(AbsolutePath);
	if (!File.is_open())
	{
		ScoreboardPopup.ErrorMessage = "Scoreboard file not found.";
		return;
	}

	const FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	if (Content.empty())
	{
		ScoreboardPopup.ErrorMessage = "Scoreboard file is empty.";
		return;
	}

	json::JSON Root = json::JSON::Load(Content);
	json::JSON EntriesJson = Root.hasKey("entries") ? Root["entries"] : Root;
	if (EntriesJson.JSONType() != json::JSON::Class::Array)
	{
		ScoreboardPopup.ErrorMessage = "Scoreboard data is invalid.";
		return;
	}

	for (auto& EntryJson : EntriesJson.ArrayRange())
	{
		FScoreboardEntry Entry;
		Entry.Nickname = EntryJson.hasKey("nickname") ? EntryJson["nickname"].ToString() : "";
		Entry.Score = EntryJson.hasKey("score") ? EntryJson["score"].ToInt() : 0;
		Entry.Logs = EntryJson.hasKey("logs") ? EntryJson["logs"].ToInt() : 0;
		Entry.HotfixCount = EntryJson.hasKey("hotfix_count") ? EntryJson["hotfix_count"].ToInt() : 0;
		Entry.CrashDumpAnalysisCount = EntryJson.hasKey("crash_dump_analysis_count") ? EntryJson["crash_dump_analysis_count"].ToInt() : 0;
		Entry.MaxDepthM = EntryJson.hasKey("max_depth_m") ? EntryJson["max_depth_m"].ToInt() : 0;
		Entry.CoachRank = EntryJson.hasKey("coach_rank") ? EntryJson["coach_rank"].ToString() : "";
		ScoreboardPopup.Entries.push_back(std::move(Entry));
	}
}

void FGameImGuiOverlay::RenderScoreboardPopup(const FRect* AnchorRect)
{
	if (ScoreboardPopup.bRequestOpen)
	{
		LoadScoreboardEntries();
		ImGui::OpenPopup("Scoreboard");
		ScoreboardPopup.bRequestOpen = false;
		ScoreboardPopup.bPopupOpen = true;
	}

	ImVec2 PopupCenter;
	if (AnchorRect && AnchorRect->Width > 0.0f && AnchorRect->Height > 0.0f)
	{
		PopupCenter = ImVec2(
			AnchorRect->X + AnchorRect->Width * 0.5f,
			AnchorRect->Y + AnchorRect->Height * 0.5f);
	}
	else
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		PopupCenter = MainViewport->GetCenter();
	}

	ImGui::SetNextWindowPos(PopupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(960.0f, 560.0f), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ScoreboardWindowBg);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, ScoreboardTitleBg);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ScoreboardTitleBg);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ScoreboardTitleBg);
	ImGui::PushStyleColor(ImGuiCol_Border, ScoreboardBorder);
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ScorePopupDim);
	ImGui::PushStyleColor(ImGuiCol_Text, ScoreboardText);
	ImGui::PushStyleColor(ImGuiCol_TextDisabled, ScoreboardTextDim);
	ImGui::PushStyleColor(ImGuiCol_Button, ScoreboardPanelAlt);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(24.0f / 255.0f, 51.0f / 255.0f, 42.0f / 255.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(32.0f / 255.0f, 68.0f / 255.0f, 56.0f / 255.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ScoreboardPanel);
	ImGui::PushStyleColor(ImGuiCol_Header, ScoreboardAccentSoft);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(112.0f / 255.0f, 241.0f / 255.0f, 170.0f / 255.0f, 0.35f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(112.0f / 255.0f, 241.0f / 255.0f, 170.0f / 255.0f, 0.45f));
	ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ScoreboardBorder);
	ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ScoreboardAccentSoft);
	ImGui::PushStyleColor(ImGuiCol_TableRowBg, ScoreboardPanel);
	ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ScoreboardPanelAlt);

	bool bKeepOpen = true;
	if (ImGui::BeginPopupModal("Scoreboard", &bKeepOpen, ImGuiWindowFlags_NoResize))
	{
		bool bShouldClose = !bKeepOpen;

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ScoreboardTitleBg);
		if (ImGui::BeginChild("##ScoreboardHeader", ImVec2(0.0f, 82.0f), true, ImGuiWindowFlags_NoScrollbar))
		{
			ImGui::TextColored(ScoreboardAccent, "> scoreboard.exe");
			ImGui::TextColored(ScoreboardTextDim, "[runtime] displaying top dive operators");

			const float CloseButtonWidth = 38.0f;
			ImGui::SameLine(ImGui::GetWindowWidth() - CloseButtonWidth - 12.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, ScoreboardDanger);
			if (ImGui::Button("X", ImVec2(CloseButtonWidth, 28.0f)))
			{
				bShouldClose = true;
			}
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::TextColored(ScoreboardTextDim, "root@enginedive:~$ cat /saves/scoreboard.json | sort --numeric --reverse");
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		ImGui::Spacing();

		if (!ScoreboardPopup.ErrorMessage.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ScoreboardWarning);
			ImGui::TextWrapped("[error] %s", ScoreboardPopup.ErrorMessage.c_str());
			ImGui::PopStyleColor();
		}
		else if (ScoreboardPopup.Entries.empty())
		{
			ImGui::TextColored(ScoreboardTextDim, "[log] no saved scores found.");
		}
		else
		{
			if (ImGui::BeginChild("##ScoreboardBody", ImVec2(0.0f, -52.0f), true))
			{
				ImGui::TextColored(ScoreboardAccent, "TOTAL_ENTRIES=%d", static_cast<int32>(ScoreboardPopup.Entries.size()));
				ImGui::Separator();

				if (ImGui::BeginTable("##ScoreboardTable", 7, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp))
				{
					ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 54.0f);
					ImGui::TableSetupColumn("HANDLE", ImGuiTableColumnFlags_WidthFixed, 110.0f);
					ImGui::TableSetupColumn("SCORE", ImGuiTableColumnFlags_WidthFixed, 110.0f);
					ImGui::TableSetupColumn("LOG", ImGuiTableColumnFlags_WidthFixed, 72.0f);
					ImGui::TableSetupColumn("PATCH", ImGuiTableColumnFlags_WidthFixed, 84.0f);
					ImGui::TableSetupColumn("CRASH", ImGuiTableColumnFlags_WidthFixed, 84.0f);
					ImGui::TableSetupColumn("DEPTH / RANK");
					ImGui::TableHeadersRow();

					for (size_t Index = 0; Index < ScoreboardPopup.Entries.size(); ++Index)
					{
						const FScoreboardEntry& Entry = ScoreboardPopup.Entries[Index];
						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::TextColored(GetScoreboardRankColor(Index), "#%02d", static_cast<int>(Index + 1));

						ImGui::TableSetColumnIndex(1);
						ImGui::Text("> %s", Entry.Nickname.c_str());

						ImGui::TableSetColumnIndex(2);
						ImGui::TextColored(ScoreboardAccent, "%d", Entry.Score);

						ImGui::TableSetColumnIndex(3);
						ImGui::Text("%d", Entry.Logs);

						ImGui::TableSetColumnIndex(4);
						ImGui::Text("%d", Entry.HotfixCount);

						ImGui::TableSetColumnIndex(5);
						ImGui::Text("%d", Entry.CrashDumpAnalysisCount);

						ImGui::TableSetColumnIndex(6);
						ImGui::Text("%dm  |  %s", Entry.MaxDepthM, Entry.CoachRank.c_str());
					}

					ImGui::EndTable();
				}
			}

			ImGui::EndChild();
		}

		ImGui::Spacing();
		ImGui::TextColored(ScoreboardTextDim, "press X or CLOSE_SESSION to dismiss");
		ImGui::SameLine();
		if (ImGui::Button("CLOSE_SESSION", ImVec2(180.0f, 0.0f)))
		{
			bShouldClose = true;
		}

		if (bShouldClose)
		{
			ScoreboardPopup.bPopupOpen = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(19);
	ImGui::PopStyleVar(6);

	if (ScoreboardPopup.bPopupOpen && !ImGui::IsPopupOpen("Scoreboard"))
	{
		ScoreboardPopup.bPopupOpen = false;
	}
}

void FGameImGuiOverlay::RenderOptionsPopup(const FRect* AnchorRect)
{
	if (OptionsPopup.bRequestOpen)
	{
		ImGui::OpenPopup("Options");
		OptionsPopup.bRequestOpen = false;
		OptionsPopup.bPopupOpen = true;
	}

	ImVec2 PopupCenter;
	if (AnchorRect && AnchorRect->Width > 0.0f && AnchorRect->Height > 0.0f)
	{
		PopupCenter = ImVec2(
			AnchorRect->X + AnchorRect->Width * 0.5f,
			AnchorRect->Y + AnchorRect->Height * 0.5f);
	}
	else
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		PopupCenter = MainViewport->GetCenter();
	}

	float SoundVolume = FAudioManager::Get().GetCategoryVolume(ESoundCategory::SFX);
	float BGMVolume = FAudioManager::Get().GetCategoryVolume(ESoundCategory::Background);

	ImGui::SetNextWindowPos(PopupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ScorePopupSurface);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_Border, ScorePopupBorder);
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ScorePopupDim);

	if (ImGui::BeginPopupModal("Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Audio");
		ImGui::Spacing();

		if (ImGui::SliderFloat("Sound", &SoundVolume, 0.0f, 1.0f, "%.2f"))
		{
			FAudioManager::Get().SetCategoryVolume(ESoundCategory::SFX, SoundVolume);
		}

		if (ImGui::SliderFloat("BGM", &BGMVolume, 0.0f, 1.0f, "%.2f"))
		{
			FAudioManager::Get().SetCategoryVolume(ESoundCategory::Background, BGMVolume);
		}

		ImGui::Spacing();
		if (ImGui::Button("Close", ImVec2(140.0f, 0.0f)))
		{
			OptionsPopup.bPopupOpen = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(6);
	ImGui::PopStyleVar(2);

	if (OptionsPopup.bPopupOpen && !ImGui::IsPopupOpen("Options"))
	{
		OptionsPopup.bPopupOpen = false;
	}
}

void FGameImGuiOverlay::RenderCreditsPopup(const FRect* AnchorRect)
{
	if (CreditsPopup.bRequestOpen)
	{
		ImGui::OpenPopup("Credits");
		CreditsPopup.bRequestOpen = false;
		CreditsPopup.bPopupOpen = true;
	}

	ImVec2 PopupCenter;
	if (AnchorRect && AnchorRect->Width > 0.0f && AnchorRect->Height > 0.0f)
	{
		PopupCenter = ImVec2(
			AnchorRect->X + AnchorRect->Width * 0.5f,
			AnchorRect->Y + AnchorRect->Height * 0.5f);
	}
	else
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		PopupCenter = MainViewport->GetCenter();
	}

	ImGui::SetNextWindowPos(PopupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ScorePopupSurface);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_Border, ScorePopupBorder);
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ScorePopupDim);

	if (ImGui::BeginPopupModal("Credits", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextColored(CreditsAccent, "6Team");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (KoreanFont)
		{
			ImGui::PushFont(KoreanFont);
		}

		ImGui::TextUnformatted("김연하");
		ImGui::TextUnformatted("김형준");
		ImGui::TextUnformatted("강건우");
		ImGui::TextUnformatted("조상현");

		if (KoreanFont)
		{
			ImGui::PopFont();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Close", ImVec2(140.0f, 0.0f)))
		{
			CreditsPopup.bPopupOpen = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(6);
	ImGui::PopStyleVar(3);

	if (CreditsPopup.bPopupOpen && !ImGui::IsPopupOpen("Credits"))
	{
		CreditsPopup.bPopupOpen = false;
	}
}
