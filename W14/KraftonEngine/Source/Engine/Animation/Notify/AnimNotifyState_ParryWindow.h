#pragma once

#include "Animation/Notify/AnimNotifyState.h"
#include "Math/Vector.h"

class UAnimInstance;
class AActor;

#include "Source/Engine/Animation/Notify/AnimNotifyState_ParryWindow.generated.h"

UCLASS()
class UAnimNotifyState_ParryWindow : public UAnimNotifyState
{
public:
	GENERATED_BODY()
	UAnimNotifyState_ParryWindow() = default;
	~UAnimNotifyState_ParryWindow() override = default;

	UPROPERTY(Edit, Save, Category="ParryWindow", DisplayName="Log State")
	bool bLogState = false;

	void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration) override;
	void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;

	static bool IsParryWindowActive(UAnimInstance* AnimInstance);
	static bool ReportSuccessfulParry(UAnimInstance* AnimInstance);
	static bool ReportSuccessfulParry(UAnimInstance* AnimInstance, AActor* Attacker);
	static bool ReportSuccessfulParry(UAnimInstance* AnimInstance, AActor* Attacker, const FVector& HitLocation);
	static bool ReportCounterOpportunity(UAnimInstance* AnimInstance, AActor* Attacker, const FVector& HitLocation);
	static bool ConsumeSuccessfulParry(UAnimInstance* AnimInstance);
	static AActor* ConsumeSuccessfulParryAttacker(UAnimInstance* AnimInstance);
	static bool ConsumeSuccessfulParryData(
		UAnimInstance* AnimInstance,
		AActor*& OutAttacker,
		FVector& OutHitLocation,
		bool& bOutHasHitLocation);
};
