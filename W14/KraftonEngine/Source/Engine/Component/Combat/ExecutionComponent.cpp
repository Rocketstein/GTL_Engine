#include "Component/Combat/ExecutionComponent.h"

#include "Component/Combat/CombatStateComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"

void UExecutionComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (MaxStocks < 1)
	{
		MaxStocks = 1;
	}
	CurrentStocks = MaxStocks;
	ExecutionState = EExecutionState::None;
	ExecutionWindowRemaining = 0.0f;

	// 형제 CombatState 의 자세 붕괴 신호를 받아 처형 창을 연다.
	if (UCombatStateComponent* Combat = GetCombatState())
	{
		PostureBrokenHandle = Combat->OnPostureBroken.AddWeakUObject(this, &UExecutionComponent::HandlePostureBroken);
	}
}

void UExecutionComponent::EndPlay()
{
	if (PostureBrokenHandle.IsValid())
	{
		if (UCombatStateComponent* Combat = GetCombatState())
		{
			Combat->OnPostureBroken.Remove(PostureBrokenHandle);
		}
		PostureBrokenHandle.Reset();
	}
	UActorComponent::EndPlay();
}

void UExecutionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ExecutionState != EExecutionState::DeathblowReady)
	{
		return;
	}

	ExecutionWindowRemaining -= DeltaTime;
	if (ExecutionWindowRemaining <= 0.0f)
	{
		// 처형되지 못하고 창이 만료 — 자세를 회복해 전투로 복귀한다.
		ExecutionState = EExecutionState::None;
		ExecutionWindowRemaining = 0.0f;
		if (UCombatStateComponent* Combat = GetCombatState())
		{
			Combat->StopStagger();
			Combat->ResetPoise();
		}
		OnExecutionWindowExpired.Broadcast(this);
	}
}

void UExecutionComponent::HandlePostureBroken(UCombatStateComponent* /*Combat*/)
{
	NotifyPostureBroken();
}

void UExecutionComponent::NotifyPostureBroken()
{
	if (ExecutionState == EExecutionState::DeathblowReady || CurrentStocks <= 0)
	{
		return;
	}
	if (UHealthComponent* Health = GetHealth())
	{
		if (Health->IsDead())
		{
			return;
		}
	}

	ExecutionState = EExecutionState::DeathblowReady;
	ExecutionWindowRemaining = ExecutionWindowSeconds;
	OnDeathblowReady.Broadcast(this);
}

bool UExecutionComponent::PerformDeathblow(AActor* Executor)
{
	if (ExecutionState != EExecutionState::DeathblowReady)
	{
		return false;
	}
	SpendStock(Executor);
	return true;
}

bool UExecutionComponent::PerformStealthDeathblow(AActor* Executor)
{
	if (CurrentStocks <= 0)
	{
		return false;
	}
	if (UHealthComponent* Health = GetHealth())
	{
		if (Health->IsDead())
		{
			return false;
		}
	}
	SpendStock(Executor);
	return true;
}

void UExecutionComponent::SpendStock(AActor* Executor)
{
	CurrentStocks = CurrentStocks > 0 ? CurrentStocks - 1 : 0;
	ExecutionState = EExecutionState::None;
	ExecutionWindowRemaining = 0.0f;

	if (CurrentStocks <= 0)
	{
		// 마지막 스톡 — 처형으로 사망 확정.
		ExecutionState = EExecutionState::Executed;
		if (UHealthComponent* Health = GetHealth())
		{
			Health->Kill(Executor, Executor);
		}
		OnExecuted.Broadcast(this);
		return;
	}

	// 다음 스톡 단계 — 자세/체력 재초기화 후 전투 재개(보스 페이즈 전환의 트리거).
	if (UCombatStateComponent* Combat = GetCombatState())
	{
		Combat->StopStagger();
		if (bResetPostureOnStockSpent)
		{
			Combat->ResetPoise();
		}
	}
	if (HealthFractionRestoredOnStockSpent > 0.0f)
	{
		if (UHealthComponent* Health = GetHealth())
		{
			Health->Heal(Health->GetMaxHealth() * HealthFractionRestoredOnStockSpent);
		}
	}
	OnStockSpent.Broadcast(this, CurrentStocks);
}

void UExecutionComponent::ResetExecution()
{
	if (MaxStocks < 1)
	{
		MaxStocks = 1;
	}
	CurrentStocks = MaxStocks;
	ExecutionState = EExecutionState::None;
	ExecutionWindowRemaining = 0.0f;
}

UCombatStateComponent* UExecutionComponent::GetCombatState() const
{
	AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetComponentByClass<UCombatStateComponent>() : nullptr;
}

UHealthComponent* UExecutionComponent::GetHealth() const
{
	AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetComponentByClass<UHealthComponent>() : nullptr;
}
