#pragma once
#include "GameFramework/GameModeBase.h"
#include "Core/CoreTypes.h"

class AMapManager;

class ARunnerGameMode : public AGameModeBase
{
public:
	DECLARE_CLASS(ARunnerGameMode, AGameModeBase)

	ARunnerGameMode();

	void StartPlay() override;

private:
	AMapManager* MapManager = nullptr;
};

