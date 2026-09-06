#include "PawnMovementComponent.h"
#include "GameFramework/Pawn/Pawn.h"

void UPawnMovementComponent::AddInputVector(FVector WorldVector, bool bForce)
{
	if (bMoveInputIgnored && !bForce)
	{
		return;
	}
	PendingInputVector += WorldVector;
}

FVector UPawnMovementComponent::ConsumeInputVector()
{
	LastInputVector = PendingInputVector;
	PendingInputVector = FVector::ZeroVector;
	return LastInputVector;
}

APawn* UPawnMovementComponent::GetPawnOwner() const
{
	if (!UpdatedComponent)
	{
		return nullptr;
	}
	return Cast<APawn>(UpdatedComponent->GetOwner());
}

void UPawnMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	if (!NewUpdatedComponent || !NewUpdatedComponent->GetOwner() || !NewUpdatedComponent->GetOwner()->IsA<APawn>()) return;
	UpdatedComponent = NewUpdatedComponent;
}
