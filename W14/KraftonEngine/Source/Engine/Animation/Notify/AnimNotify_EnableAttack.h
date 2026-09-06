#pragma once

#include "Animation/Notify/AnimNotify.h"

class UAnimInstance;

#include "Source/Engine/Animation/Notify/AnimNotify_EnableAttack.generated.h"

UCLASS()
class UAnimNotify_EnableAttack : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_EnableAttack() = default;
	~UAnimNotify_EnableAttack() override = default;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;

	static bool ConsumeEnableAttack(UAnimInstance* AnimInstance);
};
