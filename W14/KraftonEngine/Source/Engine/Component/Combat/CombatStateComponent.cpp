#include "Component/Combat/CombatStateComponent.h"

#include "AI/CombatTargetRegistry.h"
#include "Component/Combat/HealthComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>

namespace
{
	float GetOwnerGameTimeSeconds(const UActorComponent* Component)
	{
		AActor* OwnerActor = Component ? Component->GetOwner() : nullptr;
		UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
		return World ? World->GetGameTimeSeconds() : 0.0f;
	}
}

void UCombatStateComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	if (MaxPoise < 0.0f)
	{
		MaxPoise = 0.0f;
	}
	CurrentPoise = FMath::Clamp(CurrentPoise, 0.0f, MaxPoise);
	// 전투원 등록 — AI 타깃 획득이 이 등록소만 질의하면 World 전체 스캔이 사라진다.
	FCombatTargetRegistry::Get().Register(this);
}

void UCombatStateComponent::EndPlay()
{
	FCombatTargetRegistry::Get().Unregister(this);
	UActorComponent::EndPlay();
}

void UCombatStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bStaggered)
	{
		StaggerRemainingTime -= DeltaTime;
		if (StaggerRemainingTime <= 0.0f)
		{
			StopStagger();
		}
		return;
	}

	// 맷집(체간) 회복: 마지막 피해 이후 PoiseRegenDelay 가 지나야 시작한다 → 연속으로 두들기면 못 채우고
	// 무너지지만(요청 #3), 2~3초만 안 맞으면 빠른 회복으로 가득 찬다(보스는 PoiseRecoveryPerSecond 를 크게).
	const bool bRegenReady = (PoiseRegenDelay <= 0.0f)
		|| (GetOwnerGameTimeSeconds(this) - LastPoiseDamageTimeSeconds) >= PoiseRegenDelay;
	if (PoiseRecoveryPerSecond > 0.0f && CurrentPoise < MaxPoise && bRegenReady)
	{
		float Rate = PoiseRecoveryPerSecond;
		if (bPostureRecoveryScalesWithHealth)
		{
			// 체력 100%→1.0배, 0%→0.4배. 체력이 낮을수록 체간 회복이 느려진다.
			Rate *= 0.4f + 0.6f * FMath::Clamp(GetHealthRatioSafe(), 0.0f, 1.0f);
		}
		SetPoiseValue(CurrentPoise + Rate * DeltaTime);
	}
}

float UCombatStateComponent::GetHealthRatioSafe() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (UHealthComponent* Health = OwnerActor->GetComponentByClass<UHealthComponent>())
		{
			return Health->GetHealthRatio();
		}
	}
	return 1.0f;
}

bool UCombatStateComponent::ApplyPoiseDamage(float PoiseDamage)
{
	return ApplyPoiseDamageInternal(PoiseDamage, false);
}

bool UCombatStateComponent::ApplyParryReflectPoise(float PoiseDamage)
{
	return ApplyPoiseDamageInternal(PoiseDamage, true);
}

FCombatDamageReport UCombatStateComponent::ApplyParryReflectDamage(
	float Damage,
	AActor* DamageCauser,
	AActor* InstigatorActor,
	const FVector& HitLocation,
	float DamageScale)
{
	FCombatDamageReport Report;
	const float ScaledDamage = Damage * DamageScale;
	Report.RequestedDamage = ScaledDamage;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || ScaledDamage <= 0.0f)
	{
		Report.Result = ECombatDamageResult::Rejected;
		return Report;
	}

	UHealthComponent* Health = OwnerActor->GetComponentByClass<UHealthComponent>();
	if (!Health)
	{
		Report.Result = ECombatDamageResult::Rejected;
		return Report;
	}

	FCombatDamageSpec DamageSpec;
	DamageSpec.Damage = ScaledDamage;
	DamageSpec.PoiseDamage = 0.0f;
	DamageSpec.DamageCauser = DamageCauser;
	DamageSpec.InstigatorActor = InstigatorActor;
	DamageSpec.HitLocation = HitLocation;

	if (DamageCauser)
	{
		FVector Direction = OwnerActor->GetActorLocation() - DamageCauser->GetActorLocation();
		Direction.Z = 0.0f;
		DamageSpec.HitDirection = Direction.IsNearlyZero() ? OwnerActor->GetActorForward() : Direction.Normalized();
	}

	return Health->ApplyDamageSpec(DamageSpec);
}

