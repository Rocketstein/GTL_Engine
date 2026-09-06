#pragma once

#include "Core/Math/Vector3.h"

namespace Geometry
{
    struct FTriangle
    {
        FVector3 V0;
        FVector3 V1;
        FVector3 V2;

        constexpr FTriangle() : V0(), V1(), V2() {}

        constexpr FTriangle(const FVector3 &InV0, const FVector3 &InV1, const FVector3 &InV2)
            : V0(InV0), V1(InV1), V2(InV2)
        {
        }
    };
} // namespace Geometry