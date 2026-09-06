#include "GameFramework/Pawn/BossEnemyCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/Montage/AnimMontageInstance.h"
#include "Component/AI/EncounterComponent.h"
#include "Component/AI/EnemyAIBrainComponent.h"
#include "Component/AI/PhaseComponent.h"
#include "Component/Combat/CombatStateComponent.h"
#include "Component/Combat/ExecutionComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"

void ABossEnemyCharacter::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	InitDefaultComponents(SkeletalMeshFileName, FString());
}

void ABossEnemyCharacter::InitDefaultComponents(const FString& SkeletalMeshFileName, const FString& ScriptFile)
{
	Super::InitDefaultComponents(SkeletalMeshFileName, ScriptFile);
	PhaseComponent = AddComponent<UPhaseComponent>();
	EncounterComponent = AddComponent<UEncounterComponent>();
	RebindBossComponents();
}

void ABossEnemyCharacter::PostDuplicate()
{
	Super::PostDuplicate();
	RebindBossComponents();
}

void ABossEnemyCharacter::OnPostLoad(FArchive& Ar)
{
	Super::OnPostLoad(Ar);
	RebindBossComponents();
}

void ABossEnemyCharacter::RebindBossComponents()
{
	PhaseComponent = GetComponentByClass<UPhaseComponent>();
	EncounterComponent = GetComponentByClass<UEncounterComponent>();
	if (PhaseComponent)
	{
		PhaseComponent->OnPhaseChanged.AddUObject(this, &ABossEnemyCharacter::HandlePhaseChanged);
	}
	if (EncounterComponent)
	{
		EncounterComponent->OnEncounterStarted.AddUObject(this, &ABossEnemyCharacter::HandleEncounterStarted);
		EncounterComponent->OnEncounterCompleted.AddUObject(this, &ABossEnemyCharacter::HandleEncounterCompleted);
	}
}


bool ABossEnemyCharacter::StartBossEncounter()
{
	return EncounterComponent && EncounterComponent->StartEncounter();
}

bool ABossEnemyCharacter::CompleteBossEncounter()
{
	return EncounterComponent && EncounterComponent->CompleteEncounter();
}

bool ABossEnemyCharacter::IsBossEncounterActive() const
{
	return EncounterComponent && EncounterComponent->IsEncounterActive();
}

bool ABossEnemyCharacter::SetBossPhase(int32 NewPhase)
{
	return RequestBossPhase(NewPhase);
}

int32 ABossEnemyCharacter::GetBossPhase() const
{
	return PhaseComponent ? PhaseComponent->GetCurrentPhase() : 1;
}

void ABossEnemyCharacter::BeginExecutionPriorityWindow()
{
	if (!bExecutionPriorityWindowActive)
	{
		PendingExecutionPhase = 0;
	}
	bExecutionPriorityWindowActive = true;
	// 처형 연출 동안 피격 리액션(플린치)을 막는다 → 처형 몽타주(ExecutedA)가 같은 슬롯의 플린치에 끊기지 않게.
	if (UCombatStateComponent* Combat = GetCombatStateComponent())
	{
		Combat->SetBeingExecuted(true);
	}
}

void ABossEnemyCharacter::EndExecutionPriorityWindow()
{
	if (!bExecutionPriorityWindowActive)
	{
		return;
	}

	bExecutionPriorityWindowActive = false;
	// 처형 연출 종료 → 피격 리액션 다시 허용(딜 타임 동안 타격마다 피격 모션이 나오게).
	if (UCombatStateComponent* Combat = GetCombatStateComponent())
	{
		Combat->SetBeingExecuted(false);
	}
	ApplyDeferredBossPhaseChange();

	// 처형(연출)이 끝난 직후 잠깐 "딜 타임"을 연다(요청 #2): 두뇌를 busy 로 잠가 공격/이동을 막되 무적은
	// 걸지 않아 플레이어가 추가로 때릴 수 있다. (처형은 PerformDeathblow/스톡을 쓰지 않는 반복형 연출이라,
	//  실제 종료 지점인 여기서 처리한다.) 페이즈 전환이 함께 일어나면 그쪽 무적/락이 우선한다.
	if (!IsDead() && ExecutionDealTimeSeconds > 0.0f)
	{
		// 처형은 데스블로우(체간 붕괴) 기회를 '소비'한 것으로 처리한다. 창을 닫지 않으면 ExecutionComponent
		// 가 데스블로우 창 만료 시 StopStagger 를 불러 아래 딜타임 무력화를 도중에 끊는다. (이건 체간 처리 —
		// 아래 stagger 와는 별개.) 체간(자세바)도 함께 리셋해 같은 처형을 무한 반복하지 않게 한다.
		if (UExecutionComponent* Exec = GetComponentByClass<UExecutionComponent>())
		{
			Exec->ResetExecution();
		}
		StopEnemyMovement();
		SetRuntimeState(FName("Staggered"), true);
		// 딜 타임 = 처형 직후 잠깐 무력화(stagger). 체간과 무관 — 끝나도 체간을 건드리지 않는다(요청).
		// 두뇌가 멈추고(피격 가능 유지) 타격마다 피격 모션이 나온다(EnemyHitComponent 가 IsStaggered 감지).
		if (UCombatStateComponent* Combat = GetCombatStateComponent())
		{
			Combat->ResetPoise();                                         // 체간(자세) 리셋 — 처형 소비(별개 처리)
			Combat->StartStaggerNoPoiseReset(ExecutionDealTimeSeconds);   // 딜타임 무력화(체간 무관)
		}
		else
		{
			LockBrainBusy(ExecutionDealTimeSeconds);
		}
	}
}

