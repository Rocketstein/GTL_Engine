#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Math/Vector.h"
#include "Navigation/NavTypes.h"
#include "Object/FName.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AActor;

#include "Source/Engine/Component/AI/EnemyAIBrainComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FEnemyAITargetChangedSignature, class UEnemyAIBrainComponent*, AActor*);

// Target/query/move façade. 행동 정책과 상태 전이는 Lua Blueprint 또는 상위 pawn이 소유한다.
UCLASS()
class UEnemyAIBrainComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UEnemyAIBrainComponent() = default;
	~UEnemyAIBrainComponent() override = default;

	UFUNCTION(Callable, Category="EnemyAI|Target")
	void SetTarget(AActor* NewTarget);
	UFUNCTION(Callable, Category="EnemyAI|Target")
	void ClearTarget();
	UFUNCTION(Pure, Category="EnemyAI|Target")
	AActor* GetTarget() const { return TargetActor.Get(); }
	UFUNCTION(Pure, Category="EnemyAI|Target")
	bool HasValidTarget() const;

	UFUNCTION(Callable, Category="EnemyAI|Target")
	AActor* AcquireTargetByTag(const FName& Tag, float SearchRange = 0.0f);
	UFUNCTION(Callable, Category="EnemyAI|Target")
	AActor* AcquireNearestHostileTarget(float SearchRange);

	UFUNCTION(Pure, Category="EnemyAI|Target")
	float GetDistanceToTarget() const;
	UFUNCTION(Pure, Category="EnemyAI|Target")
	float GetFlatDistanceToTarget() const;
	UFUNCTION(Pure, Category="EnemyAI|Target")
	float GetVerticalDeltaToTarget() const;
	UFUNCTION(Pure, Category="EnemyAI|Target")
	FVector GetDirectionToTarget() const;
	UFUNCTION(Pure, Category="EnemyAI|Target")
	FVector GetFlatDirectionToTarget() const;
	UFUNCTION(Pure, Category="EnemyAI|Target")
	float GetAngleToTarget() const;
	UFUNCTION(Pure, Category="EnemyAI|Target")
	bool IsTargetInRange(float Range) const;
	UFUNCTION(Pure, Category="EnemyAI|Target")
	bool IsTargetInFront(float MaxAbsAngleDegrees = 70.0f) const;
	UFUNCTION(Pure, Category="EnemyAI|Target")
	bool IsTargetBehind(float MinAbsAngleDegrees = 120.0f) const;

	UFUNCTION(Callable, Category="EnemyAI|Move")
	bool RequestMoveToTarget(float AcceptanceRadius = -1.0f, bool bUsePathfinding = true);
	UFUNCTION(Callable, Category="EnemyAI|Move")
	void StopMove();
	UFUNCTION(Pure, Category="EnemyAI|Move")
	bool IsMoveActive() const;
	UFUNCTION(Pure, Category="EnemyAI|Move")	
	EPathFollowingStatus GetMoveStatus() const;
	UFUNCTION(Pure, Category="EnemyAI|Move")
	EPathFollowingRequestResult GetLastMoveRequestResult() const;
	UFUNCTION(Pure, Category="EnemyAI|Move")
	EPathFollowingResult GetLastMoveResult() const;
	UFUNCTION(Pure, Category="EnemyAI|Move")
	FString GetLastMoveFailureReason() const;

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	FEnemyAITargetChangedSignature OnTargetChanged;

private:
	TWeakObjectPtr<AActor> TargetActor = nullptr;
};
