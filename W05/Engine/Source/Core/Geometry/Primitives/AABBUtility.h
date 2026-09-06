#pragma once

#include "AABB.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector3.h"
#include <cfloat>

namespace Geometry
{
    inline FAABB TransformAABB(const FAABB &InLocalAABB, const FMatrix &InMatrix)
    {
        const FVector3 &Min = InLocalAABB.Min;
        const FVector3 &Max = InLocalAABB.Max;

        const FVector3 Corners[8] = {
            FVector3(Min.X, Min.Y, Min.Z), FVector3(Max.X, Min.Y, Min.Z),
            FVector3(Min.X, Max.Y, Min.Z), FVector3(Max.X, Max.Y, Min.Z),
            FVector3(Min.X, Min.Y, Max.Z), FVector3(Max.X, Min.Y, Max.Z),
            FVector3(Min.X, Max.Y, Max.Z), FVector3(Max.X, Max.Y, Max.Z),
        };

        FVector3 NewMin(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector3 NewMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (const FVector3 &Corner : Corners)
        {
            const FVector3 P = InMatrix.TransformPosition(Corner);

            NewMin.X = (P.X < NewMin.X) ? P.X : NewMin.X;
            NewMin.Y = (P.Y < NewMin.Y) ? P.Y : NewMin.Y;
            NewMin.Z = (P.Z < NewMin.Z) ? P.Z : NewMin.Z;

            NewMax.X = (P.X > NewMax.X) ? P.X : NewMax.X;
            NewMax.Y = (P.Y > NewMax.Y) ? P.Y : NewMax.Y;
            NewMax.Z = (P.Z > NewMax.Z) ? P.Z : NewMax.Z;
        }

        return FAABB(NewMin, NewMax);
    }

    inline void ExpandAABB(const FVector3 &InPoint, FVector3 &InOutMin, FVector3 &InOutMax)
    {
        InOutMin.X = (((InOutMin.X) < (InPoint.X)) ? (InOutMin.X) : (InPoint.X));
        InOutMin.Y = (((InOutMin.Y) < (InPoint.Y)) ? (InOutMin.Y) : (InPoint.Y));
        InOutMin.Z = (((InOutMin.Z) < (InPoint.Z)) ? (InOutMin.Z) : (InPoint.Z));

        InOutMax.X = (((InOutMax.X) > (InPoint.X)) ? (InOutMax.X) : (InPoint.X));
        InOutMax.Y = (((InOutMax.Y) > (InPoint.Y)) ? (InOutMax.Y) : (InPoint.Y));
        InOutMax.Z = (((InOutMax.Z) > (InPoint.Z)) ? (InOutMax.Z) : (InPoint.Z));
    }
} // namespace Geometry