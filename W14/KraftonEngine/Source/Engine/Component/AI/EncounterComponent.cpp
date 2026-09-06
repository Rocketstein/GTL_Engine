#include "Component/AI/EncounterComponent.h"

void UEncounterComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	if (bAutoStartOnBeginPlay)
	{
		StartEncounter();
	}
}

bool UEncounterComponent::StartEncounter()
{
	if (State == EEncounterState::Active)
	{
		return false;
	}
	State = EEncounterState::Active;
	OnEncounterStarted.Broadcast(this);
	return true;
}

bool UEncounterComponent::CompleteEncounter()
{
	if (State == EEncounterState::Completed)
	{
		return false;
	}
	State = EEncounterState::Completed;
	OnEncounterCompleted.Broadcast(this);
	return true;
}

void UEncounterComponent::ResetEncounter()
{
	State = EEncounterState::Inactive;
	OnEncounterReset.Broadcast(this);
}
