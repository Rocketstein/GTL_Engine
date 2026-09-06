#pragma once

#include "GameFramework/AActor.h"
#include "GameFramework/ProjectileActor/ProjectileActor.h"
#include "sol.hpp"

class AHomingMissileActor;
class UDecalComponent;
class USceneComponent;
class UScriptComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureUIComponent;

struct FTankWeaponAttackParams
{
    float Damage = 10.0f;

    float ProjectileSpeed = 20.0f;
    float ProjectileScale = 1.0f;
    float ColliderSize = 1.0f;
    int32 Pierce = 0;

    float TurnSpeed = 120.0f;
    float ImpactRadius = 0.0f;

    float Range = 10000.0f;
    float Radius = 0.0f;
    float TickInterval = 1.0f;
    int32 TargetCount = 1;

    float Duration = 0.0f;
};

using FTankMainGunFireParams = FTankWeaponAttackParams;

class ATankActor : public AActor
{
public:
    DECLARE_CLASS(ATankActor, AActor)

    void BeginPlay() override;
    void BindScriptFunctions(UScriptComponent& ScriptComponent) override;
    void InitDefaultComponents() override;

    FTankWeaponAttackParams ReadWeaponAttackParamsFromLua(sol::object ParamsObject) const;

    void EquipWeaponVisual(const FString& WeaponId, int32 Level);
    void EquipWeaponVisualFromLua(const FString& WeaponId, int32 Level, sol::table LayoutTable);
    void ResetRuntimeAddedComponents();

    void FireLinearProjectile(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex);
    void FireHomingMissile(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex, AActor* TargetActor);
    void ApplyTargetDamage(const FString& WeaponId, const FTankWeaponAttackParams& Params, AActor* TargetActor);
    void NotifyInstantHit(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 SlotIndex, AActor* TargetActor);
    void ApplyAreaDamage(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Center, float Radius);
    void SpawnInstallable(const FString& WeaponId, const FTankWeaponAttackParams& Params, const FVector& Position);
    void SpawnSummon(const FString& WeaponId, const FTankWeaponAttackParams& Params, int32 MuzzleIndex);

    AProjectileActor* GetProjectileActor();

    void FireHeadMainGun(const FTankWeaponAttackParams& Params);
    void FireHeadMainGun();

private:
    UStaticMesh* GetHeadGunProjectile();
    UStaticMesh* GetBasicMesh(const FString& RelativePath);
    UStaticMesh* GetProjectileMeshForWeapon(const FString& WeaponId);

    AProjectileActor* AcquireProjectile();
    AHomingMissileActor* AcquireHomingMissile();

    USceneComponent* GetWeaponMuzzle(const FString& WeaponId, int32 MuzzleIndex);
    void HideWeaponVisuals(const FString& WeaponId);
    USceneComponent* FindSceneComponentByName(const FString& ComponentName) const;
    UStaticMeshComponent* FindStaticMeshComponentByName(const FString& ComponentName) const;
    UDecalComponent* FindDecalComponentByName(const FString& ComponentName) const;
    UStaticMeshComponent* GetOrCreateWeaponVisualComponent(const FString& Name, const FString& MeshPath, const FString& ParentName);
    UDecalComponent* GetOrCreateWeaponDecalComponent(const FString& Name, const FString& MaterialPath, const FString& ParentName);
    class UPointLightComponent* GetOrCreateWeaponLightComponent(const FString& Name, const FString& ParentName);
    USceneComponent* GetOrCreateMuzzleComponent(const FString& Name, USceneComponent* Parent);
    void EnsureHealthBarComponents();
    UTextureUIComponent* GetOrCreateTextureUIComponent(const FString& Name, USceneComponent* Parent);
    void ConfigureHealthBarComponent(UTextureUIComponent* Component, const FVector2& WorldSize, const FVector2& Pivot, const FVector4& TintColor, int32 ZOrder);
    FString ReadLuaStringOrDefault(sol::object Object, const FString& DefaultValue = "") const;
    FVector ReadLuaVec3OrDefault(sol::object Object, const FVector& DefaultValue = FVector::ZeroVector) const;
    FRotator ReadLuaRotatorOrDefault(sol::object Object, const FRotator& DefaultValue = FRotator::ZeroRotator) const;
    FVector4 ReadLuaVec4OrDefault(sol::object Object, const FVector4& DefaultValue = FVector4(1, 1, 1, 1)) const;

private:
    UPointLightComponent* FindPointLightComponentByName(const FString& ComponentName) const;

private:
    UStaticMesh* HeadGunProjectile = nullptr;
    UTextureUIComponent* HealthBarBack = nullptr;
    UTextureUIComponent* HealthBarFill = nullptr;

    static constexpr const char* TankScriptPath = "TankScript.lua";
};