bool UCombatStateComponent::ApplyPoiseDamageInternal(float PoiseDamage, bool bIgnoreParryGrace)
{
	SCOPE_STAT_CAT("CombatState.ApplyPoiseDamage", "Combat");
	// 패링 유예 윈도우 동안(요청 #6)에는 체간 피해를 받지 않는다 → 반격이 자세를 무너뜨려 보스를
	// 불능 상태(stagger)로 빠뜨리지 못한다. SuperArmor 와 동일하게 취급.
	if (PoiseDamage <= 0.0f || bSuperArmor || (!bIgnoreParryGrace && IsInParryGrace()) || MaxPoise <= 0.0f)
	{
		return false;
	}

	// 마지막 체간 피해 시각 기록 → 회복 지연(PoiseRegenDelay) 판정. 연속 압박이면 못 채우고 무너진다(요청 #3).
	LastPoiseDamageTimeSeconds = GetOwnerGameTimeSeconds(this);

	SetPoiseValue(CurrentPoise - PoiseDamage);
	if (CurrentPoise <= 0.0f)
	{
		// 자세 붕괴 신호 — ExecutionComponent 가 데스블로우(처형) 창을 연다.
		OnPostureBroken.Broadcast(this);
		// 무력화(stagger)는 옵션. 보스는 bStaggerOnPostureBreak=false → 붕괴해도 무력화 없이 처형 가능만
		// 열리고, 무력화는 처형 이후 딜타임에만 일어난다(요청). 일반 적은 기본 true 로 기존처럼 붕괴 시 무력화.
		if (bStaggerOnPostureBreak)
		{
			StartStagger(DefaultStaggerDuration);
		}
		return true;
	}
	return false;
}

void UCombatStateComponent::ResetPoise()
{
	SetPoiseValue(MaxPoise);
}

void UCombatStateComponent::StartStagger(float Duration)
{
	if (Duration <= 0.0f)
	{
		ResetPoise();
		return;
	}
	// 처형 자세 붕괴에서 호출되는 공개 stagger — 끝날 때 보이는 자세(체간)를 리셋한다(기존 동작).
	bStaggerResetsPoiseOnEnd = true;
	bStaggered = true;
	StaggerRemainingTime = Duration;
	OnStaggerStarted.Broadcast(this, Duration);
}

void UCombatStateComponent::StartStaggerNoPoiseReset(float Duration)
{
	if (Duration <= 0.0f)
	{
		return;
	}
	// 체간(자세)을 건드리지 않는 무력화 — 끝나도 ResetPoise 하지 않는다(처형 직후 딜타임 등).
	// 시작 시 플린치(OnStaggerStarted)를 발행하지 않는다 → 아직 재생 중일 수 있는 처형 몽타주를 덮지 않는다.
	// 무력화 동안 실제로 맞으면 그때 타격마다 피격 모션이 나온다(EnemyHitComponent 의 per-hit 경로).
	bStaggerResetsPoiseOnEnd = false;
	bStaggered = true;
	StaggerRemainingTime = Duration;
}

void UCombatStateComponent::StopStagger()
{
	if (!bStaggered)
	{
		return;
	}
	bStaggered = false;
	StaggerRemainingTime = 0.0f;
	// 숨김 맷집 게이지 붕괴로 시작된 stagger 는 보이는 자세(처형 진행도)를 건드리지 않는다.
	if (bStaggerResetsPoiseOnEnd)
	{
		ResetPoise();
	}
	OnStaggerEnded.Broadcast(this);
}

