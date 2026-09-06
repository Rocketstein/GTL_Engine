#include "GameFramework/Controller/Controller.h"
#include "GameFramework/Pawn/Pawn.h"

void AController::Possess(APawn* Pawn)
{
	if (!Pawn || PossessedPawn.Get() == Pawn)
	{
		return;
	}

	if (PossessedPawn)
	{
		UnPossess();
	}

	if (AController* ExistingController = Pawn->GetController())
	{
		if (ExistingController != this)
		{
			ExistingController->UnPossess();
		}
	}

	PossessedPawn = Pawn;
	Pawn->PossessedBy(this);
	SetControlRotation(Pawn->GetControlRotation());
	OnPossess(Pawn);
}

void AController::UnPossess()
{
	APawn* OldPawn = PossessedPawn.Get();
	if (!OldPawn)
	{
		PossessedPawn = nullptr;
		return;
	}

	PossessedPawn = nullptr;	
	OldPawn->UnPossessed();
	OnUnPossess(OldPawn);
}

void AController::SetControlRotation(const FRotator& NewRotation)
{
	ControlRotation = NewRotation;
	if (APawn* Pawn = GetPawn())
	{
		Pawn->SetControlRotation(NewRotation);
	}
}
