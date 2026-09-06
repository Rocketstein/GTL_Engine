#pragma once

#include "ImGui/imgui.h"

namespace EditorAccentColor
{
    inline constexpr ImVec4 Value = ImVec4(0.10f, 0.54f, 0.96f, 1.0f);

    inline constexpr ImVec4 WithAlpha(float Alpha)
    {
        return ImVec4(Value.x, Value.y, Value.z, Alpha);
    }

    inline constexpr ImU32 ToU32(int Alpha = 255)
    {
        return IM_COL32(26, 138, 245, Alpha);
    }
}
