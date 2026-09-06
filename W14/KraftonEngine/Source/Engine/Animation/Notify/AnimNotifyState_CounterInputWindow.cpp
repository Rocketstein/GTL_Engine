#include "AnimNotifyState_CounterInputWindow.h"

#include "Animation/AnimInstance.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Profiling/Stats/Stats.h"

namespace
{
	TSet<TWeakObjectPtr<USkeletalMeshComponent>> GActiveCounterInputMeshes;

	void PurgeInvalidCounterInputMeshes()
	{
		for (auto It = GActiveCounterInputMeshes.begin(); It != GActiveCounterInputMeshes.end(); )
		{
			if (!IsValid(*It)) It = GActiveCounterInputMeshes.erase(It);
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

void UAnimNotifyState_CounterInputWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* /*Anim*/,
	float /*TotalDuration*/)
{
	SCOPE_STAT_CAT("CounterInputWindow.NotifyBegin", "Combat");
	PurgeInvalidCounterInputMeshes();
	if (!IsValid(MeshComp))
	{
		return;
	}

	GActiveCounterInputMeshes.insert(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));

	if (bLogState)
	{
		UE_LOG("[AnimNotifyState_CounterInputWindow] BEGIN Mesh=%s", MeshComp->GetName().c_str());
	}
}

void UAnimNotifyState_CounterInputWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/)
{
	SCOPE_STAT_CAT("CounterInputWindow.NotifyEnd", "Combat");
	PurgeInvalidCounterInputMeshes();
	if (!IsValid(MeshComp))
	{
		return;
	}

	GActiveCounterInputMeshes.erase(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));

	if (bLogState)
	{
		UE_LOG("[AnimNotifyState_CounterInputWindow] END Mesh=%s", MeshComp->GetName().c_str());
	}
}

bool UAnimNotifyState_CounterInputWindow::IsCounterInputWindowActive(UAnimInstance* AnimInstance)
{
	SCOPE_STAT_CAT("CounterInputWindow.IsActive", "Combat");
	PurgeInvalidCounterInputMeshes();

	USkeletalMeshComponent* MeshComp = GetMeshFromAnimInstance(AnimInstance);
	if (!MeshComp)
	{
		return false;
	}

	return GActiveCounterInputMeshes.find(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)) != GActiveCounterInputMeshes.end();
}
