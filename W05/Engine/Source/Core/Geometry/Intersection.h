#pragma once

#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Geometry/Primitives/Ray.h"
#include "Core/Geometry/Primitives/Triangle.h"
#include "Core/Math/MathUtility.h"

namespace Geometry
{
    inline bool IntersectRayAABB(const FRay &Ray, const FAABB &Box, float &OutT)
    {
        const float Tx1 = (Box.Min.X - Ray.Origin.X) / Ray.Direction.X;
        const float Tx2 = (Box.Max.X - Ray.Origin.X) / Ray.Direction.X;

        float TMin = (std::min)(Tx1, Tx2);
        float TMax = (std::max)(Tx1, Tx2);

        const float Ty1 = (Box.Min.Y - Ray.Origin.Y) / Ray.Direction.Y;
        const float Ty2 = (Box.Max.Y - Ray.Origin.Y) / Ray.Direction.Y;

        TMin = (std::max)(TMin, (std::min)(Ty1, Ty2));
        TMax = (std::min)(TMax, (std::max)(Ty1, Ty2));

        const float Tz1 = (Box.Min.Z - Ray.Origin.Z) / Ray.Direction.Z;
        const float Tz2 = (Box.Max.Z - Ray.Origin.Z) / Ray.Direction.Z;

        TMin = (std::max)(TMin, (std::min)(Tz1, Tz2));
        TMax = (std::min)(TMax, (std::max)(Tz1, Tz2));

        if (TMax < 0.0f || TMin > TMax)
        {
            return false;
        }

        OutT = TMin;
        return true;
    }

    inline bool IntersectRayTriangle(const FVector3 &O, const FVector3 &D,
                                     const Geometry::FTriangle &Tri, float &OutT)
    {
        const FVector3 E1 = Tri.V1 - Tri.V0;
        const FVector3 E2 = Tri.V2 - Tri.V0;
        const FVector3 H = FVector3::CrossProduct(D, E2);
        const float    A = FVector3::DotProduct(E1, H);

        if (std::fabs(A) < FMath::Epsilon)
            return false;

        const float    F = 1.f / A;
        const FVector3 S = O - Tri.V0;
        const float    U = F * FVector3::DotProduct(S, H);
        if (U < 0.f || U > 1.f)
            return false;

        const FVector3 Q = FVector3::CrossProduct(S, E1);
        const float    V = F * FVector3::DotProduct(D, Q);
        if (V < 0.f || U + V > 1.f)
            return false;

        const float T = F * FVector3::DotProduct(E2, Q);
        if (T < FMath::Epsilon)
            return false;

        OutT = T;
        return true;
    }

    inline bool IntersectRayTriangle(const FVector3 &O, const FVector3 &D, const FVector3 &V0,
                                     const FVector3 &E1, const FVector3 &E2, float &OutT)
    {
        const FVector3 H = FVector3::CrossProduct(D, E2);
        const float    A = FVector3::DotProduct(E1, H);

        if (std::fabs(A) < FMath::Epsilon)
            return false;

        const float    F = 1.f / A;
        const FVector3 S = O - V0;
        const float    U = F * FVector3::DotProduct(S, H);
        if (U < 0.f || U > 1.f)
            return false;

        const FVector3 Q = FVector3::CrossProduct(S, E1);
        const float    V = F * FVector3::DotProduct(D, Q);
        if (V < 0.f || U + V > 1.f)
            return false;

        const float T = F * FVector3::DotProduct(E2, Q);
        if (T < FMath::Epsilon)
            return false;

        OutT = T;
        return true;
    }

    inline bool IntersectRayTriangles(const FVector3 &O, const FVector3 &D,
                                      const TArray<Geometry::FTriangle> &Tris, float &OutT)
    {
        for (int i = 0; i < Tris.size(); ++i)
        {
            if (IntersectRayTriangle(O, D, Tris[i], OutT))
            {

                return true;
            }
        }
    }
} // namespace Geometry
