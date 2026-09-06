#pragma once
#include "Engine/GameFramework/AActor.h"
#include "GameState.h"

struct FSoftPauseState
{
	bool bEnabled = false;
	TArray<TWeakObjectPtr<AActor>> AllowedActors;

	bool IsActorAllowed(const AActor* Actor) const
	{
		if (!bEnabled || !Actor)
		{
			return true;
		}

		for (const TWeakObjectPtr<AActor>& Allowed : AllowedActors)
		{
			if (Allowed.Get() == Actor)
			{
				return true;
			}
		}
		return false;
	}

	void Begin(TArray<AActor*> InAllowedActors)
	{
		bEnabled = true;
		AllowedActors.clear();
		for (AActor* Actor : InAllowedActors)
		{
			if (Actor)
			{
				AllowedActors.push_back(Actor);
			}
		}
	}

	void End()
	{
		bEnabled = false;
		AllowedActors.clear();
	}
};