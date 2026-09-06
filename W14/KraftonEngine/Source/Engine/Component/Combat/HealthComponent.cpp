#include "Component/Combat/HealthComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/Notify/AnimNotifyState_ParryWindow.h"
#include "Component/Combat/CombatStateComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Profiling/Stats/Stats.h"

#include <cmath>

namespace
{
	UAnimInstance* GetActorAnimInstance(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		USkeletalMeshComponent* Mesh = Actor->GetComponentByClass<USkeletalMeshComponent>();
		return IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	}
}

void UHealthComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	if (MaxHealth <= 0.0f)
	{
		MaxHealth = 1.0f;
	}
	if (bInitializeFullHealthOnBeginPlay)
	{
		CurrentHealth = MaxHealth;
		bDead = false;
	}
	else
	{
		CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
		bDead = CurrentHealth <= 0.0f;
	}
}

FCombatDamageReport UHealthComponent::ApplyDamage(float Damage, AActor* DamageCauser, AActor* InstigatorActor)
{
	FCombatDamageSpec Spec;
	Spec.Damage = Damage;
	Spec.PoiseDamage = 0.0f;
	Spec.DamageCauser = DamageCauser;
	Spec.InstigatorActor = InstigatorActor;
	return ApplyDamageSpec(Spec);
}

FCombatDamageReport UHealthComponent::ApplyDamageSpec(const FCombatDamageSpec& DamageSpec)
{
	SCOPE_STAT_CAT("Health.ApplyDamageSpec", "Combat");
	FCombatDamageReport Report;
	Report.RequestedDamage = DamageSpec.Damage;
	Report.PreviousHealth = CurrentHealth;
	Report.NewHealth = CurrentHealth;

	if (DamageSpec.Damage <= 0.0f || bDead || bInvincible)
	{
		Report.Result = ECombatDamageResult::Rejected;
		return Report;
	}

	AActor* OwnerActor = GetOwner();
	float EffectiveDamage = DamageSpec.Damage;
	float EffectivePoiseDamage = DamageSpec.PoiseDamage;
	if (OwnerActor)
	{
		if (UCombatStateComponent* CombatState = OwnerActor->GetComponentByClass<UCombatStateComponent>())
		{
			if (!CombatState->CanReceiveDamage())
			{
				Report.Result = ECombatDamageResult::Rejected;
				return Report;
			}
			// 탄기 윈도우가 열려 있으면 받아넘김 — Perfect/Good 은 피해 무효 + 공격자 체간 반사,
			// Late 는 약화(chip)되어 통과. (윈도우 없으면 종전 동작 그대로 — 비파괴.)
			if (CombatState->IsDeflecting())
			{
				SCOPE_STAT_CAT("Health.ResolveDeflect", "Combat");
				AActor* Attacker = DamageSpec.InstigatorActor ? DamageSpec.InstigatorActor : DamageSpec.DamageCauser;
				const EDeflectGrade Grade = CombatState->ConsumeDeflect(Attacker);
				if (Grade == EDeflectGrade::Perfect || Grade == EDeflectGrade::Good)
				{
					UAnimNotifyState_ParryWindow::ReportCounterOpportunity(
						GetActorAnimInstance(OwnerActor),
						Attacker,
						DamageSpec.HitLocation);
					Report.Result = ECombatDamageResult::Deflected;
					Report.NewHealth = CurrentHealth;
					return Report;
				}
				if (Grade == EDeflectGrade::Late)
				{
					EffectiveDamage *= 0.5f;
					EffectivePoiseDamage *= 0.5f;
				}
			}
			// 가드(block) — 보스는 패링 대신 막기만 한다. 정면(GuardMaxAbsAngle 이내) 공격의
			// 피해/체간을 감쇄하되 공격자에게 반사는 없다. 측후면 공격은 가드를 관통한다.
			else if (CombatState->IsGuarding())
			{
				FVector ToAttacker = DamageSpec.HitDirection * -1.0f;
				ToAttacker.Z = 0.0f;
				bool bFrontal = true;
				if (OwnerActor && !ToAttacker.IsNearlyZero())
				{
					FVector Forward = OwnerActor->GetActorForward();
					Forward.Z = 0.0f;
					if (!Forward.IsNearlyZero())
					{
						const float Dot = Forward.Normalized().Dot(ToAttacker.Normalized());
						const float CosLimit = std::cos(CombatState->GuardMaxAbsAngle * 3.14159265f / 180.0f);
						bFrontal = Dot >= CosLimit;
					}
				}
				if (bFrontal)
				{
					EffectiveDamage *= CombatState->GuardDamageMultiplier;
					EffectivePoiseDamage *= CombatState->GuardPoiseMultiplier;
					CombatState->NotifyGuardBlocked();   // 실제로 막았음 → AI 반격(riposte) 트리거
				}
			}
		}
	}

	SetHealthValue(CurrentHealth - EffectiveDamage, false);
	Report.NewHealth = CurrentHealth;
	Report.AppliedDamage = Report.PreviousHealth - Report.NewHealth;

	if (OwnerActor)
	{
		if (UCombatStateComponent* CombatState = OwnerActor->GetComponentByClass<UCombatStateComponent>())
		{
			CombatState->ApplyPoiseDamage(EffectivePoiseDamage);
		}
	}

	if (CurrentHealth <= 0.0f)
	{
		Report.Result = ECombatDamageResult::Killed;
		Report.bKilled = true;
	}
	else
	{
		Report.Result = ECombatDamageResult::Damaged;
	}

	OnDamageApplied.Broadcast(this, Report, DamageSpec);
	OnDamaged.Broadcast(this, Report.AppliedDamage, CurrentHealth, DamageSpec.DamageCauser, DamageSpec.InstigatorActor);

	if (Report.bKilled)
	{
		BroadcastDeathOnce(DamageSpec.DamageCauser, DamageSpec.InstigatorActor);
	}

	return Report;
}

float UHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.0f || bDead)
	{
		return 0.0f;
	}
	const float Previous = CurrentHealth;
	SetHealthValue(CurrentHealth + Amount, false);
	return CurrentHealth - Previous;
}

void UHealthComponent::SetHealth(float NewHealth)
{
	SetHealthValue(NewHealth, true);
	if (CurrentHealth > 0.0f)
	{
		return;
	}
	BroadcastDeathOnce(nullptr, nullptr);
}

void UHealthComponent::Kill(AActor* DamageCauser, AActor* InstigatorActor)
{
	SetHealthValue(0.0f, false);
	BroadcastDeathOnce(DamageCauser, InstigatorActor);
}

void UHealthComponent::ResetHealth()
{
	if (MaxHealth <= 0.0f)
	{
		MaxHealth = 1.0f;
	}
	SetHealthValue(MaxHealth, true);
}

float UHealthComponent::GetHealthRatio() const
{
	if (MaxHealth <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f);
}

bool UHealthComponent::SetHealthValue(float NewHealth, bool bReviveIfPositive)
{
	const float PreviousHealth = CurrentHealth;
	const float ValidMaxHealth = MaxHealth > 0.0f ? MaxHealth : 1.0f;
	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, ValidMaxHealth);

	if (bReviveIfPositive && CurrentHealth > 0.0f)
	{
		bDead = false;
	}

	if (CurrentHealth == PreviousHealth)
	{
		return false;
	}

	OnHealthChanged.Broadcast(this, PreviousHealth, CurrentHealth, ValidMaxHealth);
	return true;
}

void UHealthComponent::BroadcastDeathOnce(AActor* DamageCauser, AActor* InstigatorActor)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	CurrentHealth = 0.0f;
	OnDeath.Broadcast(this, DamageCauser, InstigatorActor);
}
