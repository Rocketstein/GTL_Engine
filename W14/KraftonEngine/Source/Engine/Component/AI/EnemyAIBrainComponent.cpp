#include "Component/AI/EnemyAIBrainComponent.h"

#include "AI/CombatTargetRegistry.h"
#include "Component/Combat/CombatStateComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/EnemyCharacter.h"
#include "GameFramework/Controller/AIController.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"

#include <cfloat>
#include <cmath>

namespace
{
	float GetFlatAngleBetween(const FVector& Forward, const FVector& Direction)
	{
		FVector A = Forward;
		FVector B = Direction;
		A.Z = 0.0f;
		B.Z = 0.0f;
		if (A.IsNearlyZero() || B.IsNearlyZero())
		{
			return 0.0f;
		}
		A = A.Normalized();
		B = B.Normalized();
		const float Dot = FMath::Clamp(A.Dot(B), -1.0f, 1.0f);
		float Angle = acosf(Dot) * FMath::RadToDeg;
		const float CrossZ = A.X * B.Y - A.Y * B.X;
		if (CrossZ < 0.0f)
		{
			Angle = -Angle;
		}
		return Angle;
	}

	bool IsAliveTarget(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return false;
		}
		if (UHealthComponent* Health = Actor->GetComponentByClass<UHealthComponent>())
		{
			return !Health->IsDead();
		}
		return true;
	}
}

void UEnemyAIBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (TargetActor.IsValid() && !HasValidTarget())
	{
		ClearTarget();
	}
}

void UEnemyAIBrainComponent::SetTarget(AActor* NewTarget)
{
	if (TargetActor.Get() == NewTarget)
	{
		return;
	}
	TargetActor = NewTarget;
	OnTargetChanged.Broadcast(this, NewTarget);
}

void UEnemyAIBrainComponent::ClearTarget()
{
	if (!TargetActor.IsValid())
	{
		TargetActor = nullptr;
		return;
	}
	TargetActor = nullptr;
	OnTargetChanged.Broadcast(this, nullptr);
}

bool UEnemyAIBrainComponent::HasValidTarget() const
{
	AActor* Target = TargetActor.Get();
	AActor* OwnerActor = GetOwner();
	if (!IsAliveTarget(Target) || !OwnerActor || Target == OwnerActor)
	{
		return false;
	}
	UCombatStateComponent* OwnerCombat = OwnerActor->GetComponentByClass<UCombatStateComponent>();
	UCombatStateComponent* TargetCombat = Target->GetComponentByClass<UCombatStateComponent>();
	return OwnerCombat && TargetCombat && OwnerCombat->IsHostileTo(TargetCombat);
}

AActor* UEnemyAIBrainComponent::AcquireTargetByTag(const FName& Tag, float SearchRange)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World || !OwnerActor || !Tag.IsValid())
	{
		return nullptr;
	}

	AActor* BestActor = nullptr;
	float BestDistanceSq = FLT_MAX;
	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	for (AActor* Candidate : World->GetActors())
	{
		if (!IsAliveTarget(Candidate) || Candidate == OwnerActor || !Candidate->HasTag(Tag))
		{
			continue;
		}
		FVector Delta = Candidate->GetActorLocation() - OwnerLocation;
		Delta.Z = 0.0f;
		const float DistSq = Delta.X * Delta.X + Delta.Y * Delta.Y;
		if (SearchRange > 0.0f && DistSq > SearchRange * SearchRange)
		{
			continue;
		}
		if (DistSq < BestDistanceSq)
		{
			BestDistanceSq = DistSq;
			BestActor = Candidate;
		}
	}
	SetTarget(BestActor);
	return BestActor;
}

AActor* UEnemyAIBrainComponent::AcquireNearestHostileTarget(float SearchRange)
{
	AActor* OwnerActor = GetOwner();
	UCombatStateComponent* OwnerCombat = OwnerActor ? OwnerActor->GetComponentByClass<UCombatStateComponent>() : nullptr;
	if (!OwnerActor || !OwnerCombat)
	{
		return nullptr;
	}
	AActor* BestActor = FCombatTargetRegistry::Get().FindNearestHostile(OwnerCombat, OwnerActor->GetActorLocation(), SearchRange);
	SetTarget(BestActor);
	return BestActor;
}

