#include "ARunnerGameMode.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Game/Map/AMapManager.h"
#include "GameFramework/PawnActor.h"
#include "GameFramework/World.h"
#include "Core/Log.h"

IMPLEMENT_CLASS(ARunnerGameMode, AGameModeBase)

ARunnerGameMode::ARunnerGameMode()
{
	DefaultPawnClassName = "ARunner";
	PlayerControllerClassName = "APlayerController";
}

void ARunnerGameMode::StartPlay()
{
	AGameModeBase::StartPlay();

	APawnActor* PlayerPawn = GetSpawnedPawn();
	if (!PlayerPawn)
	{
		UE_LOG("[RunnerGameMode] PlayerPawn missing after AGameModeBase::StartPlay");
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG("[RunnerGameMode] World missing. MapManager spawn skipped");
		return;
	}

	MapManager = World->SpawnActor<AMapManager>();
	if (!MapManager)
	{
		UE_LOG("[RunnerGameMode] MapManager spawn failed");
		return;
	}

	//CameraManager = GetPlayerCameraManager();
	//if (!CameraManager) 
	//{
	//	UE_LOG("[RunnerGameMode] PlayerCameraManager spawn failed");
	//	return;
	//}

	MapManager->Initialize(PlayerPawn);
}
