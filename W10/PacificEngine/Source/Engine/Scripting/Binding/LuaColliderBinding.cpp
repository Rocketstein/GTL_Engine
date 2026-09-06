#include "LuaBindingInternal.h"
#include "Scripting/LuaEngineBinding.h"
#include "Component/Collision/BoxCollider2DComponent.h"
#include "Component/Collision/CircleCollider2DComponent.h"
#include "Scripting/LuaScriptTypes.h"

UCollider2DComponent* FLuaCollider2DHandle::ResolveCollider() const
{
    return Cast<UCollider2DComponent>(Resolve());
}

sol::table FLuaCollider2DHandle::GetShapeWorldLocation2D(sol::this_state State) const
{
    sol::state_view Lua(State);
    UCollider2DComponent* Collider = ResolveCollider();
    return MakeLuaVec2(Lua, Collider ? Collider->GetShapeWorldLocation2D() : FVector2(0.f, 0.f));
}

float FLuaCollider2DHandle::GetCollisionPlaneZ() const
{
    UCollider2DComponent* Collider = ResolveCollider();
    return Collider ? Collider->GetCollisionPlaneZ() : 0.f;
}

sol::table FLuaBoxCollider2DHandle::GetBoxExtent(sol::this_state State) const
{
    sol::state_view Lua(State);
    UBoxCollider2DComponent* Box = Cast<UBoxCollider2DComponent>(Resolve());
    return MakeLuaVec2(Lua, Box ? Box->GetBoxExtent2D() : FVector2(0.f, 0.f));
}

bool FLuaBoxCollider2DHandle::SetBoxExtent(const sol::object& Value) const
{
    FVector2 Extent;
    if (!ReadLuaVec2(Value, Extent)) return false;
    UBoxCollider2DComponent* Box = Cast<UBoxCollider2DComponent>(Resolve());
    if (!Box) return false;
    Box->SetBoxExtent2D(Extent);
    return true;
}

float FLuaCircleCollider2DHandle::GetRadius() const
{
    UCircleCollider2DComponent* Circle = Cast<UCircleCollider2DComponent>(Resolve());
    return Circle ? Circle->GetRadius() : 0.f;
}

bool FLuaCircleCollider2DHandle::SetRadius(float Radius) const
{
	UCircleCollider2DComponent* Circle = Cast<UCircleCollider2DComponent>(Resolve());
	if (!Circle) return false;
	Circle->SetRadius(Radius);
	return true;
}

namespace LuaBinding
{
    void RegisterCollider(sol::state& Lua)
    {
        Lua.new_usertype<FLuaCollider2DHandle>(
            "Collider2D",
            sol::no_constructor,
            sol::base_classes, sol::bases<FLuaComponentHandle>(),
            "GetShapeWorldLocation2D", &FLuaCollider2DHandle::GetShapeWorldLocation2D,
            "GetCollisionPlaneZ", &FLuaCollider2DHandle::GetCollisionPlaneZ);
        
        Lua.new_usertype<FLuaBoxCollider2DHandle>(
            "BoxCollider2D",
            sol::no_constructor,
            sol::base_classes, sol::bases<FLuaCollider2DHandle>(),
            "GetBoxExtent", &FLuaBoxCollider2DHandle::GetBoxExtent,
            "SetBoxExtent", &FLuaBoxCollider2DHandle::SetBoxExtent);

        Lua.new_usertype<FLuaCircleCollider2DHandle>(
            "CircleCollider2D",
            sol::no_constructor,
            sol::base_classes, sol::bases<FLuaCollider2DHandle>(),
            "GetRadius", &FLuaCircleCollider2DHandle::GetRadius,
            "SetRadius", &FLuaCircleCollider2DHandle::SetRadius);
    }
}
