#pragma once
#include "Engine/GameFramework/GameMode/GameModeBase.h"

#include "Source/Game/GameMode/AFinaleGameMode.generated.h"

enum class EGamePhase : uint16;

UCLASS()
class AFinaleGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AFinaleGameMode();
	void StartMatch() override;
	void OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn) override;
	void OnPossessedPawnExitedTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn)  override;

	void TogglePause();
	void OnGamePaused();
	void OnGameResumed();
	void OnEnteringCutscene();
	void OnExitingCutscene();
	void OnGameQuit();			// Quit means returned to title here
	void OnLeaderBoardView();
	void OnPlayerDeath();
	void OnPlayerRevive();
	void OnPlayerDefeated();	// True death
	void OnBossSlain(FName BossId);
	void OnVictory();

	// Returns the number of times the user exploited revive in this play
	uint16 GetReviveCount() const;
	float  GetActiveTime()  const;

private:
	void SetGamePhase(EGamePhase InPhase);
	bool CheckGamePhase(EGamePhase InPase);
	FString GetGamePhaseString();

};