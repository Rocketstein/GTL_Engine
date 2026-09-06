#pragma once

#include "Core/Math/Vector3.h"

namespace Geometry
{
    struct FSegment
    {
        FVector3 Start;
        FVector3 End;

        constexpr FSegment() : Start(), End() {}

        constexpr FSegment(const FVector3 &InStart, const FVector3 &InEnd)
            : Start(InStart), End(InEnd)
        {
        }
    };
} // namespace Geometry