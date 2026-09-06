#pragma once

#include "Animation/Notify/AnimNotifyState.h"

class UAnimInstance;

#include "Source/Engine/Animation/Notify/AnimNotifyState_CounterInputWindow.generated.h"

UCLASS()
class UAnimNotifyState_CounterInputWindow : public UAnimNotifyState
{
public:
	GENERATED_BODY()
	UAnimNotifyState_CounterInputWindow() = default;
	~UAnimNotifyState_CounterInputWindow() override = default;

	UPROPERTY(Edit, Save, Category="CounterInputWindow", DisplayName="Log State")
	bool bLogState = false;

	void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration) override;
	void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;

	static bool IsCounterInputWindowActive(UAnimInstance* AnimInstance);
};
