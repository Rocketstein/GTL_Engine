#pragma once

#include "GameFramework/Pawn/EnemyCharacter.h"
#include "Object/Ptr/WeakObjectPtr.h"

class UEncounterComponent;
class UPhaseComponent;
class FArchive;

#include "Source/Engine/GameFramework/Pawn/BossEnemyCharacter.generated.h"

UCLASS()
class ABossEnemyCharacter : public AEnemyCharacter
{
public:
	GENERATED_BODY()
	ABossEnemyCharacter() = default;
	~ABossEnemyCharacter() override = default;

	void InitDefaultComponents(const FString& SkeletalMeshFileName) override;
	void InitDefaultComponents(const FString& SkeletalMeshFileName, const FString& ScriptFile);
	void PostDuplicate() override;
	void OnPostLoad(FArchive& Ar) override;

	UFUNCTION(Pure, Category="Boss|Components")
	UPhaseComponent* GetPhaseComponent() const { return PhaseComponent; }
	UFUNCTION(Pure, Category="Boss|Components")
	UEncounterComponent* GetEncounterComponent() const { return EncounterComponent; }

	UFUNCTION(Callable, Category="Boss|Encounter")
	bool StartBossEncounter();
	UFUNCTION(Callable, Category="Boss|Encounter")
	bool CompleteBossEncounter();
	UFUNCTION(Pure, Category="Boss|Encounter")
	bool IsBossEncounterActive() const;

	UFUNCTION(Callable, Category="Boss|Phase")
	bool SetBossPhase(int32 NewPhase);
	UFUNCTION(Pure, Category="Boss|Phase")
	int32 GetBossPhase() const;

	UFUNCTION(Callable, Category="Boss|Execution")
	void BeginExecutionPriorityWindow();
	UFUNCTION(Callable, Category="Boss|Execution")
	void EndExecutionPriorityWindow();
	UFUNCTION(Pure, Category="Boss|Execution")
	bool IsExecutionPriorityWindowActive() const { return bExecutionPriorityWindowActive; }

	// 페이즈 전환 연출: 자세변경 플러리시 몽타주(예: Equip Over Shoulder). null 이면 상태 전환만.
	UPROPERTY(Edit, Save, Category="Boss|Phase", DisplayName="Phase Change Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* PhaseChangeMontage = nullptr;

	// 페이즈 전환 연출 동안의 무적 시간(초). 연출 중 피격으로 끊기는 것을 막는다.
	UPROPERTY(Edit, Save, Category="Boss|Phase", DisplayName="Phase Change Invuln Seconds", Min=0.0f, Max=10.0f, Speed=0.05f)
	float PhaseChangeInvulnSeconds = 1.4f;

	UPROPERTY(Edit, Save, Category="Boss|Death", DisplayName="Boss Death Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* BossDeathMontage = nullptr;

	UPROPERTY(Edit, Save, Category="Boss|Death", DisplayName="Boss Death Montage Play Rate", Min=0.01f, Max=10.0f, Speed=0.05f)
	float BossDeathMontagePlayRate = 1.0f;

	// 처형(데스블로우)으로 스톡 1개를 소비하고도 살아남았을 때(마지막 스톡 아님), 보스가 잠깐 행동을
	// 못 하는 "딜 타임"(초). 그 동안 플레이어가 추가로 때릴 수 있다(요청 #2). 0=비활성.
	UPROPERTY(Edit, Save, Category="Boss|Execution", DisplayName="Execution Deal Time Seconds", Min=0.0f, Max=10.0f, Speed=0.05f)
	float ExecutionDealTimeSeconds = 2.0f;
protected:
	int32 GetCurrentAIPhase() const override { return GetBossPhase(); }
	bool IsBossCharacter() const override { return true; }
	void HandleDamaged(UHealthComponent* Component, float Damage, float NewHealth, AActor* DamageCauser, AActor* InstigatorActor) override;
	void HandleDeath(UHealthComponent* Component, AActor* DamageCauser, AActor* InstigatorActor) override;
	void HandlePhaseChanged(UPhaseComponent* Component, int32 OldPhase, int32 NewPhase);
	void HandleEncounterStarted(UEncounterComponent* Component);
	void HandleEncounterCompleted(UEncounterComponent* Component);
	void RebindBossComponents();
	bool RequestBossPhase(int32 NewPhase);
	void ApplyDeferredBossPhaseChange();

	TWeakObjectPtr<UPhaseComponent> PhaseComponent = nullptr;
	TWeakObjectPtr<UEncounterComponent> EncounterComponent = nullptr;
	bool bExecutionPriorityWindowActive = false;
	int32 PendingExecutionPhase = 0;
};