bool ABossEnemyCharacter::RequestBossPhase(int32 NewPhase)
{
	if (!PhaseComponent || NewPhase <= PhaseComponent->GetCurrentPhase())
	{
		return false;
	}

	if (bExecutionPriorityWindowActive)
	{
		if (PendingExecutionPhase < NewPhase)
		{
			PendingExecutionPhase = NewPhase;
		}
		return true;
	}

	return PhaseComponent->SetPhase(NewPhase);
}

void ABossEnemyCharacter::ApplyDeferredBossPhaseChange()
{
	const int32 DeferredPhase = PendingExecutionPhase;
	PendingExecutionPhase = 0;

	if (!PhaseComponent || IsDead() || bExecutionPriorityWindowActive)
	{
		return;
	}

	if (DeferredPhase > PhaseComponent->GetCurrentPhase())
	{
		PhaseComponent->SetPhase(DeferredPhase);
	}
}

void ABossEnemyCharacter::HandleDamaged(UHealthComponent* Component, float Damage, float NewHealth, AActor* DamageCauser, AActor* InstigatorActor)
{
	Super::HandleDamaged(Component, Damage, NewHealth, DamageCauser, InstigatorActor);

	if (!Component || Component->IsDead())
	{
		return;
	}

	// Sekiro-style phase stock: health thresholds advance the boss grammar without
	// destroying/re-spawning the actor, so AI state, camera target, socket weapon, and
	// encounter ownership stay stable across phases.
	const float HealthRatio = Component->GetHealthRatio();
	int32 DesiredPhase = 1;
	if (HealthRatio <= 0.34f)
	{
		DesiredPhase = 3;
	}
	else if (HealthRatio <= 0.67f)
	{
		DesiredPhase = 2;
	}

	if (PhaseComponent && PhaseComponent->GetCurrentPhase() < DesiredPhase)
	{
		RequestBossPhase(DesiredPhase);
	}
}

void ABossEnemyCharacter::HandleDeath(UHealthComponent* Component, AActor* DamageCauser, AActor* InstigatorActor)
{
	bExecutionPriorityWindowActive = false;
	PendingExecutionPhase = 0;
	Super::HandleDeath(Component, DamageCauser, InstigatorActor);
	if (BossDeathMontage)
	{
		if (PlayCombatMontageRated(BossDeathMontage, BossDeathMontagePlayRate, 0.08f))
		{
			if (UAnimInstance* AnimInstance = GetAnimInstance())
			{
				if (UAnimMontageInstance* MontageInstance = AnimInstance->GetMontageInstance())
				{
					if (MontageInstance->GetCurrentMontage() == BossDeathMontage)
					{
						MontageInstance->SetHoldFinalFrame(true);
					}
				}
			}
		}
	}
	CompleteBossEncounter();
}

void ABossEnemyCharacter::HandlePhaseChanged(UPhaseComponent* /*Component*/, int32 /*OldPhase*/, int32 NewPhase)
{
	SetAnimGraphInt(FName("Phase"), NewPhase);
	SetRuntimeState(FName("PhaseChanged"));

	// 페이즈 전환 연출: 자세변경 플러리시 몽타주 + 짧은 무적(연출 중 끊김 방지) + 파티클 버스트.
	if (PhaseChangeMontage)
	{
		PlayCombatMontage(PhaseChangeMontage);
	}
	if (UCombatStateComponent* Combat = GetCombatStateComponent())
	{
		Combat->OpenInvulnWindow(PhaseChangeInvulnSeconds);
	}
	// 연출 동안 두뇌를 busy 로 잠가 다음 행동(이동/공격/가드)이 전환 몽타주를 덮지 못하게 한다.
	LockBrainBusy(PhaseChangeInvulnSeconds);
	//if (UParticleSystemComponent* PhaseVFX = GetComponentByClass<UParticleSystemComponent>())
	//{
	//	//PhaseVFX->Activate(true);
	//}
}

void ABossEnemyCharacter::HandleEncounterStarted(UEncounterComponent* /*Component*/)
{
	SetRuntimeState(FName("Intro"));
}

void ABossEnemyCharacter::HandleEncounterCompleted(UEncounterComponent* /*Component*/)
{
	if (!IsDead())
	{
		SetRuntimeState(FName("EncounterCompleted"));
	}
}
