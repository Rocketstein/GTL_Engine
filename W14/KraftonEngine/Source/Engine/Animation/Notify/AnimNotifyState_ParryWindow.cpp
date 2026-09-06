#include "AnimNotifyState_ParryWindow.h"

#include "Animation/AnimInstance.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Profiling/Stats/Stats.h"

namespace
{
	TSet<TWeakObjectPtr<USkeletalMeshComponent>> GActiveParryMeshes;
	TSet<TWeakObjectPtr<USkeletalMeshComponent>> GSuccessfulParryMeshes;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<AActor>> GSuccessfulParryAttackers;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FVector> GSuccessfulParryHitLocations;

	void PurgeInvalidParryMeshes()
	{
		for (auto It = GActiveParryMeshes.begin(); It != GActiveParryMeshes.end(); )
		{
			if (!IsValid(*It)) It = GActiveParryMeshes.erase(It);
			else ++It;
		}

		for (auto It = GSuccessfulParryMeshes.begin(); It != GSuccessfulParryMeshes.end(); )
		{
			if (!IsValid(*It)) It = GSuccessfulParryMeshes.erase(It);
			else ++It;
		}

		for (auto It = GSuccessfulParryAttackers.begin(); It != GSuccessfulParryAttackers.end(); )
		{
			if (!IsValid(It->first)) It = GSuccessfulParryAttackers.erase(It);
			else ++It;
		}

		for (auto It = GSuccessfulParryHitLocations.begin(); It != GSuccessfulParryHitLocations.end(); )
		{
			if (!IsValid(It->first)) It = GSuccessfulParryHitLocations.erase(It);
			else ++It;
		}
	}

	USkeletalMeshComponent* GetMeshFromAnimInstance(UAnimInstance* AnimInstance)
	{
		if (!IsValid(AnimInstance))
		{
			return nullptr;
		}

		USkeletalMeshComponent* MeshComp = AnimInstance->GetOwningComponent();
		return IsValid(MeshComp) ? MeshComp : nullptr;
	}
}

void UAnimNotifyState_ParryWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/, float /*TotalDuration*/)
{
	SCOPE_STAT_CAT("ParryWindow.NotifyBegin", "Combat");
	PurgeInvalidParryMeshes();
	if (!IsValid(MeshComp))
	{
		return;
	}

	GActiveParryMeshes.insert(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));

	if (bLogState)
	{
		UE_LOG("[AnimNotifyState_ParryWindow] BEGIN Mesh=%s", MeshComp->GetName().c_str());
	}
}

void UAnimNotifyState_ParryWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/)
{
	SCOPE_STAT_CAT("ParryWindow.NotifyEnd", "Combat");
	PurgeInvalidParryMeshes();
	if (!IsValid(MeshComp))
	{
		return;
	}

	GActiveParryMeshes.erase(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));
	GSuccessfulParryAttackers.erase(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));
	GSuccessfulParryHitLocations.erase(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));

	if (bLogState)
	{
		UE_LOG("[AnimNotifyState_ParryWindow] END Mesh=%s", MeshComp->GetName().c_str());
	}
}

bool UAnimNotifyState_ParryWindow::IsParryWindowActive(UAnimInstance* AnimInstance)
{
	PurgeInvalidParryMeshes();

	USkeletalMeshComponent* MeshComp = GetMeshFromAnimInstance(AnimInstance);
	if (!MeshComp)
	{
		return false;
	}

	return GActiveParryMeshes.find(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)) != GActiveParryMeshes.end();
}

bool UAnimNotifyState_ParryWindow::ReportSuccessfulParry(UAnimInstance* AnimInstance)
{
	return ReportSuccessfulParry(AnimInstance, nullptr);
}

