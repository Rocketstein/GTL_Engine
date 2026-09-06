#pragma once
#include "GameFramework/AActor.h"
#include "Platform/Paths.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Mesh/StaticMesh.h"

struct ProjectileInfo
{
    UStaticMesh* MeshAsset = nullptr;
    float ColliderSize = 1.0f;
    FVector Position = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Dir = FVector(1.0f, 0.0f, 0.0f);
    float Speed = 0.0f;

    ProjectileInfo() = default;

    ProjectileInfo(
        UStaticMesh* InMeshAsset,
        float InColliderSize,
        const FVector& InPosition,
        const FRotator& InRotation,
        const FVector& InDir,
        float InSpeed)
        : MeshAsset(InMeshAsset)
        , ColliderSize(InColliderSize)
        , Position(InPosition)
        , Rotation(InRotation)
        , Dir(InDir)
        , Speed(InSpeed)
    {
    }
};

class AProjectileActor : public AActor
{
public:
    DECLARE_CLASS(AProjectileActor, AActor)

    void InitDefaultComponents();
    void BindScriptFunctions(UScriptComponent& ScriptComponent) override;
    void SetDirection(const FVector& InDirection) { Direction = InDirection.Normalized(); }
    void SetSpeed(float InSpeed) { Speed = InSpeed; }
    virtual FString GetMeshPath() { return FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ"); }
    void SetColiderSize(float Size)
    {
        if (ColliderComponentIndex < 0 || ColliderComponentIndex >= static_cast<int>(OwnedComponents.size()))
        {
            return;
        }

        UCircleCollider2DComponent* Collider = GetComponent<UCircleCollider2DComponent>(ColliderComponentIndex);
        if (Collider)
        {
            Collider->SetRadius(Size);
        }
    }

    void Fire();

    void SetProjectileSetting(const ProjectileInfo& InProjectileInfo);

    void SetDamage(float InDamage) { Damage = InDamage; }
    float GetDamage() const { return Damage; }
    void SetPierceCount(int32 InPierceCount) { PierceCount = InPierceCount; }
    int32 GetPierceCount() const { return PierceCount; }
    int32 ConsumePierce();

private:
    void NotifyProjectileFiredToScript();

private:
    FVector Direction = { 0.f, 0.f, 0.f };
    float Speed = 0;
    float Damage = 10.0f;
    int32 PierceCount = 0;

    int ColliderComponentIndex = -1;
    int MovementComponentIndex = -1;
    int StaticMeshComponentIndex = -1;
};
