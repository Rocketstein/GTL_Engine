#include "GimmickManager.h"

#include "Game/GameActors/Enemy/ImposterRotationGizmo.h"
#include "Game/GameActors/Enemy/ImposterScaleGizmo.h"
#include "Game/GameActors/Enemy/ImposterTranslateGizmo.h"
#include "Game/GameActors/Obstacle/ObstacleActorBase.h"
#include "Game/Map/MapRandom.h"
#include "GameFramework/World.h"
#include "Object/Object.h"

AGimmickActorBase* FGimmickManager::TrySpawnRandomGimmick(
	UWorld* World,
	const TArray<AObstacleActorBase*>& CandidateObstacles,
	float SpawnChance)
{
	if (!World || !MapRandom::Chance(SpawnChance))
	{
		return nullptr;
	}

	return SpawnGimmickActor(World, SelectRandomGimmickType(), CandidateObstacles);
}

AGimmickActorBase* FGimmickManager::SpawnGimmickActor(
	UWorld* World,
	EGimmickType Gimmick,
	const TArray<AObstacleActorBase*>& CandidateObstacles)
{
	if (!World)
	{
		return nullptr;
	}

	switch (Gimmick)
	{
	case EGimmickType::TranslateGizmo:
	{
		AObstacleActorBase* Target = FindRandomObstacleTarget(CandidateObstacles);
		if (!Target)
		{
			return nullptr;
		}

		AImposterTranslateGizmo* Actor = World->SpawnActor<AImposterTranslateGizmo>();
		if (Actor)
		{
			Actor->Capture(Target);
		}
		return Actor;
	}
	case EGimmickType::RotationGizmo:
	{
		AObstacleActorBase* Target = FindRandomObstacleTarget(CandidateObstacles);
		if (!Target)
		{
			return nullptr;
		}

		AImposterRotationGizmo* Actor = World->SpawnActor<AImposterRotationGizmo>();
		if (Actor)
		{
			Actor->Capture(Target);
		}
		return Actor;
	}
	case EGimmickType::ScaleGizmo:
	{
		AObstacleActorBase* Target = FindRandomObstacleTarget(CandidateObstacles);
		if (!Target)
		{
			return nullptr;
		}

		AImposterScaleGizmo* Actor = World->SpawnActor<AImposterScaleGizmo>();
		if (Actor)
		{
			Actor->Capture(Target);
		}
		return Actor;
	}
	default:
		return nullptr;
	}
}

AObstacleActorBase* FGimmickManager::FindRandomObstacleTarget(const TArray<AObstacleActorBase*>& CandidateObstacles) const
{
	TArray<AObstacleActorBase*> AliveCandidates;
	for (AObstacleActorBase* Obstacle : CandidateObstacles)
	{
		if (Obstacle && IsAliveObject(Obstacle))
		{
			AliveCandidates.push_back(Obstacle);
		}
	}

	if (AliveCandidates.empty())
	{
		return nullptr;
	}

	return AliveCandidates[MapRandom::Index(static_cast<int32>(AliveCandidates.size()))];
}

EGimmickType FGimmickManager::SelectRandomGimmickType() const
{
	const int32 TypeCount = static_cast<int32>(EGimmickType::Count);
	return static_cast<EGimmickType>(MapRandom::Index(TypeCount));
}
