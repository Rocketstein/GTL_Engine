#include "TickFunction.h"
#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

namespace
{
	bool ShouldDispatchActorTick(const AActor* Actor, ELevelTick TickType)
	{
		if (!Actor)
		{
			return false;
		}

		switch (TickType)
		{
		case LEVELTICK_ViewportsOnly:
			return Actor->bTickInEditor;

		case LEVELTICK_All:
		case LEVELTICK_TimeOnly:
		case LEVELTICK_PauseTick:
			return Actor->bNeedsTick && Actor->HasActorBegunPlay();

		default:
			return false;
		}
	}
}

void FTickFunction::RegisterTickFunction()
{
	bRegistered = true;
	TickAccumulator = 0.0f;
}

void FTickFunction::UnRegisterTickFunction()
{
	bRegistered = false;
	TickAccumulator = 0.0f;
}

void FTickManager::Tick(UWorld* World, float DeltaTime, ELevelTick TickType)
{
	GatherTickFunctions(World, TickType);

	for (int GroupIndex = 0; GroupIndex < TG_MAX; ++GroupIndex)
	{
		const ETickingGroup CurrentGroup = static_cast<ETickingGroup>(GroupIndex);
		for (const FQueuedTickFunction& QueuedTickFunction : TickFunctions)
		{
			if (QueuedTickFunction.TickGroup != CurrentGroup)
			{
				continue;
			}

			FTickFunction* ResolvedTickFunction = ResolveTickFunction(QueuedTickFunction, TickType);
			if (!ResolvedTickFunction || ResolvedTickFunction->GetTickGroup() != CurrentGroup)
			{
				continue;
			}

			if (!ResolvedTickFunction->CanTick(TickType))
			{
				continue;
			}

			if (!ResolvedTickFunction->ConsumeInterval(DeltaTime))
			{
				continue;
			}

			ResolvedTickFunction->ExecuteTick(DeltaTime, TickType);
		}
	}
}

void FTickManager::Reset()
{
	TickFunctions.clear();
}

void FTickManager::GatherTickFunctions(UWorld* World, ELevelTick TickType)
{
	TickFunctions.clear();

	if (!World)
	{
		return;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (!ShouldDispatchActorTick(Actor, TickType))
		{
			continue;
		}

		QueueActorTickFunction(Actor);

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component)
			{
				continue;
			}

			QueueComponentTickFunction(Component);
		}
	}
}

void FTickManager::QueueActorTickFunction(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (!Actor->PrimaryActorTick.bRegistered)
	{
		Actor->PrimaryActorTick.RegisterTickFunction();
	}

	TickFunctions.push_back({
		Actor,
		Actor->GetUUID(),
		Actor->PrimaryActorTick.GetTickGroup(),
		EQueuedTickTarget::Actor
	});
}

void FTickManager::QueueComponentTickFunction(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (!Component->PrimaryComponentTick.bRegistered)
	{
		Component->PrimaryComponentTick.RegisterTickFunction();
	}

	TickFunctions.push_back({
		Component,
		Component->GetUUID(),
		Component->PrimaryComponentTick.GetTickGroup(),
		EQueuedTickTarget::Component
	});
}

FTickFunction* FTickManager::ResolveTickFunction(const FQueuedTickFunction& QueuedTickFunction, ELevelTick TickType) const
{
	if (!IsAliveObject(QueuedTickFunction.Target) || QueuedTickFunction.Target->GetUUID() != QueuedTickFunction.TargetUUID)
	{
		return nullptr;
	}

	if (QueuedTickFunction.TargetType == EQueuedTickTarget::Actor)
	{
		AActor* Actor = Cast<AActor>(QueuedTickFunction.Target);
		if (!ShouldDispatchActorTick(Actor, TickType))
		{
			return nullptr;
		}

		return &Actor->PrimaryActorTick;
	}

	UActorComponent* Component = Cast<UActorComponent>(QueuedTickFunction.Target);
	AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Component || !IsAliveObject(Owner) || !ShouldDispatchActorTick(Owner, TickType))
	{
		return nullptr;
	}

	return &Component->PrimaryComponentTick;
}

void FActorTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	if (Target)
	{
		Target->TickActor(DeltaTime, TickType, *this);
	}
}

const char* FActorTickFunction::GetDebugName() const
{
	return Target ? Target->GetClass()->GetName() : "FActorTickFunction";
}

void FActorComponentTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	if (Target)
	{
		Target->TickComponent(DeltaTime, TickType, *this);
	}
}

const char* FActorComponentTickFunction::GetDebugName() const
{
	return Target ? Target->GetClass()->GetName() : "FActorComponentTickFunction";
}
