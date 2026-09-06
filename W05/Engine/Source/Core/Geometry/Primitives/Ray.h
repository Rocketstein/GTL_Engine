#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Vector3.h"
#include "Core/Platform/PlatformTypes.h"

namespace Geometry
{
    struct FRay
    {
        FVector3 Origin;
        FVector3 Direction;

        constexpr FRay() : Origin(), Direction() {}

        constexpr FRay(const FVector3 &InOrigin, const FVector3 &InDirection)
            : Origin(InOrigin), Direction(InDirection)
        {
        }

        static FRay BuildRay(int32 MouseX, int32 MouseY, const FMatrix &ViewProjection,
                             float ViewportWidth, float ViewportHeight)
        {
            if (ViewportWidth <= 0 || ViewportHeight <= 0)
            {
                return Geometry::FRay{};
            }

            const float NDCX =
                (2.0f * static_cast<float>(MouseX) / static_cast<float>(ViewportWidth) - 1.0f);
            const float NDCY =
                1.0f - (2.0f * static_cast<float>(MouseY) / static_cast<float>(ViewportHeight));

            const FVector3 NearPointNDC(NDCX, NDCY, 0.0f);
            const FVector3 FarPointNDC(NDCX, NDCY, 1.0f);

            const FMatrix InvViewProjection = ViewProjection.GetInverse();

            const FVector3 NearWorld = InvViewProjection.TransformPosition(NearPointNDC);
            const FVector3 FarWorld = InvViewProjection.TransformPosition(FarPointNDC);

            const FVector3 Direction = (FarWorld - NearWorld).GetSafeNormal();

            return Geometry::FRay{NearWorld, Direction};
        }
    };
} // namespace Geometry
