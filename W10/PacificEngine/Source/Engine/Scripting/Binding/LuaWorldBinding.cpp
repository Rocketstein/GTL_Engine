#include "LuaBindingInternal.h"
#include "Scripting/LuaEngineBinding.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/AWorldSettings.h"

FLuaWorldHandle::FLuaWorldHandle(UWorld* InWorld)
    : World(InWorld)
{
}

bool FLuaWorldHandle::IsValid() const
{
    return World != nullptr;
}

bool FLuaWorldHandle::IsGameplayPaused() const
{
    return World ? World->IsGameplayPaused() : false;
}

void FLuaWorldHandle::SetGameplayPaused(bool bPaused) const
{
    if (World)
    {
        World->SetGameplayPaused(bPaused);
    }
}

float FLuaWorldHandle::GetTimeDilation() const
{
    if (World && World->GetWorldSettings())
    {
        return World->GetWorldSettings()->GetEffectiveTimeDilation();
    }
    return 1.0f;
}

void FLuaWorldHandle::SetTimeDilation(float InTimeDilation) const
{
    if (World && World->GetWorldSettings())
    {
        World->GetWorldSettings()->SetTimeDilation(InTimeDilation);
    }
}

sol::table FLuaWorldHandle::GetActorsByTag(sol::this_state State, const FString& Tag) const
{
    sol::state_view Lua(State);
    sol::table Result = Lua.create_table();
    if (!World) return Result;
    FName TargetTag(Tag);
    int32 Index = 1;
    for (AActor* Actor : World->GetActors())
    {
        if (Actor && Actor->IsVisible() && Actor->GetActorTag() == TargetTag)
        {
            Result[Index++] = FLuaActorHandle(Actor);
        }
    }
    return Result;
}

namespace LuaBinding
{
    void RegisterWorld(sol::state& Lua)
    {
        Lua.new_usertype<FLuaWorldHandle>(
            "World",
            sol::no_constructor,
            "IsValid", &FLuaWorldHandle::IsValid,
            "IsGameplayPaused", &FLuaWorldHandle::IsGameplayPaused,
            "SetGameplayPaused", &FLuaWorldHandle::SetGameplayPaused,
            "GetTimeDilation", &FLuaWorldHandle::GetTimeDilation,
            "SetTimeDilation", &FLuaWorldHandle::SetTimeDilation,
            "GetActorsByTag", &FLuaWorldHandle::GetActorsByTag);
    }
}
