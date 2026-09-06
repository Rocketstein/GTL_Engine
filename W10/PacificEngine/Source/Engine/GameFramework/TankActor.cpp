#include "TankActor.h"

#include "Collision/CollisionChannels.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Component/Collision/CircleCollider2DComponent.h"
#include "Engine/Component/DecalComponent.h"
#include "Engine/Component/PrimitiveComponent.h"
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Component/PointLightComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Scripting/LuaEngineBinding.h"
#include "Engine/Scripting/LuaScriptTypes.h"
#include "GameFramework/ActorPoolManager.h"
#include "GameFramework/HomingMissileActor.h"
#include "GameFramework/World.h"
#include "Materials/MaterialManager.h"
#include "UI/TextureUIComponent.h"
#include "Sound/SoundManager.h"

#include <algorithm>
#include <string>

#include "Component/SpotLightComponent.h"

IMPLEMENT_CLASS(ATankActor, AActor)

namespace
{
FString MakeWeaponIndexedComponentName(const FString& Prefix, const FString& WeaponId, int32 Index)
{
    return Prefix + "_" + WeaponId + "_" + std::to_string(Index > 0 ? Index : 0);
}

bool StartsWith(const FString& Value, const FString& Prefix)
{
    return Value.size() >= Prefix.size() && Value.compare(0, Prefix.size(), Prefix) == 0;
}

bool IsRuntimeAddedTankComponentName(const FString& ComponentName)
{
    return StartsWith(ComponentName, "Visual_") ||
           StartsWith(ComponentName, "Muzzle_") ||
           StartsWith(ComponentName, "MuzzleFlash_");
}

bool HasRuntimeAddedTankParent(const USceneComponent* SceneComponent)
{
    for (const USceneComponent* Parent = SceneComponent ? SceneComponent->GetParent() : nullptr;
         Parent != nullptr;
         Parent = Parent->GetParent())
    {
        if (IsRuntimeAddedTankComponentName(Parent->GetFName().ToString()))
        {
            return true;
        }
    }

    return false;
}

int32 MaxInt32(int32 A, int32 B)
{
    return A > B ? A : B;
}

constexpr const char* HealthBarBackName = "PlayerHealthBar_Back";
constexpr const char* HealthBarFillName = "PlayerHealthBar_Fill";
constexpr float HealthBarWidth = 2.0f;
constexpr float HealthBarHeight = 0.16f;
constexpr float HealthBarBackPaddingX = 0.16f;
constexpr float HealthBarBackPaddingY = 0.08f;
const FVector HealthBarRelativeLocation(0.0f, 0.0f, 1.8f);

sol::object FindFirstTableArgument(sol::variadic_args Args)
{
    for (const sol::object& Arg : Args)
    {
        if (Arg.is<sol::table>())
        {
            return Arg;
        }
    }
    return sol::object();
}
}

void ATankActor::BeginPlay()
{
    ResetRuntimeAddedComponents();
    GetHeadGunProjectile();
    EnsureHealthBarComponents();

    AActor::BeginPlay();
}

void ATankActor::BindScriptFunctions(UScriptComponent& ScriptComponent)
{
    if (ScriptComponent.GetScriptPath() != TankScriptPath)
    {
        return;
    }

    ScriptComponent.BindFunction("EquipWeaponVisual",
        [this](const std::string& WeaponId, int32 Level, sol::object LayoutObject)
        {
            if (!LayoutObject.valid() || LayoutObject == sol::nil || !LayoutObject.is<sol::table>())
            {
                UE_LOG(Tank, Warning, "EquipWeaponVisual failed: layout is not table. Weapon=%s Level=%d",
                       WeaponId.c_str(), Level);
                return;
            }

            EquipWeaponVisualFromLua(FString(WeaponId), Level, LayoutObject.as<sol::table>());
        });

    ScriptComponent.BindFunction("FireLinearProjectile",
        [this](const std::string& WeaponId, sol::object ParamsObject, int32 MuzzleIndex)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            FireLinearProjectile(FString(WeaponId), Params, MuzzleIndex);
        });

    ScriptComponent.BindFunction("FireHomingMissile",
        [this](const std::string& WeaponId, sol::object ParamsObject, int32 MuzzleIndex, const FLuaActorHandle& TargetActor)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            FireHomingMissile(FString(WeaponId), Params, MuzzleIndex, TargetActor.Resolve());
        });

    ScriptComponent.BindFunction("ApplyTargetDamage",
        [this](const std::string& WeaponId, sol::object ParamsObject, const FLuaActorHandle& TargetActor)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            ApplyTargetDamage(FString(WeaponId), Params, TargetActor.Resolve());
        });

    ScriptComponent.BindFunction("NotifyInstantHit",
        [this](const std::string& WeaponId, sol::object ParamsObject, int32 SlotIndex, const FLuaActorHandle& TargetActor)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            NotifyInstantHit(FString(WeaponId), Params, SlotIndex, TargetActor.Resolve());
        });

    ScriptComponent.BindFunction("ApplyAreaDamage",
        [this](const std::string& WeaponId, sol::object ParamsObject, sol::object CenterObject, float Radius)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            FVector Center = GetActorLocation();
            ReadLuaVec3(CenterObject, Center);
            ApplyAreaDamage(FString(WeaponId), Params, Center, Radius);
        });

    ScriptComponent.BindFunction("SpawnInstallable",
        [this](const std::string& WeaponId, sol::object ParamsObject, sol::object PositionObject)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            FVector Position = GetActorLocation();
            ReadLuaVec3(PositionObject, Position);
            SpawnInstallable(FString(WeaponId), Params, Position);
        });

    ScriptComponent.BindFunction("SpawnSummon",
        [this](const std::string& WeaponId, sol::object ParamsObject, int32 MuzzleIndex)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(ParamsObject);
            SpawnSummon(FString(WeaponId), Params, MuzzleIndex);
        });

    ScriptComponent.BindFunction("FireHeadMainGun",
        [this](sol::variadic_args Args)
        {
            const FTankWeaponAttackParams Params = ReadWeaponAttackParamsFromLua(FindFirstTableArgument(Args));
            FireHeadMainGun(Params);
        });

    ScriptComponent.BindFunction("ResetRuntimeAddedComponents",
        [this]()
        {
            ResetRuntimeAddedComponents();
        });
}

