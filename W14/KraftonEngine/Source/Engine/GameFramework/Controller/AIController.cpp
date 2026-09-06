#include "GameFramework/Controller/AIController.h"

#include "AI/Navigation/PathFollowingComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Navigation/NavigationSystem.h"

#include <algorithm>
#include <cmath>

void AAIController::BeginPlay()
{
	if (!PathFollowingComponent)
	{
		PathFollowingComponent = AddComponent<UPathFollowingComponent>();
	}
	Super::BeginPlay();
}

void AAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!MoveGoalActor.IsValid() || !PathFollowingComponent || !PathFollowingComponent->IsMoving())
	{
		return;
	}
	TimeSinceLastRepath += DeltaTime;
	if (ShouldRepathToMovingGoal())
	{
		RequestMoveToCurrentGoal(bCurrentMoveUsesPathfinding);
	}
}

void AAIController::OnPossess(APawn* Pawn)
{
	(void)Pawn;
	if (!PathFollowingComponent)
	{
		PathFollowingComponent = AddComponent<UPathFollowingComponent>();
	}
	RefreshNavAgentPropertiesFromPawn(Pawn);
}

void AAIController::OnUnPossess(APawn* OldPawn)
{
	(void)OldPawn;
	StopMovement();
}

UNavigationSystem* AAIController::GetNavigationSystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetNavigationSystem() : nullptr;
}

void AAIController::RefreshNavAgentPropertiesFromPawn(APawn* PawnOverride)
{
	APawn* ControlledPawn = PawnOverride ? PawnOverride : GetPawn();
	if (!ControlledPawn)
	{
		return;
	}
	if (ACharacter* Character = Cast<ACharacter>(ControlledPawn))
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			NavAgentProperties.Radius = std::max(0.05f, Capsule->GetScaledCapsuleRadius());
			NavAgentProperties.Height = std::max(NavAgentProperties.Radius * 2.0f, Capsule->GetScaledCapsuleHalfHeight() * 2.0f);
		}
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			NavAgentProperties.StepHeight = Movement->MaxStepHeight;
			NavAgentProperties.MaxClimbHeight = Movement->MaxStepHeight;
			NavAgentProperties.MaxDropHeight = Movement->MaxDropHeight;
		}
	}
}

EPathFollowingStatus AAIController::GetMoveStatus() const
{
	return PathFollowingComponent ? PathFollowingComponent->GetStatus() : EPathFollowingStatus::Idle;
}

bool AAIController::IsFollowingPath() const
{
	return PathFollowingComponent && PathFollowingComponent->IsMoving();
}

EPathFollowingResult AAIController::GetLastMoveResult() const
{
	return PathFollowingComponent ? PathFollowingComponent->GetLastMoveResult() : EPathFollowingResult::Invalid;
}

int32 AAIController::GetCurrentPathPointCount() const
{
	return PathFollowingComponent ? PathFollowingComponent->GetPathPointCount() : 0;
}

EPathFollowingRequestResult AAIController::SetLastMoveRequestResult(EPathFollowingRequestResult Result, const FString& Reason)
{
	LastMoveRequestResult = Result;
	LastMoveFailureReason = Reason;
	return Result;
}

bool AAIController::CanReuseCurrentMove(AActor* NewGoalActor, const FVector& NewGoalLocation, float NewAcceptanceRadius, bool bUsePathfinding) const
{
	if (!bReuseCurrentPathForSameGoal || !PathFollowingComponent || !PathFollowingComponent->IsMoving())
	{
		return false;
	}
	if (bCurrentMoveUsesPathfinding != bUsePathfinding)
	{
		return false;
	}
	if (fabsf(MoveAcceptanceRadius - NewAcceptanceRadius) > 0.05f)
	{
		return false;
	}
	if (MoveGoalActor.Get() != NewGoalActor)
	{
		return false;
	}
	const float RepathDist = std::max(0.1f, GoalRepathDistance);
	if (FVector::DistSquared(NewGoalLocation, LastRequestedGoalLocation) > RepathDist * RepathDist)
	{
		return false;
	}
	return TimeSinceLastRepath < std::max(0.0f, RepathInterval);
}

EPathFollowingRequestResult AAIController::MoveToActor(AActor* Goal, float AcceptanceRadius, bool bStopOnOverlap, bool bUsePathfinding)
{
	if (!Goal)
	{
		StopMovement();
		return SetLastMoveRequestResult(EPathFollowingRequestResult::Failed, "MoveToActor failed: null goal");
	}
	const FVector GoalLocation = Goal->GetActorLocation();
	float ResolvedAcceptanceRadius = AcceptanceRadius > 0.0f ? AcceptanceRadius : 3.0f;
	if (bStopOnOverlap)
	{
		// 캡슐이 겹치기 전에 멈추도록 수용 반경에 양쪽 캡슐 반경을 더한다(몸통 크기 인식).
		float SelfRadius = 0.0f;
		float GoalRadius = 0.0f;
		if (ACharacter* SelfChar = Cast<ACharacter>(GetPawn()))
		{
			if (UCapsuleComponent* Capsule = SelfChar->GetCapsuleComponent())
			{
				SelfRadius = Capsule->GetScaledCapsuleRadius();
			}
		}
		if (ACharacter* GoalChar = Cast<ACharacter>(Goal))
		{
			if (UCapsuleComponent* Capsule = GoalChar->GetCapsuleComponent())
			{
				GoalRadius = Capsule->GetScaledCapsuleRadius();
			}
		}
		ResolvedAcceptanceRadius += SelfRadius + GoalRadius;
	}
	if (CanReuseCurrentMove(Goal, GoalLocation, ResolvedAcceptanceRadius, bUsePathfinding))
	{
		return SetLastMoveRequestResult(EPathFollowingRequestResult::RequestSuccessful);
	}
	MoveGoalActor = Goal;
	MoveGoalLocation = GoalLocation;
	MoveAcceptanceRadius = ResolvedAcceptanceRadius;
	bCurrentMoveUsesPathfinding = bUsePathfinding;
	return RequestMoveToCurrentGoal(bUsePathfinding);
}

