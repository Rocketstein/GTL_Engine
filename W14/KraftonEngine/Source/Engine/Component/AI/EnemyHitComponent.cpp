#include "Component/AI/EnemyHitComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Component/AI/EnemyAttackComponent.h"
#include "Component/AI/PhaseComponent.h"
#include "Component/Combat/CombatStateComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/World.h"

#include <cmath>

namespace
{
	float SanitizeMontagePlayRate(float PlayRate)
	{
		return PlayRate > 0.0f ? PlayRate : 1.0f;
	}
}

void UEnemyHitComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	BindHealthEvents();
}

void UEnemyHitComponent::EndPlay()
{
	UnbindHealthEvents();
	UActorComponent::EndPlay();
}

void UEnemyHitComponent::BindHealthEvents()
{
	if (bBoundHealthEvents)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	UHealthComponent* HealthComponent = OwnerActor ? OwnerActor->GetComponentByClass<UHealthComponent>() : nullptr;
	if (!HealthComponent)
	{
		return;
	}

	BoundHealthComponent = HealthComponent;
	DamagedHandle = HealthComponent->OnDamaged.AddWeakUObject(this, &UEnemyHitComponent::HandleDamaged);
	DeathHandle = HealthComponent->OnDeath.AddWeakUObject(this, &UEnemyHitComponent::HandleDeath);

	// 맷집(체간) 붕괴 → stagger(무력화) 시작 시 피격 모션을 재생한다(요청 #3). 평소 타격당 플린치는
	// scene 에서 끌 수 있고(HitReactMaxPhase=0), 피격 모션은 게이지가 다 닳아 무너질 때만 나오게 한다.
	if (UCombatStateComponent* CombatComponent = OwnerActor->GetComponentByClass<UCombatStateComponent>())
	{
		BoundCombatComponent = CombatComponent;
		StaggerStartedHandle = CombatComponent->OnStaggerStarted.AddWeakUObject(this, &UEnemyHitComponent::HandleStaggerStarted);
	}
	bBoundHealthEvents = true;
}

void UEnemyHitComponent::UnbindHealthEvents()
{
	if (!bBoundHealthEvents)
	{
		return;
	}

	if (UHealthComponent* HealthComponent = BoundHealthComponent.Get())
	{
		if (DamagedHandle.IsValid())
		{
			HealthComponent->OnDamaged.Remove(DamagedHandle);
		}
		if (DeathHandle.IsValid())
		{
			HealthComponent->OnDeath.Remove(DeathHandle);
		}
	}

	if (UCombatStateComponent* CombatComponent = BoundCombatComponent.Get())
	{
		if (StaggerStartedHandle.IsValid())
		{
			CombatComponent->OnStaggerStarted.Remove(StaggerStartedHandle);
		}
	}

	DamagedHandle.Reset();
	DeathHandle.Reset();
	StaggerStartedHandle.Reset();
	BoundHealthComponent.Reset();
	BoundCombatComponent.Reset();
	bBoundHealthEvents = false;
}

void UEnemyHitComponent::HandleStaggerStarted(UCombatStateComponent* Combat, float /*Duration*/)
{
	// 처형 연출 중 시작되는 stagger 는 피격 모션을 재생하지 않는다(처형 몽타주 보호).
	if (Combat && Combat->IsBeingExecuted())
	{
		return;
	}
	// 자세 붕괴(무력화) → 피격 모션 1회 재생. 방향 정보가 없으므로 전면(Front) 리액션을 쓴다.
	// 직전에 막 플린치를 재생했으면(HitReactionCooldown 내) 중복을 피한다(일반 적의 타격당 플린치 + 붕괴 중복 방지).
	if (HitReactionCooldown > 0.0f)
	{
		AActor* OwnerActor = GetOwner();
		UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
		const float Now = World ? World->GetGameTimeSeconds() : 0.0f;
		if ((Now - LastHitReactGameTime) < HitReactionCooldown)
		{
			return;
		}
	}
	if (PlayHitMontage(nullptr, nullptr))
	{
		AActor* OwnerActor = GetOwner();
		UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
		LastHitReactGameTime = World ? World->GetGameTimeSeconds() : 0.0f;
	}
}

