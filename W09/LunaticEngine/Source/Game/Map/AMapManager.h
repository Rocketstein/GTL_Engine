#pragma once
#include "GameFramework/AActor.h"
#include "AMapChunk.h"
#include "Game/GameActors/Enemy/GimmickManager.h"

class AObstacleActorBase;

class AMapManager : public AActor
{
public:
	DECLARE_CLASS(AMapManager, AActor)

	AMapManager();

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void EndPlay() override;

	void Initialize(AActor* InPlayer);
	void ResetMap();
	void SetEnabled(bool bInEnabled);
	void SetPlayerActor(AActor* InPlayer);
	void SetObstacleSpawnRate(float InSpawnRate) { ObstacleSpawnRate = InSpawnRate; }
	void SetGimmickSpawnChance(float InSpawnRate) { GimmickSpawnChance = InSpawnRate; }

private:
	void  BuildTemplateLibrary();
	void  SpawnNextChunk(bool Init = false);
	void  DespawnFrontChunk();
	int32 SelectNextTemplateIndex();
	void  TrySpawnGimmickAtChunkEnd();
	TArray<AObstacleActorBase*> GatherNearbyObstacleCandidates() const;

	TArray<FMapChunkTemplate> Templates;
	TArray<AMapChunk*>        ActiveChunks;
	FGimmickManager GimmickManager;
	AActor* Player = nullptr;
	bool bEnabled = true;

	int32 StraightRunLength = 0;
	int32 MinStraightsBetweenTurns = 2;
	int32 TargetChunkCount = 7;
	float ChunkBuggedRate	 = 0.1f;
	float ObstacleSpawnRate  = 0.55f;
	float GimmickSpawnChance = 0.4f;
	float GimmickTargetSearchDistance = 60.0f;
};
