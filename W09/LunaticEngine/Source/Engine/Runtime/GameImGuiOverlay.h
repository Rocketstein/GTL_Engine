#pragma once

#include "Core/CoreTypes.h"

class FWindowsWindow;
class FRenderer;
struct FRect;
struct ImFont;

class FGameImGuiOverlay
{
public:
	void Initialize(FWindowsWindow* InWindow, FRenderer& InRenderer);
	void Shutdown();
	void Render(FRenderer& InRenderer);
	void RenderWithinCurrentFrame(const FRect* AnchorRect = nullptr);

	void OpenScoreSavePopup(int32 InScore);
	bool ConsumeScoreSavePopupResult(FString& OutNickname);
	void OpenMessagePopup(const FString& InMessage);
	bool ConsumeMessagePopupConfirmed();
	void OpenScoreboardPopup(const FString& InFilePath);
	void OpenTitleOptionsPopup();
	void OpenTitleCreditsPopup();
	bool IsScoreSavePopupOpen() const;

private:
	struct FScoreSavePopupState
	{
		bool bPopupOpen = false;
		bool bRequestOpen = false;
		bool bSubmitted = false;
		bool bFocusInput = false;
		int32 Score = 0;
		FString PendingNickname;
		char NicknameBuffer[7] = {};
	};

	struct FMessagePopupState
	{
		bool bPopupOpen = false;
		bool bRequestOpen = false;
		bool bConfirmed = false;
		FString Message;
	};

	struct FScoreboardEntry
	{
		FString Nickname;
		int32 Score = 0;
		int32 Logs = 0;
		int32 HotfixCount = 0;
		int32 CrashDumpAnalysisCount = 0;
		int32 MaxDepthM = 0;
		FString CoachRank;
	};

	struct FScoreboardPopupState
	{
		bool bPopupOpen = false;
		bool bRequestOpen = false;
		FString FilePath;
		FString ErrorMessage;
		TArray<FScoreboardEntry> Entries;
	};

	struct FOptionsPopupState
	{
		bool bPopupOpen = false;
		bool bRequestOpen = false;
	};

	struct FCreditsPopupState
	{
		bool bPopupOpen = false;
		bool bRequestOpen = false;
	};

	static bool IsAlphabetCharacter(char Character);
	void ResetNicknameBuffer();
	void SanitizeNicknameBuffer();
	void RenderScoreSavePopup(const FRect* AnchorRect = nullptr);
	void RenderMessagePopup(const FRect* AnchorRect = nullptr);
	void LoadScoreboardEntries();
	void RenderScoreboardPopup(const FRect* AnchorRect = nullptr);
	void RenderOptionsPopup(const FRect* AnchorRect = nullptr);
	void RenderCreditsPopup(const FRect* AnchorRect = nullptr);

private:
	bool bInitialized = false;
	ImFont* KoreanFont = nullptr;
	FScoreSavePopupState ScoreSavePopup;
	FMessagePopupState MessagePopup;
	FScoreboardPopupState ScoreboardPopup;
	FOptionsPopupState OptionsPopup;
	FCreditsPopupState CreditsPopup;
};
