#pragma once

#include "Math/Vector.h"
#include "MovementComponent.h"
class UProjectileMovementComponent2 : public UMovementComponent
{
  public:
    DECLARE_CLASS(UProjectileMovementComponent2, UMovementComponent)

    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

    virtual void BeginPlay() override;

    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    void SetInitialVelocity(FVector InVelocity)
    {
        InitialVelocity = InVelocity;
        Velocity = InVelocity;
    }
    void SetInitialAcceleration(FVector InAcceleration)
    {
        InitialAcceleration = InAcceleration;
        Acceleration = InAcceleration;
    }
    void SetGravityEnabled(bool InEnable) { bIsGravityEnabled = InEnable; }
    void SetGravitationalAcceleration(float InValue) { GravitationalAcceleration = InValue; }

  private:
    FVector CalculateVelocity(float DeltaTime);

    FVector Velocity = FVector(0.f, 0.f, 0.f);
    FVector InitialVelocity = FVector(0.f, 0.f, 0.f);

    FVector Acceleration = FVector(0.f, 0.f, 0.f);
    FVector InitialAcceleration = FVector(0.f, 0.f, 0.f);

    float GravitationalAcceleration = -9.80665f;
    bool  bIsGravityEnabled = false;
};
