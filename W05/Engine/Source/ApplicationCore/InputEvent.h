#pragma once
#include "Core/Platform/PlatformTypes.h"
enum class EInputEvent : uint8
{
    Pressed,
    Released,
    Repeat,
    DoubleClick
};
enum class EKey : uint8
{
    W,
    A,
    S,
    D,
    Q,
    E,
    LeftMouseButton,
    RightMouseButton,
    MiddleMouseButton,
    MouseWheelAxis
};
enum class EPointerReleaseType : uint8
{
    Click,
    Drag
};
