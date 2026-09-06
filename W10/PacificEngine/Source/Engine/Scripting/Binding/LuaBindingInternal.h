#pragma once

// Note: Core/CoreTypes.h must be included before sol.hpp to ensure the 'check' macro 
// is defined when sol2 performs push/pop_macro, preventing MSVC warning C4602.
#include "Core/CoreTypes.h"
#include "sol.hpp"

namespace LuaBinding
{
    void RegisterCommon(sol::state& Lua);
    void RegisterActor(sol::state& Lua);
    void RegisterComponent(sol::state& Lua);
    void RegisterCollider(sol::state& Lua);
    void RegisterWorld(sol::state& Lua);
    void RegisterPostProcess(sol::state& Lua);
    void RegisterPool(sol::state& Lua);
}
