#include "LuaEngineBinding.h"
#include "Binding/LuaBindingInternal.h"

void RegisterLuaEngineBindings(sol::state& Lua)
{
    LuaBinding::RegisterCommon(Lua);
    LuaBinding::RegisterActor(Lua);
    LuaBinding::RegisterComponent(Lua);
    LuaBinding::RegisterCollider(Lua);
    LuaBinding::RegisterWorld(Lua);
    LuaBinding::RegisterPostProcess(Lua);
    LuaBinding::RegisterPool(Lua);
}
