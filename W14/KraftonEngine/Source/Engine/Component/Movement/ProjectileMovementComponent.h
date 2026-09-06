#pragma once

#include "Component/Movement/MovementComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

#include "Source/Engine/Component/Movement/ProjectileMovementComponent.generated.h"
enum class EProjectileHitBehavior : int32
{
	Stop = 0,
	Bounce = 1,
	Destroy = 2,
};

UCLASS()
class UProjectileMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()
	UProjectileMovementComponent() = default;
	~UProjectileMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void ContributeSelectedVisuals(FScene& Scene) const override;

	UFUNCTION(Callable, Category="Movement|Projectile")
	void SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }
	UFUNCTION(Pure, Category="Movement|Projectile")
	FVector GetVelocity() const { return Velocity; }
	UFUNCTION(Callable, Category="Movement|Projectile")
	void SetInitialSpeed(float InInitialSpeed) { InitialSpeed = InInitialSpeed; }
	UFUNCTION(Pure, Category="Movement|Projectile")
	float GetInitialSpeed() const { return InitialSpeed; }
	UFUNCTION(Pure, Category="Movement|Projectile")
	float GetMaxSpeed() const { return MaxSpeed; }
	UFUNCTION(Pure, Category="Movement|Projectile")
	FVector GetPreviewVelocity() const;
	UFUNCTION(Callable, Category="Movement|Projectile")
	void StopSimulating();

    UFUNCTION(Callable, Category="Movement|Collision")
    void SetSweepCollisionEnabled(bool bInEnableSweep) { bSweepCollision = bInEnableSweep; }
    UFUNCTION(Pure, Category="Movement|Collision")
    bool IsSweepCollisionEnabled() const { return bSweepCollision; }

protected:
	FVector ComputeEffectiveVelocity() const;
	virtual EProjectileHitBehavior GetHitBehavior() const;
	virtual bool HandleBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, const FHitResult& HitResult);

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Velocity", Type=Vec3, Min=0.0f, Max=0.0f, Speed=1.0f)
	FVector Velocity = FVector(0.0f, 0.0f, 0.0f);
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Initial Speed", Min=0.0f, Max=0.0f, Speed=10.0f)
	float InitialSpeed = 10.0f;
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Max Speed", Min=0.0f, Max=0.0f, Speed=10.0f)
	float MaxSpeed = 100.0f;

    // true면 UpdatedComponent의 shape를 Start→End로 sweep한 뒤 이동한다.
    // CCD가 잡지 못하는 SetWorldLocation 기반 projectile 관통 방지용이다.
    UPROPERTY(Edit, Save, Category="Movement|Collision", DisplayName="Sweep Collision")
    bool bSweepCollision = true;

    // hit 직전에서 살짝 당겨 배치해 다음 프레임 start penetration을 줄인다.
    UPROPERTY(Edit, Save, Category="Movement|Collision", DisplayName="Sweep Pullback Distance", Min=0.0f, Max=10.0f, Speed=0.01f)
    float SweepPullbackDistance = 0.01f;
};
