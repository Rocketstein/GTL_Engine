#include "GameFramework/HomingMissileActor.h"

#include "Collision/CollisionChannels.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Platform/Paths.h"
#include "Viewport/GameViewportClient.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_CLASS(AHomingMissileActor, AActor)

namespace
{
FRotator RotationFromDirection(const FVector& Direction)
{
    const FVector Normalized = Direction.Normalized();
    const float Yaw = std::atan2(Normalized.Y, Normalized.X) * RAD_TO_DEG;
    const float HorizontalLength = std::sqrt(Normalized.X * Normalized.X + Normalized.Y * Normalized.Y);
    const float Pitch = std::atan2(Normalized.Z, HorizontalLength) * RAD_TO_DEG;
    return FRotator(Pitch, Yaw, 0.0f);
}

float Max3(float A, float B, float C)
{
    const float AB = A > B ? A : B;
    return AB > C ? AB : C;
}

FString GetExplosionShakeAssetPath()
{
    return FPaths::ContentRelativePath("CameraShakes/Explosion.shake");
}
}

void AHomingMissileActor::InitDefaultComponents()
{
    SetFName("HomingMissile");

    UCircleCollider2DComponent* Collider = AddComponent<UCircleCollider2DComponent>();
    SetRootComponent(Collider);
    Collider->SetFName("HomingMissileCollider");
    Collider->SetCollisionChannel(ECollisionChannel::Projectile);
    Collider->SetGenerateOverlapEvents(true);
    Collider->SetRadius(ColliderSize);

    UStaticMeshComponent* Mesh = AddComponent<UStaticMeshComponent>();
    RootComponent->AddChild(Mesh);
    Mesh->SetFName("HomingMissileMesh");
    Mesh->SetRelativeScale(FVector(0.6f, 0.6f, 0.6f));
    Mesh->SetStaticMesh(LoadDefaultMesh());
}

void AHomingMissileActor::Tick(float DeltaTime)
{
    if (UWorld* World = GetWorld())
    {
        if (World->IsGameplayPaused())
        {
            return;
        }
    }

    if (!bActive)
    {
        return;
    }

    if (!TargetActor || !TargetActor->IsVisible())
    {
        Explode();
        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    const FVector TargetLocation = TargetActor->GetActorLocation();
    const FVector ToTarget = TargetLocation - CurrentLocation;
    const float DistanceSquared = ToTarget.LengthSquared();
    const float ExplodeDistance = Max3(ColliderSize, ImpactRadius, 0.5f);

    if (DistanceSquared <= ExplodeDistance * ExplodeDistance)
    {
        Explode();
        return;
    }

    const FVector DesiredDirection = ToTarget.Normalized();
    const float TurnAlpha = FMath::Clamp((TurnSpeed * DeltaTime) / 180.0f, 0.0f, 1.0f);
    CurrentDirection = FMath::Lerp(CurrentDirection, DesiredDirection, TurnAlpha).Normalized();

    AddActorWorldOffset(CurrentDirection * Speed * DeltaTime);
    SetActorRotation(RotationFromDirection(CurrentDirection));
}

void AHomingMissileActor::Fire()
{
    bActive = true;
    CurrentDirection = GetActorForward().Normalized();
    if (CurrentDirection.LengthSquared() <= EPSILON)
    {
        CurrentDirection = FVector(1.0f, 0.0f, 0.0f);
    }
}

UStaticMesh* AHomingMissileActor::LoadDefaultMesh() const
{
    if (!GEngine)
    {
        return nullptr;
    }

    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    if (!Device)
    {
        return nullptr;
    }

    return FObjManager::Get().Load(FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ"));
}

void AHomingMissileActor::Explode()
{
    if (!bActive)
    {
        return;
    }

    bActive = false;

    if (GEngine && GEngine->GetGameViewportClient())
    {
        GEngine->GetGameViewportClient()->StartCameraShakeFromAsset(GetExplosionShakeAssetPath());
    }

    // TODO:
    // - ImpactRadius > 0: query enemies in radius and apply Damage.
    // - ImpactRadius <= 0: apply Damage to TargetActor.
    // - DamageSystem is Lua-based, so decide whether this actor calls Lua or delegates to a script.
    UE_LOG(Tank, Warning,
           "[HomingMissile] Explode called but damage application is not implemented yet. Damage=%.2f Radius=%.2f Target=%s",
           Damage,
           ImpactRadius,
           TargetActor ? TargetActor->GetFName().ToString().c_str() : "None");

    ReturnOrDeactivate();
}

void AHomingMissileActor::ReturnOrDeactivate()
{
    TargetActor = nullptr;
    RequestReturnToPool();
    Deactivate();
}