EEnemyHitDirection UEnemyHitComponent::ResolveHitDirection(AActor* DamageCauser, AActor* InstigatorActor) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return EEnemyHitDirection::Front;
	}

	AActor* SourceActor = DamageCauser ? DamageCauser : InstigatorActor;
	if (!SourceActor)
	{
		return EEnemyHitDirection::Front;
	}

	FVector SourceDirection = SourceActor->GetActorLocation() - OwnerActor->GetActorLocation();
	SourceDirection.Z = 0.0f;
	if (SourceDirection.IsNearlyZero())
	{
		return EEnemyHitDirection::Front;
	}
	SourceDirection = SourceDirection.Normalized();

	FVector Forward = OwnerActor->GetActorForward();
	Forward.Z = 0.0f;
	Forward = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward.Normalized();

	FVector Right = OwnerActor->GetActorRight();
	Right.Z = 0.0f;
	Right = Right.IsNearlyZero() ? FVector::RightVector : Right.Normalized();

	const float ForwardDot = SourceDirection.Dot(Forward);
	const float RightDot = SourceDirection.Dot(Right);
	if (std::abs(ForwardDot) >= std::abs(RightDot))
	{
		return ForwardDot >= 0.0f ? EEnemyHitDirection::Front : EEnemyHitDirection::Back;
	}

	return RightDot >= 0.0f ? EEnemyHitDirection::Right : EEnemyHitDirection::Left;
}

bool UEnemyHitComponent::PlayHitMontage(AActor* DamageCauser, AActor* InstigatorActor)
{
	return PlayDirectionalMontage(
		HitMontages,
		ResolveHitDirection(DamageCauser, InstigatorActor),
		bStopCurrentMontageBeforeHit,
		HitMontagePlayRate
	);
}

bool UEnemyHitComponent::PlayDeathMontage(AActor* DamageCauser, AActor* InstigatorActor)
{
	return PlayDirectionalMontage(
		DeathMontages,
		ResolveHitDirection(DamageCauser, InstigatorActor),
		bStopCurrentMontageBeforeDeath,
		DeathMontagePlayRate
	);
}

void UEnemyHitComponent::HandleDamaged(
	UHealthComponent* /*Component*/,
	float Damage,
	float NewHealth,
	AActor* DamageCauser,
	AActor* InstigatorActor)
{
	if (NewHealth <= 0.0f)
	{
		return;
	}
	if (Damage <= 0.0f && !bPlayHitWhenDamageIsZero)
	{
		return;
	}

	AActor* Owner = GetOwner();
	UCombatStateComponent* Combat = Owner ? Owner->GetComponentByClass<UCombatStateComponent>() : nullptr;

	// 처형(데스블로우) 연출 중에는 피격 리액션을 재생하지 않는다 → 같은 슬롯의 처형 몽타주(ExecutedA)가
	// 플린치에 덮여 끊기던 문제 수정(체력 피해는 HealthComponent 에서 그대로 적용된다).
	if (Combat && Combat->IsBeingExecuted())
	{
		return;
	}

	// 무력화(stagger) 중에는 보스가 무방비 상태다 → HitReactMaxPhase·쿨다운을 무시하고 타격마다 피격
	// 모션을 보여준다(딜 타임/처형 후 "맞고 있다"는 피드백). 평소엔 stagger 일 때만 모션이 나온다.
	const bool bStaggeredNow = Combat && Combat->IsStaggered();

	// 경직 페이즈 제한: 현재 페이즈가 HitReactMaxPhase 를 넘으면 평소엔 피격 리액션을 생략(하이퍼아머).
	// 단 무력화 중에는 우회해 타격마다 반응한다.
	if (!bStaggeredNow)
	{
		int32 CurrentPhase = 1;
		if (UPhaseComponent* Phase = Owner ? Owner->GetComponentByClass<UPhaseComponent>() : nullptr)
		{
			CurrentPhase = Phase->GetCurrentPhase();
		}
		if (CurrentPhase > HitReactMaxPhase)
		{
			return;
		}
	}

	// 패링(반격) 유예 윈도우(요청 #6): 보스가 막 패링당한 직후 — 즉 플레이어의 반격이 들어오는
	// 구간 — 에는 그 피해로 경직되지 않는다. 반격은 체력 피해만 주고 보스의 공세는 끊기지 않는다.
	// 일반 적은 ParryGraceDuration=0 이라 영향이 없다(보스만 scene 에서 활성).
	if (Combat && Combat->IsInParryGrace())
	{
		return;
	}

	// 자기 공격(선/활성/후딜)을 펼치는 중이면 일반 피격에 경직되지 않고 포즈를 유지한다(요청 #5).
	// 과거엔 전역 SuperArmor 플래그가 켜져 있어야만 동작했지만(보스는 꺼져 있어 사실상 무효였다),
	// "공격 모션 중 피격으로 불능화되지 않는다"가 목적이므로 SuperArmor 조건을 제거한다. 자세 붕괴
	// (체간 0)로 인한 stagger 는 별개로 여전히 동작한다.
	if (bPoiseThroughDuringOwnAttack && Combat && Combat->IsAttacking())
	{
		return;
	}

	// 하이퍼아머 쿨다운(요청 #1/#4): 직전 플린치 이후 HitReactionCooldown 이 지나기 전엔 추가 피격에
	// 경직하지 않는다 → 난타로 매 타격마다 플린치가 갱신돼 보스가 영영 공격을 못 시작하는 무한경직을
	// 막고, 경직 사이에 반격 틈을 확보한다. 0 이면 매 타격 경직(기존 동작 — 일반 적).
	const float NowSeconds = (Owner && Owner->GetWorld()) ? Owner->GetWorld()->GetGameTimeSeconds() : 0.0f;
	if (!bStaggeredNow && HitReactionCooldown > 0.0f && (NowSeconds - LastHitReactGameTime) < HitReactionCooldown)
	{
		return;
	}

	if (bClearAttackCooldownsOnHit)
	{
		RestartAttackCooldowns();
	}

	// 가드 중 피격이면 가드 리액션(막기 흔들림)을 재생(이동 중이어도 가드는 제자리이므로 항상).
	if (Combat && Combat->IsGuarding() && GuardHitMontage)
	{
		ACharacter* Character = Cast<ACharacter>(Owner);
		USkeletalMeshComponent* MeshComponent = Character ? Character->GetMesh() : nullptr;
		if (UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr)
		{
			if (bStopCurrentMontageBeforeHit)
			{
				AnimInstance->StopMontage(0.0f);
			}
			AnimInstance->PlayMontage(GuardHitMontage, FName::None, SanitizeMontagePlayRate(HitMontagePlayRate));
			LastHitReactGameTime = NowSeconds;   // 가드 리액션도 하이퍼아머 쿨다운을 시작시킨다
			return;
		}
	}

	// 이동(걷기/달리기) 중이면 경직 몽타주를 생략한다 — 풀바디 피격 몽타주가 걷기를 덮어 슬라이딩처럼
	// 보이던 문제 수정(슈퍼아머 보스는 칩 피해를 걸으며 흘린다). 정지 상태에서만 경직.
	if (bSkipHitWhileMoving && Owner)
	{
		if (UCharacterMovementComponent* Move = Owner->GetComponentByClass<UCharacterMovementComponent>())
		{
			const FVector V = Move->GetVelocityValue();
			if (std::sqrt(V.X * V.X + V.Y * V.Y) > 0.3f)
			{
				return;
			}
		}
	}

	if (PlayHitMontage(DamageCauser, InstigatorActor))
	{
		LastHitReactGameTime = NowSeconds;   // 플린치 재생 → 하이퍼아머 쿨다운 시작
	}
}

