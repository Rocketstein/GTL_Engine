#pragma once

#include "Core/CoreTypes.h"

/*
    Temporary cleanup switch for Phase 2.
    - Off: preserve the legacy opaque view-mode override path.
    - On : bypass the legacy opaque override and keep the proxy/material shader.
*/
inline bool GBypassLegacyOpaqueViewModeOverride = false;

inline bool IsLegacyOpaqueViewModeOverrideBypassed()
{
    return GBypassLegacyOpaqueViewModeOverride;
}

inline void SetLegacyOpaqueViewModeOverrideBypassed(bool bBypass)
{
    GBypassLegacyOpaqueViewModeOverride = bBypass;
}

inline const char* GetLegacyOpaqueViewModeOverrideBypassStateName(bool bBypass)
{
    return bBypass ? "On" : "Off";
}
