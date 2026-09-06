#include "LuaBindingInternal.h"
#include "Scripting/LuaEngineBinding.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/GamejamActor/AFlyingWaveEnemyActor.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Scripting/LuaScriptTypes.h"
#include <algorithm>

#include "GameFramework/GamejamActor/SubUVVfxActor.h"

namespace
{
    UClass* FindClassByName(const FString& ClassName)
    {
        if (ClassName.empty()) return nullptr;
        for (UClass* Class : UClass::GetAllClasses())
        {
            if (Class && ClassName == Class->GetName()) return Class;
        }
        return nullptr;
    }

    bool DoesComponentMatchClass(UActorComponent* Component, const FString& ClassName)
    {
        if (!Component) return false;
        if (ClassName.empty()) return true;
        UClass* TargetClass = FindClassByName(ClassName);
        if (!TargetClass || !Component->GetClass()) return false;
        return Component->GetClass()->IsA(TargetClass);
    }
}

FLuaActorHandle::FLuaActorHandle(const AActor* InActor)
    : UUID(InActor ? InActor->GetUUID() : 0)
{
}

AActor* FLuaActorHandle::Resolve() const
{
    if (UUID == 0) return nullptr;
    UObject* Object = UObjectManager::Get().FindByUUID(UUID);
    return Cast<AActor>(Object);
}

bool FLuaActorHandle::IsValid() const
{
    return Resolve() != nullptr;
}

FLuaWorldHandle FLuaActorHandle::GetWorld() const
{
    AActor* Actor = Resolve();
    return FLuaWorldHandle(Actor ? Actor->GetWorld() : nullptr);
}

FString FLuaActorHandle::GetName() const
{
    AActor* Actor = Resolve();
    return Actor ? Actor->GetFName().ToString() : "";
}

FString FLuaActorHandle::GetActorClassName() const
{
    AActor* Actor = Resolve();
    return Actor && Actor->GetClass() ? Actor->GetClass()->GetName() : "";
}

FString FLuaActorHandle::GetTag() const
{
    AActor* Actor = Resolve();
    return Actor ? Actor->GetActorTag().ToString() : "";
}

void FLuaActorHandle::SetTag(const FString& InTag) const
{
    AActor* Actor = Resolve();
    if (Actor) Actor->SetActorTag(FName(InTag));
}

sol::table FLuaActorHandle::GetLocation(sol::this_state State) const
{
    sol::state_view Lua(State);
    AActor* Actor = Resolve();
    return MakeLuaVec3(Lua, Actor ? Actor->GetActorLocation() : FVector(0.f, 0.f, 0.f));
}

bool FLuaActorHandle::SetLocation(const sol::object& Value) const
{
    FVector Location;
    if (!ReadLuaVec3(Value, Location)) return false;
    AActor* Actor = Resolve();
    if (!Actor) return false;
    Actor->SetActorLocation(Location);
    return true;
}

bool FLuaActorHandle::AddWorldOffset(const sol::object& Value) const
{
    FVector Delta;
    if (!ReadLuaVec3(Value, Delta)) return false;
    AActor* Actor = Resolve();
    if (!Actor) return false;
    Actor->AddActorWorldOffset(Delta);
    return true;
}

sol::table FLuaActorHandle::GetRotation(sol::this_state State) const
{
    sol::state_view Lua(State);
    AActor* Actor = Resolve();
    return MakeLuaVec3(Lua, Actor ? Actor->GetActorRotation().ToVector() : FVector(0.f, 0.f, 0.f));
}

bool FLuaActorHandle::SetRotation(const sol::object& Value) const
{
    FVector Rotation;
    if (!ReadLuaVec3(Value, Rotation)) return false;
    AActor* Actor = Resolve();
    if (!Actor) return false;
    Actor->SetActorRotation(Rotation);
    return true;
}

sol::table FLuaActorHandle::GetScale(sol::this_state State) const
{
    sol::state_view Lua(State);
    AActor* Actor = Resolve();
    return MakeLuaVec3(Lua, Actor ? Actor->GetActorScale() : FVector(1.0f, 1.0f, 1.0f));
}

bool FLuaActorHandle::SetScale(const sol::object& Value) const
{
    FVector Scale;
    if (!ReadLuaVec3(Value, Scale)) return false;
    AActor* Actor = Resolve();
    if (!Actor) return false;
    Actor->SetActorScale(Scale);
    return true;
}

sol::table FLuaActorHandle::GetForward(sol::this_state State) const
{
    sol::state_view Lua(State);
    AActor* Actor = Resolve();
    return MakeLuaVec3(Lua, Actor ? Actor->GetActorForward() : FVector(0.0f, 0.0f, 1.0f));
}

bool FLuaActorHandle::IsVisible() const
{
    AActor* Actor = Resolve();
    return Actor ? Actor->IsVisible() : false;
}

bool FLuaActorHandle::SetVisible(bool bVisible) const
{
    AActor* Actor = Resolve();
    if (!Actor) return false;
    Actor->SetVisible(bVisible);
    return true;
}

float FLuaActorHandle::GetCustomTimeDilation() const
{
    AActor* Actor = Resolve();
    return Actor ? Actor->CustomTimeDilation : 1.0f;
}

