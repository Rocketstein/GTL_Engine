#pragma once

#include "GameFramework/Controller/Controller.h"
#include "Navigation/NavTypes.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AActor;
class UPathFollowingComponent;
class UNavigationSystem;

#include "Source/Engine/GameFramework/Controller/AIController.generated.h"

UCLASS()
class AAIController : public AController
{
public:
	GENERATED_BODY()
	AAIController() = default;
	~AAIController() override = default;

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	UFUNCTION(Callable, Category="AIController|Movement")
	EPathFollowingRequestResult MoveToActor(AActor* Goal, float AcceptanceRadius = 3.0f, bool bStopOnOverlap = true, bool bUsePathfinding = true);
	UFUNCTION(Callable, Category="AIController|Movement")
	EPathFollowingRequestResult MoveToLocation(const FVector& GoalLocation, float AcceptanceRadius = 3.0f, bool bUsePathfinding = true);
	UFUNCTION(Callable, Category="AIController|Movement")
	void StopMovement();

	UFUNCTION(Pure, Category="AIController|Movement")
	UPathFollowingComponent* GetPathFollowingComponent() const { return PathFollowingComponent; }
	UFUNCTION(Pure, Category="AIController|Movement")
	EPathFollowingStatus GetMoveStatus() const;
	UFUNCTION(Pure, Category="AIController|Movement")
	bool IsFollowingPath() const;
	UFUNCTION(Pure, Category="AIController|Movement")
	AActor* GetMoveGoalActor() const { return MoveGoalActor.Get(); }
	UFUNCTION(Pure, Category="AIController|Movement")
	EPathFollowingRequestResult GetLastMoveRequestResult() const { return LastMoveRequestResult; }
	UFUNCTION(Pure, Category="AIController|Movement")
	EPathFollowingResult GetLastMoveResult() const;
	UFUNCTION(Pure, Category="AIController|Movement")
	FString GetLastMoveFailureReason() const { return LastMoveFailureReason; }
	UFUNCTION(Pure, Category="AIController|Movement")
	int32 GetCurrentPathPointCount() const;
	// 현재(가장 최근 계산된) 경로가 목표에 완전히 닿지 못한 부분 경로인가. 재사용 시에도 직전 계산값을
	// 유지한다 — 추격 AI가 "슬롯이 도달 불가(partial)면 타깃 본체로 폴백" 같은 판단에 쓴다.
	UFUNCTION(Pure, Category="AIController|Movement")
	bool WasLastPathPartial() const { return bLastPathWasPartial; }

	UFUNCTION(Callable, Category="AIController|Navigation")
	void SetRepathInterval(float InInterval) { RepathInterval = InInterval < 0.0f ? 0.0f : InInterval; }
	UFUNCTION(Pure, Category="AIController|Navigation")
	FNavAgentProperties GetNavAgentProperties() const { return NavAgentProperties; }
	UFUNCTION(Callable, Category="AIController|Navigation")
	void SetNavAgentProperties(const FNavAgentProperties& InProps) { NavAgentProperties = InProps; }

	UPROPERTY(Edit, Save, Category="AIController|Navigation", DisplayName="Nav Agent")
	FNavAgentProperties NavAgentProperties;

	UPROPERTY(Edit, Save, Category="AIController|Navigation", DisplayName="Repath Interval", Min=0.0f, Max=10.0f, Speed=0.1f)
	float RepathInterval = 0.35f;
	UPROPERTY(Edit, Save, Category="AIController|Navigation", DisplayName="Goal Move Repath Distance", Min=0.0f, Max=100.0f, Speed=0.1f)
	float GoalRepathDistance = 2.0f;
	UPROPERTY(Edit, Save, Category="AIController|Navigation", DisplayName="Reuse Current Path For Same Goal")
	bool bReuseCurrentPathForSameGoal = true;
	// 부분 경로 허용: 목표 셀까지 완전히 닿지 못해도(예: 플레이어가 벽에 붙어 목표 셀이 막힘) 갈 수
	// 있는 데까지 이동한다. false 면 부분 경로를 통째로 버려 추격이 그 자리에서 멈춘다 — 전투 AI는
	// "최대한 접근"이 옳으므로 기본 허용. 길이 있어도 멈춰버리던 증상의 한 원인.
	UPROPERTY(Edit, Save, Category="AIController|Navigation", DisplayName="Allow Partial Path")
	bool bAllowPartialPath = true;

protected:
	void OnPossess(APawn* Pawn) override;
	void OnUnPossess(APawn* OldPawn) override;

private:
	UNavigationSystem* GetNavigationSystem() const;
	void RefreshNavAgentPropertiesFromPawn(APawn* PawnOverride = nullptr);
	EPathFollowingRequestResult RequestMoveToCurrentGoal(bool bUsePathfinding);
	bool ShouldRepathToMovingGoal() const;
	bool CanReuseCurrentMove(AActor* NewGoalActor, const FVector& NewGoalLocation, float NewAcceptanceRadius, bool bUsePathfinding) const;
	EPathFollowingRequestResult SetLastMoveRequestResult(EPathFollowingRequestResult Result, const FString& Reason = FString());

	TWeakObjectPtr<UPathFollowingComponent> PathFollowingComponent = nullptr;
	TWeakObjectPtr<AActor> MoveGoalActor = nullptr;
	FVector MoveGoalLocation = FVector::ZeroVector;
	FVector LastRequestedGoalLocation = FVector::ZeroVector;
	float MoveAcceptanceRadius = 3.0f;
	float TimeSinceLastRepath = 0.0f;
	bool bCurrentMoveUsesPathfinding = true;
	EPathFollowingRequestResult LastMoveRequestResult = EPathFollowingRequestResult::Failed;
	FString LastMoveFailureReason;
	bool bLastPathWasPartial = false;
};