void ATankActor::InitDefaultComponents()
{
    SetActorTag("Player");
    
    auto Collider = AddComponent<UCircleCollider2DComponent>();
    SetRootComponent(Collider);
    Collider->SetFName({ "RootComponent" });
    Collider->SetCollisionChannel(ECollisionChannel::Player);
    Collider->SetGenerateOverlapEvents(true);
    
    auto Hitbox = AddComponent<UCircleCollider2DComponent>();
    Hitbox->SetFName("Hitbox");
    Hitbox->SetRadius(Collider->GetRadius() * 1.03f);
    Hitbox->SetCollisionChannel(ECollisionChannel::PlayerHitbox);
    Hitbox->SetGenerateOverlapEvents(true);
    Hitbox->AttachToComponent(Collider);

    auto PickupSensor = AddComponent<UCircleCollider2DComponent>();
    PickupSensor->SetFName("PickupSensor");
    PickupSensor->SetRadius(Collider->GetRadius() * 3.0f);
    PickupSensor->SetCollisionChannel(ECollisionChannel::PickupSensor);
    PickupSensor->SetGenerateOverlapEvents(true);
    PickupSensor->AttachToComponent(Collider);
    
    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* Asset = FObjManager::Get().Load(FPaths::ContentRelativePath("Models/Tank/Tank_body.obj"));
    auto BodyMesh = AddComponent<UStaticMeshComponent>();
    BodyMesh->SetFName("BodyMesh");
    BodyMesh->SetStaticMesh(Asset);
    BodyMesh->SetRelativeScale(FVector(0.5f));
    BodyMesh->AttachToComponent(Collider);
    
    // 전조등
    auto FrontLightL = AddComponent<USpotLightComponent>();
    FrontLightL->SetFName("FrontLightL");
    FrontLightL->SetRelativeLocation({1.1f, -0.8f, 0.2f});
    FrontLightL->SetRelativeRotation(FRotator(10.9f, 0, 0));
    FrontLightL->SetIntensity(4.0f);
    FrontLightL->SetLightColor(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    FrontLightL->SetAttenuationRadius(10.0f);
    FrontLightL->SetLightFalloffExponent(8.0f);
    FrontLightL->SetInnerConeAngle(9.0f);
    FrontLightL->SetOuterConeAngle(19.0f);
    FrontLightL->AttachToComponent(BodyMesh);
    auto FrontLightR = AddComponent<USpotLightComponent>();
    FrontLightR->SetFName("FrontLightR");
    FrontLightR->SetRelativeLocation({1.1f, 0.8f, 0.2f});
    FrontLightR->SetRelativeRotation(FRotator(10.9f, 0, 0));
    FrontLightR->SetIntensity(4.0f);
    FrontLightR->SetLightColor(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    FrontLightR->SetAttenuationRadius(10.0f);
    FrontLightR->SetLightFalloffExponent(8.0f);
    FrontLightR->SetInnerConeAngle(9.0f);
    FrontLightR->SetOuterConeAngle(19.0f);
    FrontLightR->AttachToComponent(BodyMesh);
    
    // 후미등
    auto RearLightL = AddComponent<USpotLightComponent>();
    RearLightL->SetFName("RearLightL");
    RearLightL->SetRelativeLocation({-1.4f, -0.6f, 0.8f});
    RearLightL->SetRelativeRotation(FRotator(126.0f, 0, 0));
    RearLightL->SetIntensity(0.5f);
    RearLightL->SetLightColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    RearLightL->SetAttenuationRadius(6.0f);
    RearLightL->SetLightFalloffExponent(20.0f);
    RearLightL->SetInnerConeAngle(9.0f);
    RearLightL->SetOuterConeAngle(44.0f);
    RearLightL->AttachToComponent(BodyMesh);
    auto RearLightR = AddComponent<USpotLightComponent>();
    RearLightR->SetFName("RearLightR");
    RearLightR->SetRelativeLocation({-1.4f, 0.6f, 0.8f});
    RearLightR->SetRelativeRotation(FRotator(126.0f, 0, 0));
    RearLightR->SetIntensity(0.5f);
    RearLightR->SetLightColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    RearLightR->SetAttenuationRadius(6.0f);
    RearLightR->SetLightFalloffExponent(20.0f);
    RearLightR->SetInnerConeAngle(9.0f);
    RearLightR->SetOuterConeAngle(44.0f);
    RearLightR->AttachToComponent(BodyMesh);

    auto Script = AddComponent<UScriptComponent>();
    Script->SetScriptPath(TankScriptPath);

    EnsureHealthBarComponents();

    HeadGunProjectile = GetBasicMesh("Models/Bullet/DefaultBullet.OBJ");
}

FTankWeaponAttackParams ATankActor::ReadWeaponAttackParamsFromLua(sol::object ParamsObject) const
{
    FTankWeaponAttackParams Params;

    if (!ParamsObject.valid() || ParamsObject == sol::nil || !ParamsObject.is<sol::table>())
    {
        return Params;
    }

    sol::table Table = ParamsObject.as<sol::table>();

    Params.Damage = Table.get_or("Damage", 10.0f);

    Params.ProjectileSpeed = Table.get_or("ProjectileSpeed", 20.0f);
    Params.ProjectileScale = Table.get_or("ProjectileScale", 1.0f);
    Params.ColliderSize = Table.get_or("ColliderSize", Params.ProjectileScale);
    Params.Pierce = Table.get_or("Pierce", 0);

    Params.TurnSpeed = Table.get_or("TurnSpeed", 120.0f);
    Params.ImpactRadius = Table.get_or("ImpactRadius", 0.0f);

    Params.Range = Table.get_or("Range", 10000.0f);
    Params.Radius = Table.get_or("Radius", 0.0f);
    Params.TickInterval = Table.get_or("TickInterval", 1.0f);
    Params.TargetCount = Table.get_or("TargetCount", 1);

    Params.Duration = Table.get_or("Duration", 0.0f);

    return Params;
}

void ATankActor::EquipWeaponVisual(const FString& WeaponId, int32 Level)
{
    UE_LOG(Tank, Warning, "EquipWeaponVisual(WeaponId, Level) is deprecated. Weapon=%s Level=%d",
           WeaponId.c_str(), Level);
}

void ATankActor::EquipWeaponVisualFromLua(const FString& WeaponId, int32 Level, sol::table LayoutTable)
{
    HideWeaponVisuals(WeaponId);

    sol::object VisualsObject = LayoutTable["Visuals"];
    if (!VisualsObject.valid() || VisualsObject == sol::nil || !VisualsObject.is<sol::table>())
    {
        UE_LOG(Tank, Warning, "EquipWeaponVisualFromLua failed: Visuals missing. Weapon=%s Level=%d",
               WeaponId.c_str(), Level);
        return;
    }

    sol::table Visuals = VisualsObject.as<sol::table>();
    for (const auto& Pair : Visuals)
    {
        sol::object VisualObject = Pair.second;
        if (!VisualObject.valid() || VisualObject == sol::nil || !VisualObject.is<sol::table>())
        {
            continue;
        }

        sol::table VisualDef = VisualObject.as<sol::table>();
        const FString Name = ReadLuaStringOrDefault(VisualDef["Name"]);
        const FString ComponentType = ReadLuaStringOrDefault(VisualDef["Component"], "StaticMesh");
        const FString Mesh = ReadLuaStringOrDefault(VisualDef["Mesh"]);
        const FString Material = ReadLuaStringOrDefault(VisualDef["Material"]);
        const FString Parent = ReadLuaStringOrDefault(VisualDef["Parent"], "RootComponent");

        if (Name.empty())
        {
            UE_LOG(Tank, Warning, "Invalid visual def. Weapon=%s", WeaponId.c_str());
            continue;
        }

        USceneComponent* Visual = nullptr;
        if (ComponentType == "Decal" || ComponentType == "UDecalComponent")
        {
            if (Material.empty())
            {
                UE_LOG(Tank, Warning, "Invalid decal visual def. Weapon=%s Name=%s", WeaponId.c_str(), Name.c_str());
                continue;
            }

            Visual = GetOrCreateWeaponDecalComponent(Name, Material, Parent);
        }
        else if (ComponentType == "PointLight" || ComponentType == "UPointLightComponent")
        {
            UPointLightComponent* Light = GetOrCreateWeaponLightComponent(Name, Parent);
            if (Light)
            {
                const float Intensity = VisualDef.get_or("Intensity", 2.5f);
                const FVector4 Color = ReadLuaVec4OrDefault(VisualDef["Color"], FVector4(1, 1, 1, 1));
                const float Radius = VisualDef.get_or("AttenuationRadius", 10.0f);

                Light->SetIntensity(Intensity);
                Light->SetLightColor(Color);
                Light->SetAttenuationRadius(Radius);
                Light->MarkRenderStateDirty();
            }
            Visual = Light;
        }
        else
        {
            if (Mesh.empty())
            {
                UE_LOG(Tank, Warning, "Invalid mesh visual def. Weapon=%s Name=%s", WeaponId.c_str(), Name.c_str());
                continue;
            }

            Visual = GetOrCreateWeaponVisualComponent(Name, Mesh, Parent);
        }

        if (!Visual)
        {
            continue;
        }

        const FVector Location = ReadLuaVec3OrDefault(VisualDef["Location"], FVector::ZeroVector);
        const FRotator Rotation = ReadLuaRotatorOrDefault(VisualDef["Rotation"], FRotator::ZeroRotator);
        const FVector Scale = ReadLuaVec3OrDefault(VisualDef["Scale"], FVector(1.0f, 1.0f, 1.0f));

        Visual->SetRelativeLocation(Location);
        Visual->SetRelativeRotation(Rotation);
        Visual->SetRelativeScale(Scale);
        if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Visual))
        {
            Primitive->SetVisibility(true);
        }

        const FString MuzzleName = ReadLuaStringOrDefault(VisualDef["MuzzleName"]);
        if (!MuzzleName.empty())
        {
            USceneComponent* Muzzle = GetOrCreateMuzzleComponent(MuzzleName, Visual);
            if (Muzzle)
            {
                const FVector MuzzleLocation = ReadLuaVec3OrDefault(VisualDef["MuzzleLocation"], FVector::ZeroVector);
                const FRotator MuzzleRotation = ReadLuaRotatorOrDefault(VisualDef["MuzzleRotation"], FRotator::ZeroRotator);
                Muzzle->SetRelativeLocation(MuzzleLocation);
                Muzzle->SetRelativeRotation(MuzzleRotation);
                Muzzle->SetActive(true);
            }
        }
    }

    UE_LOG(Tank, Info, "EquipWeaponVisualFromLua success. Weapon=%s Level=%d", WeaponId.c_str(), Level);
}

