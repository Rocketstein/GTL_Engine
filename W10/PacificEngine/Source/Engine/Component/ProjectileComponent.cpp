#include "ProjectileComponent.h"
#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(UProjectileMovementComponent2, UMovementComponent)

void UProjectileMovementComponent2::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    if (UWorld* World = GetWorld())
    {
        if (World->IsGameplayPaused())
        {
            return;
        }
    }

    if (!UpdatedComponent)
        return;

    UpdatedComponent->SetRelativeLocation(UpdatedComponent->GetRelativeLocation() + CalculateVelocity(DeltaTime));
}

FVector UProjectileMovementComponent2::CalculateVelocity(float DeltaTime)
{
    if (!UpdatedComponent)
        return FVector(0.f, 0.f, 0.f);

    if (bIsGravityEnabled)
        Acceleration.Z = Acceleration.Z + GravitationalAcceleration * DeltaTime;

    Velocity += Acceleration * DeltaTime;

    return Velocity * DeltaTime;
}

void UProjectileMovementComponent2::BeginPlay()
{
    Velocity = InitialVelocity;
    Acceleration = InitialAcceleration;
}

void UProjectileMovementComponent2::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);

    OutProps.push_back({"InitialVelocity", EPropertyType::Vec3, &InitialVelocity});
    OutProps.push_back({"InitialAcceleration", EPropertyType::Vec3, &InitialAcceleration});

    OutProps.push_back({"GravityEnabled", EPropertyType::Bool, &bIsGravityEnabled});
    OutProps.push_back({"GravitationalAcceleration", EPropertyType::Float, &GravitationalAcceleration});
}