float UEnemyAIBrainComponent::GetDistanceToTarget() const
{
	AActor* OwnerActor = GetOwner();
	AActor* Target = TargetActor.Get();
	if (!OwnerActor || !IsValid(Target))
	{
		return FLT_MAX;
	}
	return FVector::Distance(OwnerActor->GetActorLocation(), Target->GetActorLocation());
}

float UEnemyAIBrainComponent::GetFlatDistanceToTarget() const
{
	AActor* OwnerActor = GetOwner();
	AActor* Target = TargetActor.Get();
	if (!OwnerActor || !IsValid(Target))
	{
		return FLT_MAX;
	}
	FVector Delta = Target->GetActorLocation() - OwnerActor->GetActorLocation();
	Delta.Z = 0.0f;
	return Delta.Length();
}

float UEnemyAIBrainComponent::GetVerticalDeltaToTarget() const
{
	AActor* OwnerActor = GetOwner();
	AActor* Target = TargetActor.Get();
	if (!OwnerActor || !IsValid(Target))
	{
		return FLT_MAX;
	}
	return fabsf(Target->GetActorLocation().Z - OwnerActor->GetActorLocation().Z);
}

FVector UEnemyAIBrainComponent::GetDirectionToTarget() const
{
	AActor* OwnerActor = GetOwner();
	AActor* Target = TargetActor.Get();
	if (!OwnerActor || !IsValid(Target))
	{
		return FVector::ZeroVector;
	}
	const FVector Delta = Target->GetActorLocation() - OwnerActor->GetActorLocation();
	if (Delta.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	return Delta.Normalized();
}

FVector UEnemyAIBrainComponent::GetFlatDirectionToTarget() const
{
	FVector Direction = GetDirectionToTarget();
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	return Direction.Normalized();
}

float UEnemyAIBrainComponent::GetAngleToTarget() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !HasValidTarget())
	{
		return 0.0f;
	}
	return GetFlatAngleBetween(OwnerActor->GetActorForward(), GetFlatDirectionToTarget());
}

bool UEnemyAIBrainComponent::IsTargetInRange(float Range) const
{
	return HasValidTarget() && GetFlatDistanceToTarget() <= Range;
}

bool UEnemyAIBrainComponent::IsTargetInFront(float MaxAbsAngleDegrees) const
{
	return HasValidTarget() && fabsf(GetAngleToTarget()) <= MaxAbsAngleDegrees;
}

bool UEnemyAIBrainComponent::IsTargetBehind(float MinAbsAngleDegrees) const
{
	return HasValidTarget() && fabsf(GetAngleToTarget()) >= MinAbsAngleDegrees;
}

bool UEnemyAIBrainComponent::RequestMoveToTarget(float AcceptanceRadius, bool bUsePathfinding)
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy || !HasValidTarget())
	{
		return false;
	}
	const float Radius = AcceptanceRadius > 0.0f ? AcceptanceRadius : 0.5f;
	return Enemy->RequestMoveToTarget(Radius, bUsePathfinding);
}

void UEnemyAIBrainComponent::StopMove()
{
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		Enemy->StopEnemyMovement();
	}
}

bool UEnemyAIBrainComponent::IsMoveActive() const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	return Enemy && Enemy->IsPathFollowing();
}

EPathFollowingStatus UEnemyAIBrainComponent::GetMoveStatus() const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	AAIController* Controller = Enemy ? Enemy->GetEnemyAIController() : nullptr;
	return Controller ? Controller->GetMoveStatus() : EPathFollowingStatus::Idle;
}

EPathFollowingRequestResult UEnemyAIBrainComponent::GetLastMoveRequestResult() const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	AAIController* Controller = Enemy ? Enemy->GetEnemyAIController() : nullptr;
	return Controller ? Controller->GetLastMoveRequestResult() : EPathFollowingRequestResult::Failed;
}

EPathFollowingResult UEnemyAIBrainComponent::GetLastMoveResult() const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	AAIController* Controller = Enemy ? Enemy->GetEnemyAIController() : nullptr;
	return Controller ? Controller->GetLastMoveResult() : EPathFollowingResult::Invalid;
}

FString UEnemyAIBrainComponent::GetLastMoveFailureReason() const
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	AAIController* Controller = Enemy ? Enemy->GetEnemyAIController() : nullptr;
	return Controller ? Controller->GetLastMoveFailureReason() : "No AIController";
}