void FLuaActorHandle::SetCustomTimeDilation(float InTimeDilation) const
{
    AActor* Actor = Resolve();
    if (Actor) Actor->CustomTimeDilation = InTimeDilation;
}

FLuaComponentHandle FLuaActorHandle::GetComponent(const sol::variadic_args& Args) const
{
    FString ClassName;
    int32 TargetIndex = 1;
    for (const sol::object& Arg : Args)
    {
        if (Arg.is<FString>()) ClassName = Arg.as<FString>();
        else if (Arg.get_type() == sol::type::number) TargetIndex = Arg.as<int32>();
    }
    TargetIndex = std::max(TargetIndex, 1);
    AActor* Actor = Resolve();
    if (!Actor) return FLuaComponentHandle();
    int32 MatchIndex = 1;
    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (!DoesComponentMatchClass(Component, ClassName)) continue;
        if (MatchIndex == TargetIndex) return FLuaComponentHandle(Component);
        ++MatchIndex;
    }
    return FLuaComponentHandle();
}

FLuaComponentHandle FLuaActorHandle::GetComponentByName(const sol::variadic_args& Args) const
{
    FString ClassName;
    FString ComponentName;

    int32 StringIdx = 0;
    for (const sol::object& Arg : Args)
    {
        if (Arg.is<FString>())
        {
            if (StringIdx == 0) ClassName = Arg.as<FString>();
            else if (StringIdx == 1) ComponentName = Arg.as<FString>();
            StringIdx++;
        }
    }

    AActor* Actor = Resolve();
    if (!Actor) return FLuaComponentHandle();

    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (!DoesComponentMatchClass(Component, ClassName)) continue;
        if (!ComponentName.empty() && Component->GetFName().ToString() != ComponentName) continue;
        return FLuaComponentHandle(Component);
    }
    return FLuaComponentHandle();
}

sol::table FLuaActorHandle::GetComponents(sol::this_state State, const sol::variadic_args& Args) const
{
    sol::state_view Lua(State);
    sol::table Result = Lua.create_table();
    FString ClassName;
    for (const sol::object& Arg : Args)
    {
        if (Arg.is<FString>()) { ClassName = Arg.as<FString>(); break; }
    }
    AActor* Actor = Resolve();
    if (!Actor) return Result;
    int32 LuaIndex = 1;
    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (DoesComponentMatchClass(Component, ClassName)) Result[LuaIndex++] = FLuaComponentHandle(Component);
    }
    return Result;
}

bool FLuaActorHandle::SetVfxPreset(const FString& PresetName) const
{
    AActor* Actor = Resolve();
    ASubUVVfxActor* Vfx = Cast<ASubUVVfxActor>(Actor);
    if (!Vfx)
        return false;

    Vfx->SetVfxPreset(PresetName);
    return true;
}

bool FLuaActorHandle::InitFlyingWave(
    const sol::object& DirObj,
    float Speed,
    float LifeTime) const
{
    AActor* Actor = Resolve();
    AFlyingWaveEnemyActor* Flying = Cast<AFlyingWaveEnemyActor>(Actor);

    if (!Flying)
        return false;

    FVector MoveDir;
    if (!ReadLuaVec3(DirObj, MoveDir))
        return false;

    Flying->InitWave(MoveDir, Speed, LifeTime);
    return true;
}

namespace LuaBinding
{
    void RegisterActor(sol::state& Lua)
    {
        Lua.new_usertype<FLuaActorHandle>(
            "Actor",
            sol::no_constructor,

            "IsValid", &FLuaActorHandle::IsValid,
            "GetUUID", &FLuaActorHandle::GetUUID,
            "GetWorld", &FLuaActorHandle::GetWorld,
            "GetName", &FLuaActorHandle::GetName,
            "GetClassName", &FLuaActorHandle::GetActorClassName,

            "GetTag", &FLuaActorHandle::GetTag,
            "SetTag", &FLuaActorHandle::SetTag,

            "GetLocation", &FLuaActorHandle::GetLocation,
            "SetLocation", &FLuaActorHandle::SetLocation,
            "AddWorldOffset", &FLuaActorHandle::AddWorldOffset,

            "GetRotation", &FLuaActorHandle::GetRotation,
            "SetRotation", &FLuaActorHandle::SetRotation,

            "GetScale", &FLuaActorHandle::GetScale,
            "SetScale", &FLuaActorHandle::SetScale,

            "GetForward", &FLuaActorHandle::GetForward,

            "IsVisible", &FLuaActorHandle::IsVisible,
            "SetVisible", &FLuaActorHandle::SetVisible,

            "GetCustomTimeDilation", &FLuaActorHandle::GetCustomTimeDilation,
            "SetCustomTimeDilation", &FLuaActorHandle::SetCustomTimeDilation,

            "GetComponent", &FLuaActorHandle::GetComponent,
            "GetComponentByName", &FLuaActorHandle::GetComponentByName,
            "GetComponents", &FLuaActorHandle::GetComponents,
            "InitFlyingWave", &FLuaActorHandle::InitFlyingWave,
            "SetVfxPreset", &FLuaActorHandle::SetVfxPreset);
            }
            }
