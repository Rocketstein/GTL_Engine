#pragma once

#include "Component/ActorComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Core/Delegate.h"
#include "Object/FName.h"

#include "Source/Engine/Component/AI/EnemyAttackComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FEnemyAttackCommittedSignature, class UEnemyAttackComponent*, FName);

USTRUCT()
struct FEnemyAttackCooldownEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient, Category="Attack")
	FName AttackName = FName::None;

	UPROPERTY(Transient, Category="Attack")
	float RemainingTime = 0.0f;
};

UCLASS()
class UEnemyAttackComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UEnemyAttackComponent() = default;
	~UEnemyAttackComponent() override = default;

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	UFUNCTION(Pure, Category="EnemyAI|Attack")
	bool IsAttackOnCooldown(const FName& AttackName) const;
	UFUNCTION(Pure, Category="EnemyAI|Attack")
	float GetAttackCooldownRemaining(const FName& AttackName) const;
	UFUNCTION(Pure, Category="EnemyAI|Attack")
	float GetGlobalCooldownRemaining() const { return GlobalCooldownRemaining; }
	UFUNCTION(Callable, Category="EnemyAI|Attack")
	void SetAttackCooldown(const FName& AttackName, float Cooldown);
	UFUNCTION(Callable, Category="EnemyAI|Attack")
	void ClearAttackCooldowns();
	UFUNCTION(Callable, Category="EnemyAI|Attack")
	void RestartAttackCooldowns();

	UFUNCTION(Pure, Category="EnemyAI|Attack")
	bool CanUseAttack(const FEnemyAttackData& Attack, int32 Phase, float Distance, float AbsAngle) const;
	UFUNCTION(Callable, Category="EnemyAI|Attack")
	bool CommitAttack(const FName& AttackName);
	UFUNCTION(Callable, Category="EnemyAI|Attack")
	bool CommitAttackData(const FEnemyAttackData& Attack);
	UFUNCTION(Pure, Category="EnemyAI|Attack")	
	FEnemyAttackData FindAttackByName(const FName& AttackName) const;
	UFUNCTION(Pure, Category="EnemyAI|Attack")
	FName GetLastAttackName() const { return LastAttackName; }

	UPROPERTY(Edit, Save, Category="EnemyAI|Attack", DisplayName="Attacks", Type=Array)
	TArray<FEnemyAttackData> Attacks;

	UPROPERTY(Edit, Save, Category="EnemyAI|Attack", DisplayName="Global Cooldown", Min=0.0f, Max=60.0f, Speed=0.05f)
	float GlobalCooldown = 0.3f;

	UPROPERTY(Edit, Save, Category="EnemyAI|Attack", DisplayName="Recent Attack Memory", Min=0.0f, Max=16.0f, Speed=1.0f)
	int32 RecentAttackMemory = 2;

	FEnemyAttackCommittedSignature OnAttackCommitted;

private:
	int32 FindCooldownIndex(const FName& AttackName) const;
	float GlobalCooldownRemaining = 0.0f;
	TArray<FEnemyAttackCooldownEntry> Cooldowns;
	TArray<FName> RecentAttacks;
	FName LastAttackName = FName::None;
};
