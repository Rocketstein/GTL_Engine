#pragma once

#include "Component/ActorComponent.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Math/Vector.h"

#include "Source/Engine/Component/Animation/CharacterLocomotionComponent.generated.h"

class USkeletalMeshComponent;
class UCharacterMovementComponent;

UCLASS()
class UCharacterLocomotionComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UCharacterLocomotionComponent() = default;
	~UCharacterLocomotionComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	UFUNCTION(Callable, Category="Locomotion")
	void SetDriverEnabled(bool bEnabled);
	UFUNCTION(Pure, Category="Locomotion")
	bool IsDriverEnabled() const { return bDriverEnabled; }

	UPROPERTY(Edit, Save, Category="Locomotion|Anim", DisplayName="Idle Anim", AssetType="UAnimSequence")
	FString IdleAnimPath;
	UPROPERTY(Edit, Save, Category="Locomotion|Anim", DisplayName="Walk Anim", AssetType="UAnimSequence")
	FString WalkAnimPath;
	UPROPERTY(Edit, Save, Category="Locomotion|Anim", DisplayName="Run Anim", AssetType="UAnimSequence")
	FString RunAnimPath;
	UPROPERTY(Edit, Save, Category="Locomotion|Anim", DisplayName="Jump Anim", AssetType="UAnimSequence")
	FString JumpAnimPath;
	UPROPERTY(Edit, Save, Category="Locomotion|Anim", DisplayName="Attack Anim", AssetType="UAnimSequence")
	FString AttackAnimPath;

	UPROPERTY(Edit, Save, Category="Locomotion", DisplayName="Walk Speed Threshold", Min=0.0f, Max=100.0f, Speed=0.05f)
	float WalkSpeedThreshold = 0.15f;
	UPROPERTY(Edit, Save, Category="Locomotion", DisplayName="Run Speed Threshold", Min=0.0f, Max=100.0f, Speed=0.1f)
	float RunSpeedThreshold = 4.0f;

	UPROPERTY(Edit, Save, Category="Locomotion|Footstep", DisplayName="Enable Footstep Shake")
	bool bEnableFootstepShake = false;
	UPROPERTY(Edit, Save, Category="Locomotion|Footstep", DisplayName="Footstep Shake Scale", Min=0.0f, Max=20.0f, Speed=0.05f)
	float FootstepShakeScale = 1.0f;
	UPROPERTY(Edit, Save, Category="Locomotion|Footstep", DisplayName="Footfalls Per Cycle", Min=1, Max=8, Speed=1)
	int32 FootfallsPerCycle = 2;
	UPROPERTY(Edit, Save, Category="Locomotion|Footstep", DisplayName="Run Shake Multiplier", Min=0.0f, Max=10.0f, Speed=0.05f)
	float RunShakeMultiplier = 1.5f;
	UPROPERTY(Edit, Save, Category="Locomotion", DisplayName="Driver Enabled On Begin")
	bool bDriverEnabled = true;

private:
	USkeletalMeshComponent* ResolveMesh() const;
	UCharacterMovementComponent* ResolveMovement() const;
	void PlayIfChanged(const FString& AnimPath, bool bLooping);
	void TickFootstepShake(float DeltaTime, float Speed2D, bool bRunning, bool bFalling);

	FString CurrentAnimPath;
	float FootstepTimer = 0.0f;
	float FootstepBlendSeed = 0.0f;
};