void ATankActor::ResetRuntimeAddedComponents()
{
    TArray<UActorComponent*> ComponentsToRemove;

    for (UActorComponent* Component : OwnedComponents)
    {
        if (!Component)
        {
            continue;
        }

        if (!IsRuntimeAddedTankComponentName(Component->GetFName().ToString()))
        {
            continue;
        }

        if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
        {
            if (HasRuntimeAddedTankParent(SceneComponent))
            {
                continue;
            }
        }

        ComponentsToRemove.push_back(Component);
    }

    for (UActorComponent* Component : ComponentsToRemove)
    {
        if (Component)
        {
            RemoveComponent(Component);
        }
    }
}

void ATankActor::FireLinearProjectile(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex)
{
    USceneComponent* Muzzle = GetWeaponMuzzle(WeaponId, MuzzleIndex);
    if (!Muzzle)
    {
        UE_LOG(Tank, Warning, "FireLinearProjectile failed. Weapon=%s Muzzle=%d", WeaponId.c_str(), MuzzleIndex);
        return;
    }

    AProjectileActor* Projectile = AcquireProjectile();
    if (!Projectile)
    {
        UE_LOG(Tank, Warning, "FireLinearProjectile failed. Projectile acquire failed.");
        return;
    }

    const FVector FireDir = Muzzle->GetForwardVector();
    const FVector SpawnLocation = Muzzle->GetWorldLocation();

    Projectile->SetProjectileSetting({
        GetProjectileMeshForWeapon(WeaponId),
        Params.ColliderSize,
        SpawnLocation,
        Muzzle->GetWorldRotation().ToRotator(),
        FireDir,
        Params.ProjectileSpeed
    });

    Projectile->SetActorScale(FVector(Params.ProjectileScale, Params.ProjectileScale, Params.ProjectileScale));
    Projectile->SetDamage(Params.Damage);
    Projectile->SetPierceCount(Params.Pierce);

    UE_LOG(Tank, Info, "[%s] FireLinearProjectile Muzzle=%d Damage=%.2f Speed=%.2f",
           WeaponId.c_str(), MuzzleIndex, Params.Damage, Params.ProjectileSpeed);

    Projectile->Fire();
}