void UEnemyHitComponent::HandleDeath(UHealthComponent* /*Component*/, AActor* DamageCauser, AActor* InstigatorActor)
{
	if (bClearAttackCooldownsOnHit)
	{
		RestartAttackCooldowns();
	}
	PlayDeathMontage(DamageCauser, InstigatorActor);
}

void UEnemyHitComponent::RestartAttackCooldowns()
{
	AActor* OwnerActor = GetOwner();
	UEnemyAttackComponent* AttackComponent = OwnerActor ? OwnerActor->GetComponentByClass<UEnemyAttackComponent>() : nullptr;
	if (AttackComponent)
	{
		AttackComponent->RestartAttackCooldowns();
	}
}

UAnimMontage* UEnemyHitComponent::SelectMontage(const FEnemyDirectionalMontages& Montages, EEnemyHitDirection Direction) const
{
	switch (Direction)
	{
	case EEnemyHitDirection::Front:
		return Montages.Front;
	case EEnemyHitDirection::Back:
		return Montages.Back;
	case EEnemyHitDirection::Left:
		return Montages.Left;
	case EEnemyHitDirection::Right:
		return Montages.Right;
	default:
		return Montages.Front;
	}
}

bool UEnemyHitComponent::PlayDirectionalMontage(
	const FEnemyDirectionalMontages& Montages,
	EEnemyHitDirection Direction,
	bool bStopCurrentMontage,
	float PlayRate)
{
	UAnimMontage* Montage = SelectMontage(Montages, Direction);
	if (!Montage)
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* MeshComponent = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}

	if (bStopCurrentMontage)
	{
		AnimInstance->StopMontage(0.0f);
	}
	AnimInstance->PlayMontage(Montage, FName::None, SanitizeMontagePlayRate(PlayRate));
	return true;
}