bool UAnimNotifyState_ParryWindow::ReportSuccessfulParry(UAnimInstance* AnimInstance, AActor* Attacker)
{
	SCOPE_STAT_CAT("ParryWindow.ReportSuccessful", "Combat");
	PurgeInvalidParryMeshes();

	USkeletalMeshComponent* MeshComp = GetMeshFromAnimInstance(AnimInstance);
	if (!MeshComp)
	{
		return false;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	if (GActiveParryMeshes.find(Key) == GActiveParryMeshes.end())
	{
		return false;
	}

	GSuccessfulParryMeshes.insert(Key);
	if (IsValid(Attacker))
	{
		GSuccessfulParryAttackers[Key] = Attacker;
	}
	else
	{
		GSuccessfulParryAttackers.erase(Key);
	}
	GSuccessfulParryHitLocations.erase(Key);
	return true;
}

bool UAnimNotifyState_ParryWindow::ReportSuccessfulParry(
	UAnimInstance* AnimInstance,
	AActor* Attacker,
	const FVector& HitLocation)
{
	if (!ReportSuccessfulParry(AnimInstance, Attacker))
	{
		return false;
	}

	if (USkeletalMeshComponent* MeshComp = GetMeshFromAnimInstance(AnimInstance))
	{
		GSuccessfulParryHitLocations[TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)] = HitLocation;
		return true;
	}

	return false;
}

bool UAnimNotifyState_ParryWindow::ReportCounterOpportunity(
	UAnimInstance* AnimInstance,
	AActor* Attacker,
	const FVector& HitLocation)
{
	SCOPE_STAT_CAT("ParryWindow.ReportCounterOpportunity", "Combat");
	PurgeInvalidParryMeshes();

	USkeletalMeshComponent* MeshComp = GetMeshFromAnimInstance(AnimInstance);
	if (!MeshComp)
	{
		return false;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	GSuccessfulParryMeshes.insert(Key);
	if (IsValid(Attacker))
	{
		GSuccessfulParryAttackers[Key] = Attacker;
	}
	else
	{
		GSuccessfulParryAttackers.erase(Key);
	}
	GSuccessfulParryHitLocations[Key] = HitLocation;
	return true;
}

bool UAnimNotifyState_ParryWindow::ConsumeSuccessfulParry(UAnimInstance* AnimInstance)
{
	AActor* Attacker = nullptr;
	FVector HitLocation = FVector::ZeroVector;
	bool bHasHitLocation = false;
	return ConsumeSuccessfulParryData(AnimInstance, Attacker, HitLocation, bHasHitLocation);
}

AActor* UAnimNotifyState_ParryWindow::ConsumeSuccessfulParryAttacker(UAnimInstance* AnimInstance)
{
	AActor* Attacker = nullptr;
	FVector HitLocation = FVector::ZeroVector;
	bool bHasHitLocation = false;
	ConsumeSuccessfulParryData(AnimInstance, Attacker, HitLocation, bHasHitLocation);
	return Attacker;
}

bool UAnimNotifyState_ParryWindow::ConsumeSuccessfulParryData(
	UAnimInstance* AnimInstance,
	AActor*& OutAttacker,
	FVector& OutHitLocation,
	bool& bOutHasHitLocation)
{
	SCOPE_STAT_CAT("ParryWindow.ConsumeSuccessfulData", "Combat");
	PurgeInvalidParryMeshes();
	OutAttacker = nullptr;
	OutHitLocation = FVector::ZeroVector;
	bOutHasHitLocation = false;

	USkeletalMeshComponent* MeshComp = GetMeshFromAnimInstance(AnimInstance);
	if (!MeshComp)
	{
		return false;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	auto SuccessIt = GSuccessfulParryMeshes.find(Key);
	if (SuccessIt == GSuccessfulParryMeshes.end())
	{
		return false;
	}

	auto AttackerIt = GSuccessfulParryAttackers.find(Key);
	if (AttackerIt != GSuccessfulParryAttackers.end())
	{
		OutAttacker = AttackerIt->second.Get();
		GSuccessfulParryAttackers.erase(AttackerIt);
	}

	auto LocationIt = GSuccessfulParryHitLocations.find(Key);
	if (LocationIt != GSuccessfulParryHitLocations.end())
	{
		OutHitLocation = LocationIt->second;
		bOutHasHitLocation = true;
		GSuccessfulParryHitLocations.erase(LocationIt);
	}

	GSuccessfulParryMeshes.erase(SuccessIt);
	return true;
}
