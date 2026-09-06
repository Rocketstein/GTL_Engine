#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Navigation/NavTypes.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AAIController;
class APawn;
class ACharacter;
class UCapsuleComponent;

#include "Source/Engine/AI/Navigation/PathFollowingComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FPathFollowingFinishedSignature, class UPathFollowingComponent*, EPathFollowingResult);

UCLASS()
class UPathFollowingComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UPathFollowingComponent() = default;
	~UPathFollowingComponent() override = default;

	UFUNCTION(Callable, Category="AI|PathFollowing")
	bool RequestMove(const FNavigationPath& InPath, float InAcceptanceRadius);
	UFUNCTION(Callable, Category="AI|PathFollowing")
	void AbortMove();
	UFUNCTION(Callable, Category="AI|PathFollowing")
	void PauseMove();
	UFUNCTION(Callable, Category="AI|PathFollowing")
	void ResumeMove();
	UFUNCTION(Callable, Category="AI|PathFollowing")
	void FinishMove(EPathFollowingResult Result);

	UFUNCTION(Pure, Category="AI|PathFollowing")
	EPathFollowingStatus GetStatus() const { return Status; }
	UFUNCTION(Pure, Category="AI|PathFollowing")
	bool IsMoving() const { return Status == EPathFollowingStatus::Moving; }
	UFUNCTION(Pure, Category="AI|PathFollowing")
	bool HasValidPath() const { return Path.IsValid(); }
	UFUNCTION(Pure, Category="AI|PathFollowing")
	int32 GetCurrentPathIndex() const { return CurrentPathIndex; }
	UFUNCTION(Pure, Category="AI|PathFollowing")
	FVector GetCurrentPathPoint() const;
	UFUNCTION(Pure, Category="AI|PathFollowing")
	float GetRemainingDistance() const;
	UFUNCTION(Pure, Category="AI|PathFollowing")
	int32 GetPathPointCount() const { return Path.Num(); }
	UFUNCTION(Pure, Category="AI|PathFollowing")
	EPathFollowingResult GetLastMoveResult() const { return LastMoveResult; }
	UFUNCTION(Pure, Category="AI|PathFollowing")
	int32 GetLastPathPointCount() const { return LastPathPointCount; }
	UFUNCTION(Pure, Category="AI|PathFollowing")
	FVector GetLastMoveGoalLocation() const { return LastMoveGoalLocation; }

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Path Point Acceptance Radius", Min=0.05f, Max=50.0f, Speed=0.05f)
	float PathPointAcceptanceRadius = 1.0f;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Default Acceptance Radius", Min=0.05f, Max=50.0f, Speed=0.05f)
	float DefaultAcceptanceRadius = 3.0f;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Move Input Scale", Min=0.0f, Max=1.0f, Speed=0.01f)
	float MoveInputScale = 1.0f;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Blocked Timeout", Min=0.0f, Max=10.0f, Speed=0.1f)
	float BlockedTimeout = 1.2f;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Blocked Min Move Distance", Min=0.0f, Max=10.0f, Speed=0.01f)
	float BlockedMinMoveDistance = 0.15f;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Use Agent Radius For Corner Acceptance")
	bool bUseAgentRadiusForCornerAcceptance = true;
	// 회귀 수정(8f399bb): 코너 수용 반경이 0.45×반경≈0.19m 로 너무 작아, 정지거리(≈1.8m)로
	// 달리는 에이전트가 웨이포인트를 지나쳐도 "도착" 판정이 안 나 공전(루프)했다. 작동 가능한
	// 값으로 키운다(반경 ≈ 0.46m, 최소 0.5m). 도착/코너 감속(아래)과 함께 루프를 없앤다.
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Corner Acceptance Radius Scale", Min=0.05f, Max=4.0f, Speed=0.01f)
	float CornerAcceptanceRadiusScale = 1.1f;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Min Corner Acceptance Radius", Min=0.02f, Max=10.0f, Speed=0.01f)
	float MinCornerAcceptanceRadius = 0.5f;
	// ── 도착/코너 감속 (관성 overshoot 로 인한 공전/루프 방지) ──
	// 풀입력으로 달리면 정지거리(≈v²/2·brakingFriction)가 수용 반경보다 커서 목표를 지나쳐 되돌아오며
	// 원을 그린다. 최종 목표 가까이서, 또 급회전 직전에 입력을 줄여 미리 감속한다.
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Arrival Slowdown Radius", Min=0.0f, Max=20.0f, Speed=0.05f)
	float ArrivalSlowdownRadius = 2.0f;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Corner Slowdown Radius", Min=0.0f, Max=20.0f, Speed=0.05f)
	float CornerSlowdownRadius = 1.5f;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing", DisplayName="Min Arrival Input Scale", Min=0.02f, Max=1.0f, Speed=0.01f)
	float MinArrivalScale = 0.15f;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing|Debug", DisplayName="Draw Path On Request")
	bool bDrawDebugPathOnRequest = true;
	UPROPERTY(Edit, Save, Category="AI|PathFollowing|Debug", DisplayName="Debug Path Draw Duration", Min=0.0f, Max=60.0f, Speed=0.1f)
	float DebugPathDrawDuration = 5.0f;

	FPathFollowingFinishedSignature OnMoveFinished;

private:
	AAIController* GetControllerOwner() const;
	APawn* GetPawnOwner() const;
	ACharacter* GetCharacterOwner() const;
	float GetOwnerAgentRadius() const;
	float GetEffectivePathPointAcceptanceRadius(bool bFinalPoint) const;
	void AdvancePathIfNeeded();

	FNavigationPath Path;
	EPathFollowingStatus Status = EPathFollowingStatus::Idle;
	int32 CurrentPathIndex = 0;
	float AcceptanceRadius = 3.0f;
	FVector LastMoveLocation = FVector::ZeroVector;
	FVector LastMoveGoalLocation = FVector::ZeroVector;
	EPathFollowingResult LastMoveResult = EPathFollowingResult::Invalid;
	int32 LastPathPointCount = 0;
	float BlockedTime = 0.0f;
};
