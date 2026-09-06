#pragma once

#include "Component/ActorComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Core/Delegate.h"

class AActor;

#include "Source/Engine/Component/Combat/HealthComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_FiveParams(FHealthDamagedSignature, class UHealthComponent*, float, float, AActor*, AActor*);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FHealthDeathSignature, class UHealthComponent*, AActor*, AActor*);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FHealthDamageAppliedSignature, class UHealthComponent*, const FCombatDamageReport&, const FCombatDamageSpec&);
DECLARE_MULTICAST_DELEGATE_FourParams(FHealthChangedSignature, class UHealthComponent*, float, float, float);

UCLASS()
class UHealthComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UHealthComponent() = default;
	~UHealthComponent() override = default;

	void BeginPlay() override;

	UFUNCTION(Callable, Category="Combat|Health")
	FCombatDamageReport ApplyDamage(float Damage, AActor* DamageCauser = nullptr, AActor* InstigatorActor = nullptr);
	UFUNCTION(Callable, Category="Combat|Health")
	FCombatDamageReport ApplyDamageSpec(const FCombatDamageSpec& DamageSpec);
	UFUNCTION(Callable, Category="Combat|Health")
	float Heal(float Amount);
	UFUNCTION(Callable, Category="Combat|Health")
	void SetHealth(float NewHealth);
	UFUNCTION(Callable, Category="Combat|Health")
	void Kill(AActor* DamageCauser = nullptr, AActor* InstigatorActor = nullptr);
	UFUNCTION(Callable, Category="Combat|Health")
	void ResetHealth();

	UFUNCTION(Pure, Category="Combat|Health")
	bool IsDead() const { return bDead; }
	UFUNCTION(Pure, Category="Combat|Health")
	bool IsAlive() const { return !bDead && CurrentHealth > 0.0f; }
	UFUNCTION(Pure, Category="Combat|Health")
	float GetCurrentHealth() const { return CurrentHealth; }
	UFUNCTION(Pure, Category="Combat|Health")
	float GetMaxHealth() const { return MaxHealth; }
	UFUNCTION(Pure, Category="Combat|Health")
	float GetHealthRatio() const;

	UFUNCTION(Callable, Category="Combat|Health")
	void SetInvincible(bool bInInvincible) { bInvincible = bInInvincible; }
	UFUNCTION(Pure, Category="Combat|Health")
	bool IsInvincible() const { return bInvincible; }

	UPROPERTY(Edit, Save, Category="Combat|Health", DisplayName="Max Health", Min=1.0f, Max=1000000.0f, Speed=1.0f)
	float MaxHealth = 100.0f;

	UPROPERTY(Edit, Save, Category="Combat|Health", DisplayName="Current Health", Min=0.0f, Max=1000000.0f, Speed=1.0f)
	float CurrentHealth = 100.0f;

	UPROPERTY(Edit, Save, Category="Combat|Health", DisplayName="Initialize Full Health On Begin Play")
	bool bInitializeFullHealthOnBeginPlay = true;

	UPROPERTY(Edit, Save, Category="Combat|Health", DisplayName="Invincible")
	bool bInvincible = false;

	UPROPERTY(Transient, Category="Combat|Health")
	bool bDead = false;

	FHealthDamagedSignature OnDamaged;
	FHealthDeathSignature OnDeath;
	FHealthDamageAppliedSignature OnDamageApplied;
	FHealthChangedSignature OnHealthChanged;

private:
	bool SetHealthValue(float NewHealth, bool bReviveIfPositive);
	void BroadcastDeathOnce(AActor* DamageCauser, AActor* InstigatorActor);
};
