#include "Component/AI/PhaseComponent.h"

void UPhaseComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	CurrentPhase = InitialPhase > 0 ? InitialPhase : 1;
}

bool UPhaseComponent::SetPhase(int32 NewPhase)
{
	if (NewPhase <= 0 || NewPhase == CurrentPhase)
	{
		return false;
	}
	const int32 OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;
	OnPhaseChanged.Broadcast(this, OldPhase, NewPhase);
	return true;
}

FEnemyPhaseData UPhaseComponent::GetCurrentPhaseData() const
{
	for (const FEnemyPhaseData& PhaseData : Phases)
	{
		if (PhaseData.Phase == CurrentPhase)
		{
			return PhaseData;
		}
	}
	return FEnemyPhaseData();
}