void UCombatStateComponent::MarkParried()
{
	// 리코일(공격 몽타주 역재생) 등 리스너에게 항상 알린다(요청 A) — 유예 윈도우 활성 여부와 무관.
	OnParried.Broadcast(this);
	if (ParryGraceDuration <= 0.0f)
	{
		return;
	}
	const float Now = GetOwnerGameTimeSeconds(this);
	ParryGraceUntilSeconds = (std::max)(ParryGraceUntilSeconds, Now + ParryGraceDuration);
}

bool UCombatStateComponent::IsInParryGrace() const
{
	return GetOwnerGameTimeSeconds(this) < ParryGraceUntilSeconds;
}

float UCombatStateComponent::GetPoiseRatio() const
{
	if (MaxPoise <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(CurrentPoise / MaxPoise, 0.0f, 1.0f);
}

float UCombatStateComponent::GetPostureRatio() const
{
	return 1.0f - GetPoiseRatio();
}

bool UCombatStateComponent::SetPoiseValue(float NewPoise)
{
	const float PreviousPoise = CurrentPoise;
	const float ValidMaxPoise = MaxPoise > 0.0f ? MaxPoise : 0.0f;
	CurrentPoise = FMath::Clamp(NewPoise, 0.0f, ValidMaxPoise);

	if (CurrentPoise == PreviousPoise)
	{
		return false;
	}

	OnPostureChanged.Broadcast(this, PreviousPoise, CurrentPoise, ValidMaxPoise);
	return true;
}

void UCombatStateComponent::MarkAttacking(float WindowSeconds)
{
	if (WindowSeconds <= 0.0f)
	{
		return;
	}
	const float Until = GetOwnerGameTimeSeconds(this) + WindowSeconds;
	AttackThreatUntilSeconds = (std::max)(AttackThreatUntilSeconds, Until);
}

bool UCombatStateComponent::IsAttacking() const
{
	return GetAttackThreatRemaining() > 0.0f;
}

float UCombatStateComponent::GetAttackThreatRemaining() const
{
	const float Now = GetOwnerGameTimeSeconds(this);
	return (std::max)(0.0f, AttackThreatUntilSeconds - Now);
}

void UCombatStateComponent::MarkPerilous(EPerilousType InType, float Duration)
{
	if (InType == EPerilousType::None || Duration <= 0.0f)
	{
		return;
	}
	ActivePerilousType   = InType;
	PerilousUntilSeconds = GetOwnerGameTimeSeconds(this) + Duration;
	OnPerilousCue.Broadcast(this, InType);
}

bool UCombatStateComponent::IsPerilousActive() const
{
	return GetOwnerGameTimeSeconds(this) < PerilousUntilSeconds;
}

EPerilousType UCombatStateComponent::GetActivePerilousType() const
{
	return IsPerilousActive() ? ActivePerilousType : EPerilousType::None;
}

void UCombatStateComponent::OpenDeflectWindow(float Seconds)
{
	const float Now = GetOwnerGameTimeSeconds(this);
	const float Len = Seconds > 0.0f ? Seconds : DeflectWindowSeconds;
	DeflectOpenedAtSeconds = Now;
	DeflectUntilSeconds    = Now + Len;
	DeflectWindowLen       = Len;
}

bool UCombatStateComponent::IsDeflecting() const
{
	return GetOwnerGameTimeSeconds(this) < DeflectUntilSeconds;
}

EDeflectGrade UCombatStateComponent::ConsumeDeflect(AActor* Attacker)
{
	SCOPE_STAT_CAT("CombatState.ConsumeDeflect", "Combat");
	if (!IsDeflecting())
	{
		LastDeflectGrade = EDeflectGrade::None;
		return EDeflectGrade::None;
	}

	const float Now       = GetOwnerGameTimeSeconds(this);
	const float Remaining = DeflectUntilSeconds - Now;
	const float Frac      = DeflectWindowLen > 0.0f ? FMath::Clamp(Remaining / DeflectWindowLen, 0.0f, 1.0f) : 0.0f;

	// 윈도우 초반(많이 남음)에 받아넘길수록 정확 — Perfect/Good/Late.
	EDeflectGrade Grade;
	float ReflectScale;
	if (Frac >= 0.5f)
	{
		Grade        = EDeflectGrade::Perfect;
		ReflectScale = 1.0f;
	}
	else if (Frac >= 0.15f)
	{
		Grade        = EDeflectGrade::Good;
		ReflectScale = 0.6f;
	}
	else
	{
		Grade        = EDeflectGrade::Late;
		ReflectScale = 0.0f;
	}

	// 공격자에게 체간 반사 — 탄기가 공격자의 체간을 깎아 인살 창을 연다.
	// 위험공격 종류별 결과표(보고서 1군 #4)로 반사량을 보정한다: 찌르기 간파 성공은 큰 자세 손실,
	// 잡기는 탄기로 해결되지 않으므로 반사 0(점프/이탈로 대응해야 함).
	// MarkParried 는 체간 반사량과 독립적으로 호출한다. 반사량이 0 이거나 데이터상 DeflectReflectPoise 가
	// 비어 있어도, Perfect/Good 탄기 자체가 성공했다면 공격자 리코일/반격 유예는 반드시 열려야 한다.
	if (Attacker && (Grade == EDeflectGrade::Perfect || Grade == EDeflectGrade::Good))
	{
		if (UCombatStateComponent* AttackerCombat = Attacker->GetComponentByClass<UCombatStateComponent>())
		{
			const FPerilousResolution Resolution = GetPerilousResolution(AttackerCombat->GetActivePerilousType());
			const float ReflectDamageScale = ReflectScale * Resolution.AnswerReflectPoiseScale;
			if (ReflectDamageScale > 0.0f && DeflectReflectPoise > 0.0f)
			{
				AttackerCombat->ApplyParryReflectPoise(DeflectReflectPoise * ReflectDamageScale);
			}

			// 패링 성공(Perfect/Good) → 공격자(보스)에게 "반격 유예" 윈도우와 리코일 신호를 연다.
			AttackerCombat->MarkParried();
			if (ReflectDamageScale > 0.0f && DeflectReflectDamage > 0.0f)
			{
				AttackerCombat->ApplyParryReflectDamage(DeflectReflectDamage, GetOwner(), GetOwner(), FVector::ZeroVector, ReflectDamageScale);
			}
		}
	}

	DeflectUntilSeconds = Now; // 윈도우 닫기
	LastDeflectGrade    = Grade;
	OnDeflectResolved.Broadcast(this, Grade, Attacker);
	return Grade;
}

void UCombatStateComponent::OpenGuardWindow(float Seconds)
{
	const float Now = GetOwnerGameTimeSeconds(this);
	const float Len = Seconds > 0.0f ? Seconds : GuardWindowSeconds;
	GuardUntilSeconds = (std::max)(GuardUntilSeconds, Now + Len);
}

void UCombatStateComponent::CloseGuard()
{
	GuardUntilSeconds = GetOwnerGameTimeSeconds(this);
}

bool UCombatStateComponent::IsGuarding() const
{
	return GetOwnerGameTimeSeconds(this) < GuardUntilSeconds;
}

void UCombatStateComponent::NotifyGuardBlocked()
{
	LastGuardBlockSeconds = GetOwnerGameTimeSeconds(this);
}

bool UCombatStateComponent::WasGuardBlockedWithin(float Seconds) const
{
	if (Seconds <= 0.0f)
	{
		return false;
	}
	return (GetOwnerGameTimeSeconds(this) - LastGuardBlockSeconds) <= Seconds;
}

void UCombatStateComponent::OpenInvulnWindow(float Seconds)
{
	if (Seconds <= 0.0f)
	{
		return;
	}
	const float Now = GetOwnerGameTimeSeconds(this);
	InvulnUntilSeconds = (std::max)(InvulnUntilSeconds, Now + Seconds);
}

bool UCombatStateComponent::IsTimedInvuln() const
{
	return GetOwnerGameTimeSeconds(this) < InvulnUntilSeconds;
}

bool UCombatStateComponent::IsHostileTo(const UCombatStateComponent* Other) const
{
	if (!Other)
	{
		return false;
	}
	if (Team == ECombatTeam::Neutral || Other->Team == ECombatTeam::Neutral)
	{
		return false;
	}
	return Team != Other->Team;
}