void ATankActor::FireHomingMissile(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex, AActor* TargetActor)
{
    if (!TargetActor)
    {
        UE_LOG(Tank, Warning, "FireHomingMissile failed. Target is null.");
        return;
    }

    USceneComponent* Muzzle = GetWeaponMuzzle(WeaponId, MuzzleIndex);
    if (!Muzzle)
    {
        UE_LOG(Tank, Warning, "FireHomingMissile failed. Muzzle not found.");
        return;
    }

    AHomingMissileActor* Missile = AcquireHomingMissile();
    if (!Missile)
    {
        UE_LOG(Tank, Warning, "FireHomingMissile failed. Missile acquire failed.");
        return;
    }

    Missile->SetActorLocation(Muzzle->GetWorldLocation());
    Missile->SetActorRotation(Muzzle->GetWorldRotation().ToRotator());
    Missile->SetActorScale(FVector(Params.ProjectileScale, Params.ProjectileScale, Params.ProjectileScale));
    Missile->SetTargetActor(TargetActor);
    Missile->SetDamage(Params.Damage);
    Missile->SetImpactRadius(Params.ImpactRadius);
    Missile->SetTurnSpeed(Params.TurnSpeed);
    Missile->SetSpeed(Params.ProjectileSpeed);

    UE_LOG(Tank, Info, "[%s] FireHomingMissile Target=%s Damage=%.2f",
           WeaponId.c_str(), TargetActor->GetFName().ToString().c_str(), Params.Damage);

    Missile->Fire();
}

