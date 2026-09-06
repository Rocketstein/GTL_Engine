#include "AFinaleGameMode.h"
#include "GameState.h"
#include "Engine/Core/Logging/Log.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/Actor/TriggerVolumeBase.h"
#include "Engine/Object/Reflection/UClass.h"
#include "Engine/GameFramework/World.h"
#include "Engine/GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/BaseCombatCharacter.h"
#include "Component/Combat/HealthComponent.h"
#include "Engine/Runtime/Engine.h"

void AFinaleGameMode::SetGamePhase(EGamePhase InPhase)
{
	if (AFinaleGameState* State = Cast<AFinaleGameState>(GetGameState()))
	{
		State->Phase = InPhase;
		UE_LOG("[AFinaleGameMode] Game Phase set to: %s", GetGamePhaseString().c_str());
	}
}

bool AFinaleGameMode::CheckGamePhase(EGamePhase InPhase)
{
	if (AFinaleGameState* State = Cast<AFinaleGameState>(GetGameState()))
	{
		return State->Phase == InPhase;
	}

	return false;
}

FString AFinaleGameMode::GetGamePhaseString()
{
	if (AFinaleGameState* State = Cast<AFinaleGameState>(GetGameState()))
	{
		return State->GetPhaseString();
	}

	return "Unknown";
}

uint16 AFinaleGameMode::GetReviveCount() const
{
	if (AFinaleGameState* State = Cast<AFinaleGameState>(GetGameState()))
	{
		return State->ReviveCount;
	}

	return 0;
}

float AFinaleGameMode::GetActiveTime() const
{
	if (AFinaleGameState* State = Cast<AFinaleGameState>(GetGameState()))
	{
		return State->ActiveTime;
	}

	return 0;
}

AFinaleGameMode::AFinaleGameMode()
{
	GameStateClass = AFinaleGameState::StaticClass();
}

void AFinaleGameMode::StartMatch()
{
	AGameModeBase::StartMatch();
	UE_LOG("[AFinaleGameMode] Match Started");
	SetGamePhase(EGamePhase::Playing);
}

void AFinaleGameMode::TogglePause()
{
	if (CheckGamePhase(EGamePhase::Playing))     OnGamePaused();
	else if (CheckGamePhase(EGamePhase::Paused)) OnGameResumed();
}

void AFinaleGameMode::OnGamePaused()
{
	if (!CheckGamePhase(EGamePhase::Playing)) return;

	SetGamePhase(EGamePhase::Paused);

	if (UWorld* World = GetWorld())
	{
		World->SetPaused(true);
	}
}

// Called by player controller or an optional UI button
void AFinaleGameMode::OnGameResumed()
{
	if (!CheckGamePhase(EGamePhase::Paused)) return;

	if (UWorld* World = GetWorld())
	{
		World->SetPaused(false);
	}

	SetGamePhase(EGamePhase::Playing);
}

void AFinaleGameMode::OnEnteringCutscene()
{
	if (CheckGamePhase(EGamePhase::CutScene))
	{
		return;
	}

	// Block player input while the cinematic owns the camera.
	if (APlayerController* PC = GetPlayerController())
	{
		PC->DisableInput();
	}

	SetGamePhase(EGamePhase::CutScene);
}

void AFinaleGameMode::OnExitingCutscene()
{
	if (!CheckGamePhase(EGamePhase::CutScene))
	{
		return;
	}

	if (APlayerController* PC = GetPlayerController())
	{
		PC->EnableInput();
	}

	SetGamePhase(EGamePhase::Playing);
}

void AFinaleGameMode::OnGameQuit()
{
	// Unfreeze before leaving — the title scene spawns a fresh World, but
	// don't hand off a paused one.
	if (UWorld* World = GetWorld())
	{
		World->SetPaused(false);
		World->SetSoftPaused(false);
	}

	if (GEngine)
	{
		GEngine->RequestTransitionToScene("Game/GameTitle");
	}
}

void AFinaleGameMode::OnLeaderBoardView()
{
	if (!CheckGamePhase(EGamePhase::Defeated) && !CheckGamePhase(EGamePhase::Victory)) return;
	SetGamePhase(EGamePhase::Leaderboard);

}

void AFinaleGameMode::OnPlayerDeath()
{
	if (!CheckGamePhase(EGamePhase::Playing)) return;
	SetGamePhase(EGamePhase::Dead);

	// Mute player input during death. The World keeps ticking, so the death
	// animation + camera fade still play; only player control is suppressed.
	if (APlayerController* PC = GetPlayerController())
	{
		PC->DisableInput();
	}
}

void AFinaleGameMode::OnPlayerRevive()
{
	if (!CheckGamePhase(EGamePhase::Dead)) return;
	SetGamePhase(EGamePhase::Playing);
	if (AFinaleGameState* State = Cast<AFinaleGameState>(GetGameState()))
	{
		State->ReviveCount++;
	}

	// Hand control back to the player.
	if (APlayerController* PC = GetPlayerController())
	{
		PC->EnableInput();
	}
}

void AFinaleGameMode::OnPlayerDefeated()
{
	if (CheckGamePhase(EGamePhase::Playing))
	{
		OnPlayerDeath();
	}
	if (!CheckGamePhase(EGamePhase::Dead)) return;
	SetGamePhase(EGamePhase::Defeated);

	if (UWorld* World = GetWorld())
	{
		World->SetSoftPaused(true);
	}
}

// Boss slain itself is not a state — it's the event that resolves the run into
// Victory. Routes through OnVictory so the Playing -> Victory transition has a
// single owner (and a single gate).
void AFinaleGameMode::OnBossSlain(FName BossId)
{
	if (!CheckGamePhase(EGamePhase::Playing)) return;
	OnVictory();
}

void AFinaleGameMode::OnVictory()
{
	if (!CheckGamePhase(EGamePhase::Playing)) return;
	SetGamePhase(EGamePhase::Victory);
}

void AFinaleGameMode::OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn)
{
	if (!Trigger || !Pawn || !Pawn->IsPossessed()) return;

	const FName Tag = Trigger->GetTriggerTag();

	UE_LOG("[OnPossessedPawnEnteredTrigger] Trigger Box Invoked");

	if (Tag == FName("TrueDeath"))
	{
		// Player-controlled pawn -> game-over flow; AI enemy -> just kill the actor.
		if (Pawn->GetPlayerController() != nullptr)
		{
			UE_LOG("[KillVolume] Player fell -> defeat");
			OnPlayerDefeated();
		}
		else if (ABaseCombatCharacter* Combatant = Cast<ABaseCombatCharacter>(Pawn))
		{
			if (UHealthComponent* HP = Combatant->GetHealthComponent())
			{
				UE_LOG("[KillVolume] Enemy %s fell -> kill", Pawn->GetName().c_str());
				HP->Kill();
			}
		}
		return;
	}
}

void AFinaleGameMode::OnPossessedPawnExitedTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn)
{
	// Does nothing for now
}
