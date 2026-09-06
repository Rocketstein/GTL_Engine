#pragma once
#include "Engine/GameFramework/GameMode/GameStateBase.h"

#include "Source/Game/GameMode/GameState.generated.h"

UENUM()
enum class EGamePhase : uint16 
{
	None		= 1 << 0,
	Playing		= 1 << 1,
	Paused		= 1 << 2,
	CutScene	= 1 << 3,
	Dead		= 1 << 4,
	Defeated	= 1 << 5,	// True death
	Victory		= 1 << 6,
	GameOver	= 1 << 7,
	Leaderboard	= 1 << 8,
};

UCLASS()
class AFinaleGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	EGamePhase Phase   = EGamePhase::None;
	uint16 ReviveCount = 0;
	float  ActiveTime  = 0;	// Tracks how much time has been elapsed since the game start, only counting the time when GamePhase == Playing

public:
	void	Tick(float DeltaTime) override;
	FString GetPhaseString();
	void    UpdateActiveTime(float DeltaTime);
};