void ATankActor::ApplyTargetDamage(const FString& WeaponId, const FTankWeaponAttackParams& Params, AActor* TargetActor)
{
    if (!TargetActor)
    {
        UE_LOG(Tank, Warning, "[%s] ApplyTargetDamage skipped: target is null. Damage is handled by Lua.",
               WeaponId.c_str());
        return;
    }

    UE_LOG(Tank, Info, "[%s] ApplyTargetDamage effect hook only. Damage=%.2f Target=%s",
           WeaponId.c_str(), Params.Damage, TargetActor->GetFName().ToString().c_str());
}

void ATankActor::NotifyInstantHit(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 SlotIndex, AActor* TargetActor)
{
    if (!TargetActor)
    {
        UE_LOG(Tank, Warning, "[%s] NotifyInstantHit skipped: target is null. Slot=%d",
               WeaponId.c_str(), SlotIndex);
        return;
    }

    USceneComponent* Origin = GetWeaponMuzzle(WeaponId, SlotIndex);
    if (!Origin)
    {
        Origin = FindSceneComponentByName(MakeWeaponIndexedComponentName("Visual", WeaponId, SlotIndex));
    }

    if (!Origin)
    {
        UE_LOG(Tank, Warning, "[%s] NotifyInstantHit failed: visual/muzzle not found. Slot=%d",
               WeaponId.c_str(), SlotIndex);
        return;
    }

    const FVector OriginLocation = Origin->GetWorldLocation();
    UE_LOG(Tank, Info, "[%s] NotifyInstantHit effect hook only. Slot=%d Damage=%.2f Origin=(%.2f, %.2f, %.2f) Target=%s",
           WeaponId.c_str(), SlotIndex, Params.Damage,
           OriginLocation.X, OriginLocation.Y, OriginLocation.Z,
           TargetActor->GetFName().ToString().c_str());
	
	FSoundManager::Get().Play(FName("SubAttack"), ESoundBus::SFX, 0.2f);
}

void ATankActor::ApplyAreaDamage(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Center, float Radius)
{
    UE_LOG(Tank, Warning, "[%s] ApplyAreaDamage is a stub. Damage=%.2f Center=(%.2f, %.2f, %.2f) Radius=%.2f. Implement radius query or use Lua DamageSystem.",
           WeaponId.c_str(), Params.Damage, Center.X, Center.Y, Center.Z, Radius);
}

void ATankActor::SpawnInstallable(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Position)
{
    UE_LOG(Tank, Warning, "[%s] SpawnInstallable is not implemented yet. Damage=%.2f Position=(%.2f, %.2f, %.2f)",
           WeaponId.c_str(), Params.Damage, Position.X, Position.Y, Position.Z);
}

