#include "LuaBindingInternal.h"
#include "Scripting/LuaEngineBinding.h"
#include "GameFramework/ActorPoolManager.h"

FLuaActorPoolManagerHandle::FLuaActorPoolManagerHandle(FActorPoolManager* InManager)
	: Manager(InManager)
{
}

bool FLuaActorPoolManagerHandle::IsValid() const
{
	return Manager != nullptr;
}

void FLuaActorPoolManagerHandle::Warmup(const FString& ClassName, uint32 Count) const
{
	if (Manager) Manager->Warmup(ClassName, Count);
}

FLuaActorHandle FLuaActorPoolManagerHandle::Acquire(const FString& ClassName) const
{
	if (!Manager) return FLuaActorHandle();
	return FLuaActorHandle(Manager->Acquire(ClassName));
}

void FLuaActorPoolManagerHandle::Release(const FLuaActorHandle& Actor) const
{
	if (Manager) Manager->Release(Actor.Resolve());
}

void FLuaActorPoolManagerHandle::ReleaseActiveByClass(const FString& ClassName) const
{
	if (Manager) Manager->ReleaseActiveByClass(ClassName);
}

uint32 FLuaActorPoolManagerHandle::GetActiveCount(const FString& ClassName) const
{
	return Manager ? Manager->GetActiveCount(ClassName) : 0;
}

namespace LuaBinding
{
    void RegisterPool(sol::state& Lua)
    {
        Lua.new_usertype<FLuaActorPoolManagerHandle>(
            "ActorPoolManager",
            sol::no_constructor,
            "IsValid", &FLuaActorPoolManagerHandle::IsValid,
            "Warmup", &FLuaActorPoolManagerHandle::Warmup,
            "Acquire", &FLuaActorPoolManagerHandle::Acquire,
            "Release", &FLuaActorPoolManagerHandle::Release,
            "ReleaseActiveByClass", &FLuaActorPoolManagerHandle::ReleaseActiveByClass,
            "GetActiveCount", &FLuaActorPoolManagerHandle::GetActiveCount);
    }
}
