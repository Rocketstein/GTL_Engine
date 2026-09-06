#include "ProjectileActor.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Component/ProjectileComponent.h"
#include "Engine/Component/ScriptComponent.h"
#include "GameFramework/ActorPoolManager.h"

IMPLEMENT_CLASS(AProjectileActor, AActor)

void AProjectileActor::InitDefaultComponents()
{
    auto ColliderComponent = AddComponent<UCircleCollider2DComponent>();
    ColliderComponent->SetCollisionChannel(ECollisionChannel::Projectile);
    ColliderComponent->SetGenerateOverlapEvents(true);
    SetRootComponent(ColliderComponent);
    auto StaticMeshComponent = AddComponent<UStaticMeshComponent>();
    StaticMeshComponent->SetReceivesDecals(false);
    RootComponent->AddChild(StaticMeshComponent);

    auto MovementComponent = AddComponent<UProjectileMovementComponent2>();
    MovementComponent->SetUpdatedComponent(GetRootComponent());

    for (uint32 i = 0; i < OwnedComponents.size(); i++)
    {
        if (OwnedComponents[i] != ColliderComponent)
            continue;
        ColliderComponentIndex = i;
    }

    for (uint32 i = 0; i < OwnedComponents.size(); i++)
    {
        if (OwnedComponents[i] != MovementComponent)
            continue;
        MovementComponentIndex = i;
    }

    for (uint32 i = 0; i < OwnedComponents.size(); i++)
    {
        if (OwnedComponents[i] != StaticMeshComponent)
            continue;
        StaticMeshComponentIndex = i;
    }

    auto ScriptComponent = AddComponent<UScriptComponent>();
    ScriptComponent->SetScriptPath("ProjectileScript.lua");
}

void AProjectileActor::BindScriptFunctions(UScriptComponent& ScriptComponent)
{
    if (ScriptComponent.GetScriptPath() != "ProjectileScript.lua")
    {
        return;
    }

    ScriptComponent.BindFunction("GetProjectileDamage",
        [this]() -> float
        {
            return Damage;
        });

    ScriptComponent.BindFunction("ConsumePierce",
        [this]() -> int32
        {
            return ConsumePierce();
        });

    ScriptComponent.BindFunction("ReturnToPool",
        [this]()
        {
            RequestReturnToPool();
        });
}

int32 AProjectileActor::ConsumePierce()
{
    // PierceCount stores extra pierces after the first hit.
    --PierceCount;
    return PierceCount;
}


void AProjectileActor::Fire()
{
    NotifyProjectileFiredToScript();

    if (MovementComponentIndex < 0 || MovementComponentIndex >= static_cast<int>(OwnedComponents.size()))
    {
        return;
    }

    auto movementCompoent = Cast<UProjectileMovementComponent2>(OwnedComponents[MovementComponentIndex]);
    if (!movementCompoent)
    {
        return;
    }

    movementCompoent->SetInitialVelocity(Direction * Speed);
}

void AProjectileActor::NotifyProjectileFiredToScript()
{
    for (UActorComponent* Component : OwnedComponents)
    {
        UScriptComponent* Script = Cast<UScriptComponent>(Component);
        if (!Script || Script->GetScriptPath() != "ProjectileScript.lua")
        {
            continue;
        }

        Script->CallFunction("OnProjectileFired");
    }
}

void AProjectileActor::SetProjectileSetting(const ProjectileInfo& InProjectileInfo)
{
    if (StaticMeshComponentIndex < 0 || StaticMeshComponentIndex >= static_cast<int>(OwnedComponents.size()) ||
        ColliderComponentIndex < 0 || ColliderComponentIndex >= static_cast<int>(OwnedComponents.size()))
    {
        return;
    }

    UStaticMeshComponent* StaticMeshComponent = GetComponent<UStaticMeshComponent>(StaticMeshComponentIndex);
    UCircleCollider2DComponent* ColliderComponent = GetComponent<UCircleCollider2DComponent>(ColliderComponentIndex);
    if (!StaticMeshComponent || !ColliderComponent)
    {
        return;
    }

    StaticMeshComponent->SetStaticMesh(InProjectileInfo.MeshAsset);

    ColliderComponent->SetRadius(InProjectileInfo.ColliderSize);

    SetActorLocation(InProjectileInfo.Position);
    SetActorRotation(InProjectileInfo.Rotation);

    SetDirection(InProjectileInfo.Dir);
    SetSpeed(InProjectileInfo.Speed);

	SetColiderSize(InProjectileInfo.ColliderSize);
}