void ATankActor::SpawnSummon(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex)
{
    USceneComponent* Muzzle = GetWeaponMuzzle(WeaponId, MuzzleIndex);
    if (!Muzzle)
    {
        UE_LOG(Tank, Warning, "[%s] SpawnSummon failed: muzzle not found. Muzzle=%d",
               WeaponId.c_str(), MuzzleIndex);
        return;
    }

    const FVector Position = Muzzle->GetWorldLocation();
    UE_LOG(Tank, Warning, "[%s] SpawnSummon is not implemented yet. Damage=%.2f Muzzle=%d Position=(%.2f, %.2f, %.2f)",
           WeaponId.c_str(), Params.Damage, MuzzleIndex, Position.X, Position.Y, Position.Z);
}

void ATankActor::FireHeadMainGun(const FTankWeaponAttackParams& Params)
{
    FireLinearProjectile("MainCannon", Params, 0);
}

void ATankActor::FireHeadMainGun()
{
    FireHeadMainGun(FTankWeaponAttackParams{});
}

UStaticMesh* ATankActor::GetHeadGunProjectile()
{
    if (!HeadGunProjectile)
    {
        HeadGunProjectile = GetBasicMesh("Models/Bullet/DefaultBullet.OBJ");
    }
    return HeadGunProjectile;
}

UStaticMesh* ATankActor::GetBasicMesh(const FString& RelativePath)
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

    return FObjManager::Get().Load(FPaths::ContentRelativePath(RelativePath));
}

UStaticMesh* ATankActor::GetProjectileMeshForWeapon(const FString& WeaponId)
{
    (void)WeaponId;
    return GetHeadGunProjectile();
}

AProjectileActor* ATankActor::GetProjectileActor()
{
    return AcquireProjectile();
}

AProjectileActor* ATankActor::AcquireProjectile()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(Tank, Warning, "AcquireProjectile failed: World is null");
        return nullptr;
    }

    if (World->GetPoolManager())
    {
        if (AProjectileActor* Projectile = World->GetPoolManager()->Acquire<AProjectileActor>())
        {
            return Projectile;
        }
    }

    return World->SpawnActor<AProjectileActor>();
}

AHomingMissileActor* ATankActor::AcquireHomingMissile()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(Tank, Warning, "AcquireHomingMissile failed: World is null");
        return nullptr;
    }

    if (World->GetPoolManager())
    {
        if (AHomingMissileActor* Missile = World->GetPoolManager()->Acquire<AHomingMissileActor>())
        {
            return Missile;
        }
    }

    return World->SpawnActor<AHomingMissileActor>();
}

USceneComponent* ATankActor::GetWeaponMuzzle(const FString& WeaponId, int32 MuzzleIndex)
{
    const int32 SafeIndex = MaxInt32(0, MuzzleIndex);
    USceneComponent* Muzzle = FindSceneComponentByName(MakeWeaponIndexedComponentName("Muzzle", WeaponId, SafeIndex));
    if (Muzzle)
    {
        return Muzzle;
    }

    UE_LOG(Tank, Warning, "Muzzle not found. Weapon=%s Index=%d", WeaponId.c_str(), MuzzleIndex);
    return nullptr;
}

void ATankActor::HideWeaponVisuals(const FString& WeaponId)
{
    const FString VisualPrefix = "Visual_" + WeaponId + "_";
    const FString MuzzlePrefix = "Muzzle_" + WeaponId + "_";

    for (UActorComponent* Component : OwnedComponents)
    {
        if (!Component)
        {
            continue;
        }

        const FString ComponentName = Component->GetFName().ToString();
        if (!StartsWith(ComponentName, VisualPrefix) && !StartsWith(ComponentName, MuzzlePrefix))
        {
            continue;
        }

        Component->SetActive(false);
        if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
        {
            Primitive->SetVisibility(false);
        }
    }
}

USceneComponent* ATankActor::FindSceneComponentByName(const FString& ComponentName) const
{
    for (UActorComponent* Component : OwnedComponents)
    {
        USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
        if (SceneComponent && SceneComponent->GetFName().ToString() == ComponentName)
        {
            return SceneComponent;
        }
    }
    return nullptr;
}

UStaticMeshComponent* ATankActor::FindStaticMeshComponentByName(const FString& ComponentName) const
{
    for (UActorComponent* Component : OwnedComponents)
    {
        UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
        if (MeshComponent && MeshComponent->GetFName().ToString() == ComponentName)
        {
            return MeshComponent;
        }
    }
    return nullptr;
}

UDecalComponent* ATankActor::FindDecalComponentByName(const FString& ComponentName) const
{
    for (UActorComponent* Component : OwnedComponents)
    {
        UDecalComponent* DecalComponent = Cast<UDecalComponent>(Component);
        if (DecalComponent && DecalComponent->GetFName().ToString() == ComponentName)
        {
            return DecalComponent;
        }
    }
    return nullptr;
}

