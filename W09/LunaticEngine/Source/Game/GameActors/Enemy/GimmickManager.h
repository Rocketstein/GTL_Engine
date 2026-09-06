#pragma once

#include "Game/GameActors/Enemy/GimmickActorBase.h"

class UWorld;
class AObstacleActorBase;

// TODO: Add more gimmicks.
enum class EGimmickType
{
	TranslateGizmo,
	RotationGizmo,
	ScaleGizmo,
	Count,
};

class FGimmickManager
{
public:
	FGimmickManager() = default;

	AGimmickActorBase* SpawnGimmickActor(
		UWorld* World,
		EGimmickType Gimmick,
		const TArray<AObstacleActorBase*>& CandidateObstacles);
	AGimmickActorBase* TrySpawnRandomGimmick(
		UWorld* World,
		const TArray<AObstacleActorBase*>& CandidateObstacles,
		float SpawnChance);

private:
	AObstacleActorBase* FindRandomObstacleTarget(const TArray<AObstacleActorBase*>& CandidateObstacles) const;
	EGimmickType SelectRandomGimmickType() const;
};
