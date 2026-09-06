#pragma once
#include "GameFramework/AActor.h"
#include "Game/Map/MapChunkTemplate.h"

class USceneComponent;
class AItemActorBase;

class AMapChunk : public AActor {
public:
	DECLARE_CLASS(AMapChunk, AActor)

	void BeginPlay() override;
	void EndPlay()	 override;
	void InitFromTemplate(const FMapChunkTemplate& InTemplate, float InChunkBuggedRate);
	void SetChunkBuggedRate(float InRate) { ChunkBuggedRate = InRate; }

	FVector    GetExitLocation() const;
	FRotator   GetExitRotation() const;
	float      GetChunkLength()  const { return Template.Length; }
	EChunkType GetChunkType()    const { return Template.ChunkType; }
	TArray<AObstacleActorBase*>& GetSpawnedObstacles() { return SpawnedObstacles; }
	const TArray<AObstacleActorBase*>& GetSpawnedObstacles() const { return SpawnedObstacles; }

private:
	// Random Obstacle generator
	void SpawnObstacle();

	// Item Spawn 관련 함수
	void SpawnItemForSlot(const FDecisionSlot& DecisionSlot, EObstacleDecision Decision);

	// Builds floor based on the FloorBlockInfos array in the template 
	void BuildFloor();

private:
	FMapChunkTemplate     Template;
	float                 ObstacleFillRate = 0.f;
	float				  ChunkBuggedRate  = 0.1f;
	USceneComponent*      ChunkRoot = nullptr;
	TArray<UStaticMeshComponent*> FloorMeshes;
	TArray<AObstacleActorBase*> SpawnedObstacles;
	TArray<AItemActorBase*> SpawnedItems;

	bool bWasObstacleSpawned = false;
};
