#include "AI/Navigation/PathFollowingComponent.h"

#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/Controller/AIController.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace
{
	float FlatDistanceSquared(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}
}

AAIController* UPathFollowingComponent::GetControllerOwner() const
{
	return Cast<AAIController>(GetOwner());
}

APawn* UPathFollowingComponent::GetPawnOwner() const
{
	AAIController* Controller = GetControllerOwner();
	return Controller ? Controller->GetPawn() : nullptr;
}

ACharacter* UPathFollowingComponent::GetCharacterOwner() const
{
	return Cast<ACharacter>(GetPawnOwner());
}

float UPathFollowingComponent::GetOwnerAgentRadius() const
{
	ACharacter* Character = GetCharacterOwner();
	if (!Character)
	{
		return 0.0f;
	}
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	return Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f;
}

float UPathFollowingComponent::GetEffectivePathPointAcceptanceRadius(bool bFinalPoint) const
{
	if (bFinalPoint || !bUseAgentRadiusForCornerAcceptance)
	{
		return bFinalPoint ? AcceptanceRadius : PathPointAcceptanceRadius;
	}

	const float AgentRadius = GetOwnerAgentRadius();
	if (AgentRadius <= 0.0f)
	{
		return PathPointAcceptanceRadius;
	}

	const float AgentScaledRadius = std::max(MinCornerAcceptanceRadius, AgentRadius * std::max(0.05f, CornerAcceptanceRadiusScale));
	return std::min(PathPointAcceptanceRadius, AgentScaledRadius);
}

bool UPathFollowingComponent::RequestMove(const FNavigationPath& InPath, float InAcceptanceRadius)
{
	if (!InPath.IsValid())
	{
		FinishMove(EPathFollowingResult::Invalid);
		return false;
	}
	Path = InPath;
	AcceptanceRadius = InAcceptanceRadius > 0.0f ? InAcceptanceRadius : DefaultAcceptanceRadius;
	CurrentPathIndex = Path.PathPoints.size() > 1 ? 1 : 0;
	LastPathPointCount = Path.Num();
	LastMoveGoalLocation = Path.GetEndLocation();
	LastMoveResult = EPathFollowingResult::Invalid;
	Status = EPathFollowingStatus::Moving;
	BlockedTime = 0.0f;
	if (APawn* Pawn = GetPawnOwner())
	{
		LastMoveLocation = Pawn->GetActorLocation();
	}
	if (bDrawDebugPathOnRequest)
	{
		UWorld* World = GetWorld();
		for (int32 Index = 0; World && Index + 1 < Path.Num(); ++Index)
		{
			const FVector A = Path.GetPathPointLocation(Index) + FVector(0.0f, 0.0f, 6.0f);
			const FVector B = Path.GetPathPointLocation(Index + 1) + FVector(0.0f, 0.0f, 6.0f);
			DrawDebugLine(World, A, B, FColor(255, 220, 40), DebugPathDrawDuration);
			DrawDebugSphere(World, A, 0.18f, 8, FColor(40, 140, 255), DebugPathDrawDuration);
		}
		if (World && Path.Num() > 0)
		{
			DrawDebugSphere(World, Path.GetEndLocation() + FVector(0.0f, 0.0f, 6.0f), 0.24f, 8, FColor(255, 80, 40), DebugPathDrawDuration);
		}
	}
	return true;
}

void UPathFollowingComponent::AbortMove()
{
	if (Status == EPathFollowingStatus::Idle)
	{
		return;
	}
	FinishMove(EPathFollowingResult::Aborted);
}

void UPathFollowingComponent::PauseMove()
{
	if (Status == EPathFollowingStatus::Moving)
	{
		Status = EPathFollowingStatus::Paused;
	}
}

void UPathFollowingComponent::ResumeMove()
{
	if (Status == EPathFollowingStatus::Paused && Path.IsValid())
	{
		Status = EPathFollowingStatus::Moving;
	}
}

void UPathFollowingComponent::FinishMove(EPathFollowingResult Result)
{
	LastMoveResult = Result;
	Status = EPathFollowingStatus::Idle;
	CurrentPathIndex = 0;
	Path.Reset();
	BlockedTime = 0.0f;
	if (ACharacter* Character = GetCharacterOwner())
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->ClearInputVector();
			if (Result != EPathFollowingResult::Success)
			{
				Movement->StopHorizontalMovementImmediately();
			}
		}
	}
	OnMoveFinished.Broadcast(this, Result);
}

