#include "CharacterLocomotionComponent.h"

#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Engine/GameFramework/AActor.h"
#include "Engine/GameFramework/World.h"
#include "Engine/GameFramework/GameMode/PlayerController.h"
#include "Engine/GameFramework/Camera/PlayerCameraManager.h"
#include "Engine/GameFramework/Camera/WaveOscillatorCameraShake.h"
#include "Engine/Runtime/Engine.h"

#include <algorithm>
#include <cmath>

namespace
{
	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return std::max(MinValue, std::min(MaxValue, Value));
	}

}

void UCharacterLocomotionComponent::BeginPlay()
{
	Super::BeginPlay();
	FootstepTimer = 0.0f;
	FootstepBlendSeed = 0.0f;
	if (bDriverEnabled)
	{
		PlayIfChanged(IdleAnimPath, true);
	}
}

void UCharacterLocomotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDriverEnabled)
	{
		return;
	}

	UCharacterMovementComponent* Movement = ResolveMovement();
	const FVector Velocity = Movement ? Movement->GetVelocityValue() : FVector::ZeroVector;
	const float Speed2D = std::sqrt(Velocity.X * Velocity.X + Velocity.Y * Velocity.Y);
	const bool bFalling = Movement ? Movement->IsFalling() : false;
	const bool bRunning = Speed2D >= RunSpeedThreshold;

	if (bFalling && !JumpAnimPath.empty() && JumpAnimPath != "None")
	{
		PlayIfChanged(JumpAnimPath, true);
	}
	else if (bRunning && !RunAnimPath.empty() && RunAnimPath != "None")
	{
		PlayIfChanged(RunAnimPath, true);
	}
	else if (Speed2D >= WalkSpeedThreshold && !WalkAnimPath.empty() && WalkAnimPath != "None")
	{
		PlayIfChanged(WalkAnimPath, true);
	}
	else
	{
		PlayIfChanged(IdleAnimPath, true);
	}

	TickFootstepShake(DeltaTime, Speed2D, bRunning, bFalling);
}

void UCharacterLocomotionComponent::SetDriverEnabled(bool bEnabled)
{
	bDriverEnabled = bEnabled;
	FootstepTimer = 0.0f;
	if (bEnabled)
	{
		CurrentAnimPath.clear();
		PlayIfChanged(IdleAnimPath, true);
	}
}

USkeletalMeshComponent* UCharacterLocomotionComponent::ResolveMesh() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->GetComponentByClass<USkeletalMeshComponent>() : nullptr;
}

UCharacterMovementComponent* UCharacterLocomotionComponent::ResolveMovement() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->GetComponentByClass<UCharacterMovementComponent>() : nullptr;
}

void UCharacterLocomotionComponent::PlayIfChanged(const FString& AnimPath, bool bLooping)
{
	if (AnimPath.empty() || AnimPath == "None" || AnimPath == CurrentAnimPath)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = ResolveMesh();
	if (!Mesh)
	{
		return;
	}

	if (Mesh->PlayAnimationByPath(AnimPath, bLooping))
	{
		CurrentAnimPath = AnimPath;
	}
}

void UCharacterLocomotionComponent::TickFootstepShake(float DeltaTime, float Speed2D, bool bRunning, bool bFalling)
{
	if (!bEnableFootstepShake || bFalling || Speed2D < WalkSpeedThreshold || FootstepShakeScale <= 0.0f)
	{
		FootstepTimer = 0.0f;
		return;
	}

	const int32 SafeFootfalls = std::max(1, FootfallsPerCycle);
	const float BasePeriod = bRunning ? 0.34f : 0.48f;
	const float SpeedRatio = ClampFloat(Speed2D / std::max(RunSpeedThreshold, 0.1f), 0.35f, 1.75f);
	const float Period = ClampFloat(BasePeriod / SpeedRatio / static_cast<float>(SafeFootfalls), 0.12f, 0.65f);

	FootstepTimer += DeltaTime;
	if (FootstepTimer < Period)
	{
		return;
	}

	FootstepTimer = 0.0f;
	FootstepBlendSeed += 0.37f;
	const float Variation = 0.85f + 0.3f * std::fabs(std::sin(FootstepBlendSeed));
	const float Scale = FootstepShakeScale * (bRunning ? RunShakeMultiplier : 1.0f) * Variation;

	if (!GEngine || !GEngine->GetWorld())
	{
		return;
	}

	APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
	APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
	if (Manager)
	{
		Manager->StartCameraShake<UWaveOscillatorCameraShake>(Scale);
	}
}