UStaticMeshComponent* ATankActor::GetOrCreateWeaponVisualComponent(const FString& Name, const FString& MeshPath, const FString& ParentName)
{
    const bool bUseWorldParent = ParentName.empty() || ParentName == "World" || ParentName == "None";

    if (UStaticMeshComponent* Existing = FindStaticMeshComponentByName(Name))
    {
        Existing->SetStaticMesh(GetBasicMesh(MeshPath));
        Existing->SetActive(true);
        Existing->SetVisibility(true);
        if (bUseWorldParent)
        {
            Existing->SetParent(nullptr);
        }
        else if (USceneComponent* Parent = FindSceneComponentByName(ParentName))
        {
            Existing->AttachToComponent(Parent);
        }
        return Existing;
    }

    USceneComponent* Parent = nullptr;
    if (!bUseWorldParent)
    {
        Parent = FindSceneComponentByName(ParentName);
        if (!Parent)
        {
            UE_LOG(Tank, Warning, "Weapon visual parent not found: %s", ParentName.c_str());
            return nullptr;
        }
    }

    UStaticMeshComponent* Visual = AddComponent<UStaticMeshComponent>();
    if (!Visual)
    {
        return nullptr;
    }

    Visual->SetFName(Name);
    if (Parent)
    {
        Parent->AddChild(Visual);
    }
    Visual->SetStaticMesh(GetBasicMesh(MeshPath));
    Visual->SetActive(true);
    Visual->SetVisibility(true);
    return Visual;
}

UDecalComponent* ATankActor::GetOrCreateWeaponDecalComponent(const FString& Name, const FString& MaterialPath, const FString& ParentName)
{
    const bool bUseWorldParent = ParentName.empty() || ParentName == "World" || ParentName == "None";
    UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);

    if (UDecalComponent* Existing = FindDecalComponentByName(Name))
    {
        Existing->SetMaterial(0, Material);
        Existing->SetActive(true);
        Existing->SetVisibility(true);
        if (bUseWorldParent)
        {
            Existing->SetParent(nullptr);
        }
        else if (USceneComponent* Parent = FindSceneComponentByName(ParentName))
        {
            Existing->AttachToComponent(Parent);
        }
        return Existing;
    }

    USceneComponent* Parent = nullptr;
    if (!bUseWorldParent)
    {
        Parent = FindSceneComponentByName(ParentName);
        if (!Parent)
        {
            UE_LOG(Tank, Warning, "Weapon decal parent not found: %s", ParentName.c_str());
            return nullptr;
        }
    }

    UDecalComponent* Decal = AddComponent<UDecalComponent>();
    if (!Decal)
    {
        return nullptr;
    }

    Decal->SetFName(Name);
    if (Parent)
    {
        Parent->AddChild(Decal);
    }
    Decal->SetMaterial(0, Material);
    Decal->SetActive(true);
    Decal->SetVisibility(true);
    return Decal;
}

USceneComponent* ATankActor::GetOrCreateMuzzleComponent(const FString& Name, USceneComponent* Parent)
{
    if (USceneComponent* Existing = FindSceneComponentByName(Name))
    {
        Existing->SetActive(true);
        if (Parent)
        {
            Existing->AttachToComponent(Parent);
        }
        return Existing;
    }

    if (!Parent)
    {
        return nullptr;
    }

    USceneComponent* Muzzle = AddComponent<USceneComponent>();
    if (!Muzzle)
    {
        return nullptr;
    }

    Muzzle->SetFName(Name);
    Parent->AddChild(Muzzle);
    Muzzle->SetActive(true);
    return Muzzle;
}

void ATankActor::EnsureHealthBarComponents()
{
    USceneComponent* Parent = GetRootComponent();
    if (!Parent)
    {
        Parent = FindSceneComponentByName("RootComponent");
    }

    if (!Parent)
    {
        UE_LOG(Tank, Warning, "Health bar setup skipped: root component is null.");
        return;
    }

    HealthBarBack = GetOrCreateTextureUIComponent(HealthBarBackName, Parent);
    HealthBarFill = GetOrCreateTextureUIComponent(HealthBarFillName, Parent);

    ConfigureHealthBarComponent(
        HealthBarBack,
        FVector2(HealthBarWidth + HealthBarBackPaddingX, HealthBarHeight + HealthBarBackPaddingY),
        FVector2(0.5f, 8.5f),
        FVector4(0.04f, 0.04f, 0.04f, 0.78f),
        0);

    ConfigureHealthBarComponent(
        HealthBarFill,
        FVector2(HealthBarWidth, HealthBarHeight),
        FVector2(0.5f, 12.5f),
        FVector4(0.18f, 0.95f, 0.28f, 0.95f),
        1);
}