FVector UPathFollowingComponent::GetCurrentPathPoint() const
{
	return Path.GetPathPointLocation(CurrentPathIndex);
}

float UPathFollowingComponent::GetRemainingDistance() const
{
	APawn* Pawn = GetPawnOwner();
	if (!Pawn || !Path.IsValid())
	{
		return 0.0f;
	}
	float Remaining = sqrtf(FlatDistanceSquared(Pawn->GetActorLocation(), GetCurrentPathPoint()));
	for (int32 Index = CurrentPathIndex; Index + 1 < Path.Num(); ++Index)
	{
		Remaining += sqrtf(FlatDistanceSquared(Path.GetPathPointLocation(Index), Path.GetPathPointLocation(Index + 1)));
	}
	return Remaining;
}

void UPathFollowingComponent::AdvancePathIfNeeded()
{
	APawn* Pawn = GetPawnOwner();
	if (!Pawn || !Path.IsValid())
	{
		FinishMove(EPathFollowingResult::Invalid);
		return;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	while (CurrentPathIndex < Path.Num())
	{
		const bool bFinalPoint = CurrentPathIndex >= Path.Num() - 1;
		const float Radius = GetEffectivePathPointAcceptanceRadius(bFinalPoint);
		if (FlatDistanceSquared(PawnLocation, Path.GetPathPointLocation(CurrentPathIndex)) > Radius * Radius)
		{
			break;
		}
		if (bFinalPoint)
		{
			FinishMove(EPathFollowingResult::Success);
			return;
		}
		++CurrentPathIndex;
	}
}

void UPathFollowingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Status != EPathFollowingStatus::Moving)
	{
		return;
	}
	ACharacter* Character = GetCharacterOwner();
	if (!Character || !Path.IsValid())
	{
		FinishMove(EPathFollowingResult::Invalid);
		return;
	}

	AdvancePathIfNeeded();
	if (Status != EPathFollowingStatus::Moving)
	{
		return;
	}

	const FVector Target = GetCurrentPathPoint();
	FVector Direction = Target - Character->GetActorLocation();
	Direction.Z = 0.0f;
	if (!Direction.IsNearlyZero())
	{
		const float DistToWaypoint = Direction.Length();
		float Scale = MoveInputScale;

		// 관성 overshoot(공전/루프) 방지: 최종 목표 가까이서, 또는 다음 웨이포인트로 급회전일 때
		// 가까울수록 입력을 줄여 미리 감속한다(정지거리 > 수용반경이라 안 하면 목표를 지나쳐 돈다).
		const bool bFinal = CurrentPathIndex >= Path.Num() - 1;
		float SlowRadius = bFinal ? ArrivalSlowdownRadius : 0.0f;
		if (!bFinal && CurrentPathIndex + 1 < Path.Num())
		{
			FVector ToNext = Path.GetPathPointLocation(CurrentPathIndex + 1) - Target;
			ToNext.Z = 0.0f;
			if (!ToNext.IsNearlyZero())
			{
				const FVector CurDir  = Direction.Normalized();
				const FVector NextDir = ToNext.Normalized();
				const float   Dot     = CurDir.X * NextDir.X + CurDir.Y * NextDir.Y; // 1=직진, -1=U턴
				const float   Turn    = (1.0f - Dot) * 0.5f;                          // 0=직진, 1=U턴
				SlowRadius = CornerSlowdownRadius * Turn;
			}
		}
		if (SlowRadius > 0.01f && DistToWaypoint < SlowRadius)
		{
			Scale *= std::max(MinArrivalScale, DistToWaypoint / SlowRadius);
		}

		Character->AddMovementInput(Direction.Normalized(), Scale);
	}

	const FVector CurrentLocation = Character->GetActorLocation();
	if (FlatDistanceSquared(CurrentLocation, LastMoveLocation) <= BlockedMinMoveDistance * BlockedMinMoveDistance)
	{
		BlockedTime += DeltaTime;
		if (BlockedTimeout > 0.0f && BlockedTime >= BlockedTimeout)
		{
			FinishMove(EPathFollowingResult::Blocked);
			return;
		}
	}
	else
	{
		LastMoveLocation = CurrentLocation;
		BlockedTime = 0.0f;
	}
}
