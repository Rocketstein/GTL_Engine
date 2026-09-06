#include "Component/Combat/CombatHitEventComponent.h"

#include "Component/Combat/CombatFeedbackComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "GameFramework/AActor.h"
#include "Profiling/Stats/Stats.h"

UCombatHitEventComponent* UCombatHitEventComponent::FindOrCreate(AActor* OwnerActor)
{
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	if (UCombatHitEventComponent* Existing = OwnerActor->GetComponentByClass<UCombatHitEventComponent>())
	{
		return Existing;
	}

	return OwnerActor->AddComponent<UCombatHitEventComponent>();
}

void UCombatHitEventComponent::BroadcastImpactForActor(AActor* OwnerActor, const FCombatImpactEvent& ImpactEvent)
{
	UCombatHitEventComponent* HitEventComponent = FindOrCreate(OwnerActor);
	if (!HitEventComponent)
	{
		return;
	}

	UCombatFeedbackComponent::EnsureForActor(OwnerActor);
	HitEventComponent->BroadcastCombatImpact(ImpactEvent);
}

void UCombatHitEventComponent::BroadcastAttackHit(AActor* Target, UPrimitiveComponent* HitComponent, const FCombatDamageSpec& DamageSpec, FName HitEventName)
{
	SCOPE_STAT_CAT("CombatHitEvent.BroadcastAttackHit", "Combat");
	OnAttackHit.Broadcast(this, GetOwner(), Target, HitComponent, DamageSpec, HitEventName);
}

void UCombatHitEventComponent::BroadcastAttackParried(AActor* Defender, UPrimitiveComponent* HitComponent, const FCombatDamageSpec& DamageSpec, FName HitEventName)
{
	SCOPE_STAT_CAT("CombatHitEvent.BroadcastAttackParried", "Combat");
	OnAttackParried.Broadcast(this, GetOwner(), Defender, HitComponent, DamageSpec, HitEventName);
}

void UCombatHitEventComponent::BroadcastCombatImpact(const FCombatImpactEvent& ImpactEvent)
{
	SCOPE_STAT_CAT("CombatHitEvent.BroadcastCombatImpact", "Combat");
	OnCombatImpact.Broadcast(this, ImpactEvent);
}

FCombatDamageReport UCombatHitEventComponent::ApplyDamageToTarget(AActor* Target, float Damage, float PoiseDamage, AActor* DamageCauser, AActor* InstigatorActor)
{
	FCombatDamageSpec DamageSpec;
	DamageSpec.Damage = Damage;
	DamageSpec.PoiseDamage = PoiseDamage;
	DamageSpec.DamageCauser = DamageCauser ? DamageCauser : GetOwner();
	DamageSpec.InstigatorActor = InstigatorActor ? InstigatorActor : GetOwner();

	return ApplyDamageSpecToTarget(Target, DamageSpec);
}

FCombatDamageReport UCombatHitEventComponent::ApplyDamageSpecToTarget(AActor* Target, const FCombatDamageSpec& DamageSpec)
{
	SCOPE_STAT_CAT("CombatHitEvent.ApplyDamageSpecToTarget", "Combat");
	FCombatDamageReport Report;
	Report.RequestedDamage = DamageSpec.Damage;

	if (!IsValid(Target))
	{
		Report.Result = ECombatDamageResult::Rejected;
		return Report;
	}

	UHealthComponent* HealthComponent = Target->GetComponentByClass<UHealthComponent>();
	if (!HealthComponent)
	{
		Report.Result = ECombatDamageResult::Rejected;
		return Report;
	}

	return HealthComponent->ApplyDamageSpec(DamageSpec);
}
