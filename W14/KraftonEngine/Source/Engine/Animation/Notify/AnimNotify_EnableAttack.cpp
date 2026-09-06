#include "AnimNotify_EnableAttack.h"

#include "Animation/AnimInstance.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/Pawn/Character.h"
#include "Object/Ptr/WeakObjectPtr.h"

namespace
{
	TSet<TWeakObjectPtr<USkeletalMeshComponent>> GEnableAttackMeshes;
}

void UAnimNotify_EnableAttack::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/)
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	GEnableAttackMeshes.insert(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));

	ACharacter* OwnerCharacter = Cast<ACharacter>(MeshComp->GetOwner());
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		Movement->SetMovementInputEnabled(true);
	}
}

bool UAnimNotify_EnableAttack::ConsumeEnableAttack(UAnimInstance* AnimInstance)
{
	if (!IsValid(AnimInstance))
	{
		return false;
	}

	USkeletalMeshComponent* MeshComp = AnimInstance->GetOwningComponent();
	if (!IsValid(MeshComp))
	{
		return false;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	auto It = GEnableAttackMeshes.find(Key);
	if (It == GEnableAttackMeshes.end())
	{
		return false;
	}

	GEnableAttackMeshes.erase(It);
	return true;
}