UTextureUIComponent* ATankActor::GetOrCreateTextureUIComponent(const FString& Name, USceneComponent* Parent)
{
    if (USceneComponent* ExistingScene = FindSceneComponentByName(Name))
    {
        UTextureUIComponent* Existing = Cast<UTextureUIComponent>(ExistingScene);
        if (!Existing)
        {
            UE_LOG(Tank, Warning, "UI component name is already used by non-texture UI component: %s", Name.c_str());
            return nullptr;
        }

        Existing->SetActive(true);
        if (Parent)
        {
            Existing->AttachToComponent(Parent);
        }
        return Existing;
    }

    UTextureUIComponent* Component = AddComponent<UTextureUIComponent>();
    if (!Component)
    {
        return nullptr;
    }

    Component->SetFName(Name);
    Component->SetActive(true);
    if (Parent)
    {
        Component->AttachToComponent(Parent);
    }
    return Component;
}

void ATankActor::ConfigureHealthBarComponent(UTextureUIComponent* Component, const FVector2& WorldSize, const FVector2& Pivot, const FVector4& TintColor, int32 ZOrder)
{
    if (!Component)
    {
        return;
    }

    Component->SetRenderSpace(EUIRenderSpace::WorldSpace);
    Component->SetGeometryType(EUIGeometryType::Quad);
    Component->SetTexturePath("");
    Component->ResetSubUVRect();
    Component->SetWorldSize(WorldSize);
    Component->SetBillboard(true);
    Component->SetPivot(Pivot);
    Component->SetRotationDegrees(0.0f);
    Component->SetLayer(100);
    Component->SetZOrder(ZOrder);
    Component->SetHitTestVisible(false);
    Component->SetRelativeLocation(HealthBarRelativeLocation);
    Component->SetTintColor(TintColor);
    Component->SetVisibility(true);
}

FString ATankActor::ReadLuaStringOrDefault(sol::object Object, const FString& DefaultValue) const
{
    if (!Object.valid() || Object == sol::nil)
    {
        return DefaultValue;
    }

    if (Object.is<std::string>())
    {
        return FString(Object.as<std::string>());
    }

    if (Object.is<FString>())
    {
        return Object.as<FString>();
    }

    return DefaultValue;
}

FVector ATankActor::ReadLuaVec3OrDefault(sol::object Object, const FVector& DefaultValue) const
{
    FVector Value = DefaultValue;
    if (ReadLuaVec3(Object, Value))
    {
        return Value;
    }
    return DefaultValue;
}

FRotator ATankActor::ReadLuaRotatorOrDefault(sol::object Object, const FRotator& DefaultValue) const
{
    const FVector DefaultEuler(DefaultValue.Roll, DefaultValue.Pitch, DefaultValue.Yaw);
    const FVector Euler = ReadLuaVec3OrDefault(Object, DefaultEuler);
    return FRotator(Euler.Y, Euler.Z, Euler.X);
}

FVector4 ATankActor::ReadLuaVec4OrDefault(sol::object Object, const FVector4& DefaultValue) const
{
    FVector4 Value = DefaultValue;
    if (ReadLuaVec4(Object, Value))
    {
        return Value;
    }
    return DefaultValue;
}

UPointLightComponent* ATankActor::GetOrCreateWeaponLightComponent(const FString& Name, const FString& ParentName)
{
    const bool bUseWorldParent = ParentName.empty() || ParentName == "World" || ParentName == "None";

    if (UPointLightComponent* Existing = FindPointLightComponentByName(Name))
    {
        Existing->SetActive(true);
        if (bUseWorldParent)
        {
            Existing->SetParent(nullptr);
        }
        else if (USceneComponent* Parent = FindSceneComponentByName(ParentName))
        {
            Existing->AttachToComponent(Parent);
        }
        return Existing;
    }

    USceneComponent* Parent = nullptr;
    if (!bUseWorldParent)
    {
        Parent = FindSceneComponentByName(ParentName);
        if (!Parent)
        {
            UE_LOG(Tank, Warning, "Weapon light parent not found: %s", ParentName.c_str());
            return nullptr;
        }
    }

    UPointLightComponent* Light = AddComponent<UPointLightComponent>();
    if (!Light)
    {
        return nullptr;
    }

    Light->SetFName(Name);
    if (Parent)
    {
        Parent->AddChild(Light);
    }
    Light->SetActive(true);
    return Light;
}

UPointLightComponent* ATankActor::FindPointLightComponentByName(const FString& ComponentName) const
{
    for (UActorComponent* Component : OwnedComponents)
    {
        UPointLightComponent* LightComponent = Cast<UPointLightComponent>(Component);
        if (LightComponent && LightComponent->GetFName().ToString() == ComponentName)
        {
            return LightComponent;
        }
    }
    return nullptr;
}
