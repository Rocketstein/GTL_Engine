#pragma once

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/GameImGuiOverlay.h"

class UGameEngine : public UEngine
{
public:
	DECLARE_CLASS(UGameEngine, UEngine)

	UGameEngine() = default;
	~UGameEngine() override = default;

	void Init(FWindowsWindow* InWindow) override;
	void Shutdown() override;
	void BeginPlay() override;
	bool LoadScene(const FString& InSceneReference) override;
	void Tick(float DeltaTime) override;
	void RenderImGuiOverlay(FRenderer& InRenderer) override;

	void OpenScoreSavePopup(int32 InScore) override;
	bool ConsumeScoreSavePopupResult(FString& OutNickname) override;
	void OpenMessagePopup(const FString& InMessage) override;
	bool ConsumeMessagePopupConfirmed() override;
	void OpenScoreboardPopup(const FString& InFilePath) override;
	void OpenTitleOptionsPopup() override;
	void OpenTitleCreditsPopup() override;
	bool IsScoreSavePopupOpen() const override;

protected:
	void LoadStartLevel();

private:
	FGameImGuiOverlay ImGuiOverlay;
};
