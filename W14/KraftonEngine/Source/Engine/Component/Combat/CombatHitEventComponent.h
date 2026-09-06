#pragma once

#include "Component/ActorComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Core/Delegate.h"
#include "Object/FName.h"

class AActor;
class UPrimitiveComponent;

#include "Source/Engine/Component/Combat/CombatHitEventComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_SixParams(FCombatAttackHitSignature, class UCombatHitEventComponent*, AActor*, AActor*, UPrimitiveComponent*, const FCombatDamageSpec&, FName);
DECLARE_MULTICAST_DELEGATE_SixParams(FCombatAttackParriedSignature, class UCombatHitEventComponent*, AActor*, AActor*, UPrimitiveComponent*, const FCombatDamageSpec&, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FCombatImpactSignature, class UCombatHitEventComponent*, const FCombatImpactEvent&);

UCLASS()
class UCombatHitEventComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UCombatHitEventComponent() = default;
	~UCombatHitEventComponent() override = default;

	static UCombatHitEventComponent* FindOrCreate(AActor* OwnerActor);
	static void BroadcastImpactForActor(AActor* OwnerActor, const FCombatImpactEvent& ImpactEvent);

	void BroadcastAttackHit(AActor* Target, UPrimitiveComponent* HitComponent, const FCombatDamageSpec& DamageSpec, FName HitEventName);
	void BroadcastAttackParried(AActor* Defender, UPrimitiveComponent* HitComponent, const FCombatDamageSpec& DamageSpec, FName HitEventName);
	void BroadcastCombatImpact(const FCombatImpactEvent& ImpactEvent);

	UFUNCTION(Callable, Category="Combat|HitEvent")
	FCombatDamageReport ApplyDamageToTarget(AActor* Target, float Damage, float PoiseDamage = 0.0f, AActor* DamageCauser = nullptr, AActor* InstigatorActor = nullptr);

	UFUNCTION(Callable, Category="Combat|HitEvent")
	FCombatDamageReport ApplyDamageSpecToTarget(AActor* Target, const FCombatDamageSpec& DamageSpec);

	FCombatAttackHitSignature OnAttackHit;
	FCombatAttackParriedSignature OnAttackParried;
	FCombatImpactSignature OnCombatImpact;
};
