#include "Component/AI/EnemyAttackComponent.h"

#include <algorithm>
#include <cmath>

void UEnemyAttackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	GlobalCooldownRemaining = (std::max)(0.0f, GlobalCooldownRemaining - DeltaTime);
	for (auto It = Cooldowns.begin(); It != Cooldowns.end(); )
	{
		It->RemainingTime = (std::max)(0.0f, It->RemainingTime - DeltaTime);
		if (It->RemainingTime <= 0.0f)
		{
			It = Cooldowns.erase(It);
			continue;
		}
		++It;
	}
}

int32 UEnemyAttackComponent::FindCooldownIndex(const FName& AttackName) const
{
	for (int32 Index = 0; Index < static_cast<int32>(Cooldowns.size()); ++Index)
	{
		if (Cooldowns[Index].AttackName == AttackName)
		{
			return Index;
		}
	}
	return -1;
}

bool UEnemyAttackComponent::IsAttackOnCooldown(const FName& AttackName) const
{
	return GetAttackCooldownRemaining(AttackName) > 0.0f;
}

float UEnemyAttackComponent::GetAttackCooldownRemaining(const FName& AttackName) const
{
	const int32 Index = FindCooldownIndex(AttackName);
	return Index >= 0 ? Cooldowns[Index].RemainingTime : 0.0f;
}

void UEnemyAttackComponent::SetAttackCooldown(const FName& AttackName, float Cooldown)
{
	if (!AttackName.IsValid() || Cooldown <= 0.0f)
	{
		return;
	}
	const int32 Index = FindCooldownIndex(AttackName);
	if (Index >= 0)
	{
		Cooldowns[Index].RemainingTime = (std::max)(Cooldowns[Index].RemainingTime, Cooldown);
		return;
	}
	FEnemyAttackCooldownEntry Entry;
	Entry.AttackName = AttackName;
	Entry.RemainingTime = Cooldown;
	Cooldowns.push_back(Entry);
}

void UEnemyAttackComponent::ClearAttackCooldowns()
{
	Cooldowns.clear();
	GlobalCooldownRemaining = 0.0f;
}

void UEnemyAttackComponent::RestartAttackCooldowns()
{
	Cooldowns.clear();
	for (const FEnemyAttackData& Attack : Attacks)
	{
		SetAttackCooldown(Attack.AttackName, Attack.Cooldown);
	}
	GlobalCooldownRemaining = (std::max)(0.0f, GlobalCooldown);
}

bool UEnemyAttackComponent::CanUseAttack(const FEnemyAttackData& Attack, int32 Phase, float Distance, float AbsAngle) const
{
	if (!Attack.AttackName.IsValid() || !Attack.Montage)
	{
		return false;
	}
	if (Phase < Attack.MinPhase || Phase > Attack.MaxPhase)
	{
		return false;
	}
	if (Distance < Attack.MinRange || Distance > Attack.MaxRange)
	{
		return false;
	}
	if (Attack.bRequiresTargetInFront && fabsf(AbsAngle) > Attack.MaxAbsAngle)
	{
		return false;
	}
	if (GlobalCooldownRemaining > 0.0f || IsAttackOnCooldown(Attack.AttackName))
	{
		return false;
	}
	return Attack.Weight > 0.0f;
}

bool UEnemyAttackComponent::CommitAttack(const FName& AttackName)
{
	const FEnemyAttackData Attack = FindAttackByName(AttackName);
	if (!Attack.AttackName.IsValid())
	{
		return false;
	}
	return CommitAttackData(Attack);
}

bool UEnemyAttackComponent::CommitAttackData(const FEnemyAttackData& Attack)
{
	if (!Attack.AttackName.IsValid())
	{
		return false;
	}
	SetAttackCooldown(Attack.AttackName, Attack.Cooldown);
	GlobalCooldownRemaining = (std::max)(GlobalCooldownRemaining, GlobalCooldown);
	LastAttackName = Attack.AttackName;
	RecentAttacks.insert(RecentAttacks.begin(), Attack.AttackName);
	while (static_cast<int32>(RecentAttacks.size()) > RecentAttackMemory)
	{
		RecentAttacks.pop_back();
	}
	OnAttackCommitted.Broadcast(this, Attack.AttackName);
	return true;
}

FEnemyAttackData UEnemyAttackComponent::FindAttackByName(const FName& AttackName) const
{
	for (const FEnemyAttackData& Attack : Attacks)
	{
		if (Attack.AttackName == AttackName)
		{
			return Attack;
		}
	}
	return FEnemyAttackData();
}
