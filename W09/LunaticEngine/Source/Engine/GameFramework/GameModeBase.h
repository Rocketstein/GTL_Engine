#pragma once
#include "AActor.h"
#include "Core/CoreTypes.h"

class APawnActor;
class APlayerController;
class APlayerCameraManager;

class AGameModeBase : public AActor
{
public:
	DECLARE_CLASS(AGameModeBase, AActor);

	// from StartPlay DefaultPawn / PlayerController spawn, Possess actor
	virtual void StartPlay();

	// subclass consturctor default value( DefaultPawnClass / PlayerControllerClass)
	FString DefaultPawnClassName = "APawnActor";
	FString PlayerControllerClassName = "APlayerController";
	FString PlayerCameraManagerClassName = "APlayerCameraManager";

	APawnActor* GetSpawnedPawn() const { return SpawnedPawn; }
	APlayerController* GetSpawnedController() const { return SpawnedController; }
	APlayerCameraManager* GetPlayerCameraManager() const { return CameraManager; }

protected:
	APawnActor* SpawnedPawn = nullptr;
	APlayerController* SpawnedController = nullptr;
	APlayerCameraManager* CameraManager = nullptr;
};