EPathFollowingRequestResult AAIController::MoveToLocation(const FVector& GoalLocation, float AcceptanceRadius, bool bUsePathfinding)
{
	const float ResolvedAcceptanceRadius = AcceptanceRadius > 0.0f ? AcceptanceRadius : 3.0f;
	if (CanReuseCurrentMove(nullptr, GoalLocation, ResolvedAcceptanceRadius, bUsePathfinding))
	{
		return SetLastMoveRequestResult(EPathFollowingRequestResult::RequestSuccessful);
	}
	MoveGoalActor = nullptr;
	MoveGoalLocation = GoalLocation;
	MoveAcceptanceRadius = ResolvedAcceptanceRadius;
	bCurrentMoveUsesPathfinding = bUsePathfinding;
	return RequestMoveToCurrentGoal(bUsePathfinding);
}

void AAIController::StopMovement()
{
	MoveGoalActor = nullptr;
	if (PathFollowingComponent)
	{
		PathFollowingComponent->AbortMove();
	}
}

bool AAIController::ShouldRepathToMovingGoal() const
{
	if (RepathInterval <= 0.0f || TimeSinceLastRepath < RepathInterval)
	{
		return false;
	}
	AActor* Goal = MoveGoalActor.Get();
	if (!Goal)
	{
		return false;
	}
	const float DistSq = FVector::DistSquared(Goal->GetActorLocation(), LastRequestedGoalLocation);
	return DistSq >= GoalRepathDistance * GoalRepathDistance;
}

EPathFollowingRequestResult AAIController::RequestMoveToCurrentGoal(bool bUsePathfinding)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return SetLastMoveRequestResult(EPathFollowingRequestResult::Failed, "Move request failed: no possessed pawn");
	}
	if (!PathFollowingComponent)
	{
		PathFollowingComponent = AddComponent<UPathFollowingComponent>();
	}
	if (!PathFollowingComponent)
	{
		return SetLastMoveRequestResult(EPathFollowingRequestResult::Failed, "Move request failed: missing PathFollowingComponent");
	}

	RefreshNavAgentPropertiesFromPawn(ControlledPawn);

	const FVector Start = ControlledPawn->GetActorLocation();
	const FVector Goal = MoveGoalActor.IsValid() ? MoveGoalActor->GetActorLocation() : MoveGoalLocation;
	LastRequestedGoalLocation = Goal;
	TimeSinceLastRepath = 0.0f;

	FVector FlatDelta = Goal - Start;
	FlatDelta.Z = 0.0f;
	if (FlatDelta.Dot(FlatDelta) <= MoveAcceptanceRadius * MoveAcceptanceRadius)
	{
		PathFollowingComponent->FinishMove(EPathFollowingResult::Success);
		return SetLastMoveRequestResult(EPathFollowingRequestResult::AlreadyAtGoal);
	}

	FNavigationPath Path;
	if (bUsePathfinding)
	{
		UNavigationSystem* NavSys = GetNavigationSystem();
		if (!NavSys)
		{
			return SetLastMoveRequestResult(EPathFollowingRequestResult::Failed, "Move request failed: NavigationSystem is null");
		}
		if (!NavSys->FindPathSync(Start, Goal, NavAgentProperties, Path))
		{
			bLastPathWasPartial = false;
			return SetLastMoveRequestResult(EPathFollowingRequestResult::Failed, NavSys->GetLastQueryMessage());
		}
		if (Path.bIsPartial && !bAllowPartialPath)
		{
			bLastPathWasPartial = true;
			return SetLastMoveRequestResult(EPathFollowingRequestResult::Failed, "Move request failed: partial path rejected");
		}
		bLastPathWasPartial = Path.bIsPartial;
	}
	else
	{
		Path.Reset();
		Path.PathPoints.emplace_back(Start);
		Path.PathPoints.emplace_back(Goal);
		Path.bIsValid = true;
		bLastPathWasPartial = false;
	}

	if (!PathFollowingComponent->RequestMove(Path, MoveAcceptanceRadius))
	{
		return SetLastMoveRequestResult(EPathFollowingRequestResult::Failed, "Move request failed: PathFollowingComponent rejected path");
	}
	return SetLastMoveRequestResult(EPathFollowingRequestResult::RequestSuccessful);
}
