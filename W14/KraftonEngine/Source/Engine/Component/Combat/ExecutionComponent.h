#pragma once

#include "Component/ActorComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Core/Delegate.h"

class AActor;
class UCombatStateComponent;
class UHealthComponent;

#include "Source/Engine/Component/Combat/ExecutionComponent.generated.h"

// =============================================================================
//  UExecutionComponent — 데스블로우 스톡/처형 승패 구조
// =============================================================================
//  세키로 핵심: 체력 0 즉사가 아니라, 자세 붕괴 → 처형 창(DeathblowReady) → 스톡 소비.
//  졸개는 스톡 1개라 처형 1회로 사망, 강적은 여러 스톡을 거친다. 스톡을 소비하면
//  자세/체력/상태를 재초기화하고 다음 단계로 넘어간다(페이즈 전환의 트리거).
//
//  배선: 형제 UCombatStateComponent 의 OnPostureBroken 을 받아 창을 연다. HP 관리는
//  HealthComponent 가 계속 전담하고, 이 컴포넌트는 "처형으로 죽인다"만 담당한다.
// =============================================================================

DECLARE_MULTICAST_DELEGATE_OneParam(FExecutionStateSignature, class UExecutionComponent*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FExecutionStockSignature, class UExecutionComponent*, int32);

UCLASS()
class UExecutionComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UExecutionComponent()           = default;
	~UExecutionComponent() override = default;

	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	// 자세 붕괴 알림 — CombatState->OnPostureBroken 바인딩으로 자동 호출되며, Lua 정책이 직접 호출해도 된다.
	UFUNCTION(Callable, Category="Combat|Execution")
	void NotifyPostureBroken();

	// 처형 수행 — 데스블로우 창이 열려 있을 때만 성공. 마지막 스톡이면 사망, 아니면 다음 스톡 재초기화.
	UFUNCTION(Callable, Category="Combat|Execution")
	bool PerformDeathblow(AActor* Executor);

	// 스텔스 처형 — 인지하지 못한 적은 창 없이도 스톡을 직접 소비한다.
	UFUNCTION(Callable, Category="Combat|Execution")
	bool PerformStealthDeathblow(AActor* Executor);

	// 스톡/창/상태를 초기값으로 되돌린다(리스폰/리셋용).
	UFUNCTION(Callable, Category="Combat|Execution")
	void ResetExecution();

	UFUNCTION(Pure, Category="Combat|Execution")
	bool IsDeathblowReady() const { return ExecutionState == EExecutionState::DeathblowReady; }
	UFUNCTION(Pure, Category="Combat|Execution")
	int32 GetCurrentStocks() const { return CurrentStocks; }
	UFUNCTION(Pure, Category="Combat|Execution")
	int32 GetMaxStocks() const { return MaxStocks; }
	UFUNCTION(Pure, Category="Combat|Execution")
	float GetExecutionWindowRemaining() const { return ExecutionWindowRemaining; }
	UFUNCTION(Pure, Category="Combat|Execution")
	int32 GetExecutionStateInt() const { return static_cast<int32>(ExecutionState); }

	// 강적의 스톡 수. 졸개=1(처형 즉사), 미니보스/보스=2+.
	UPROPERTY(Edit, Save, Category="Combat|Execution", DisplayName="Max Stocks", Min=1.0f, Max=16.0f, Speed=1.0f)
	int32 MaxStocks = 1;

	// 자세 붕괴 후 처형이 가능한 창의 길이(초). 만료되면 자세를 회복해 전투로 복귀한다.
	UPROPERTY(Edit, Save, Category="Combat|Execution", DisplayName="Execution Window Seconds", Min=0.1f, Max=15.0f, Speed=0.05f)
	float ExecutionWindowSeconds = 3.0f;

	// 스톡을 소비하고 다음 단계로 넘어갈 때 회복할 체력 비율(0=회복 없음). 보스 연출용.
	UPROPERTY(Edit, Save, Category="Combat|Execution", DisplayName="Health Fraction Restored On Stock Spent", Min=0.0f, Max=1.0f, Speed=0.01f)
	float HealthFractionRestoredOnStockSpent = 0.0f;

	// 스톡 소비 시 자세를 가득 채워 다음 단계를 시작할지.
	UPROPERTY(Edit, Save, Category="Combat|Execution", DisplayName="Reset Posture On Stock Spent")
	bool bResetPostureOnStockSpent = true;

	UPROPERTY(Transient, Category="Combat|Execution")
	int32 CurrentStocks = 1;

	FExecutionStateSignature OnDeathblowReady;        // 처형 창이 열림
	FExecutionStockSignature OnStockSpent;            // 스톡 소비됨(인자=남은 스톡)
	FExecutionStateSignature OnExecuted;              // 마지막 스톡 소비 — 사망
	FExecutionStateSignature OnExecutionWindowExpired; // 창 만료 — 전투 복귀

private:
	void SpendStock(AActor* Executor);
	UCombatStateComponent* GetCombatState() const;
	UHealthComponent* GetHealth() const;
	void HandlePostureBroken(UCombatStateComponent* Combat);

	EExecutionState ExecutionState   = EExecutionState::None;
	float           ExecutionWindowRemaining = 0.0f;
	FDelegateHandle PostureBrokenHandle;
};
