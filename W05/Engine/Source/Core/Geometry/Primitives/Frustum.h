#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Vector4.h"

namespace Geometry
{
    struct FFrustum
    {
        // Left, Right, Bottom, Top, Near, Far
        FVector4 Planes[6];
        FMatrix  ViewProjection = FMatrix::Identity;

        void BuildFromViewProjection(const FMatrix &InViewProjection)
        {
            ViewProjection = InViewProjection;

            const FMatrix  T = InViewProjection.GetTransposed();
            const FVector4 R0(T.M[0][0], T.M[0][1], T.M[0][2], T.M[0][3]);
            const FVector4 R1(T.M[1][0], T.M[1][1], T.M[1][2], T.M[1][3]);
            const FVector4 R2(T.M[2][0], T.M[2][1], T.M[2][2], T.M[2][3]);
            const FVector4 R3(T.M[3][0], T.M[3][1], T.M[3][2], T.M[3][3]);

            Planes[0] = R3 + R0; // Left
            Planes[1] = R3 - R0; // Right
            Planes[2] = R3 + R1; // Bottom
            Planes[3] = R3 - R1; // Top
            Planes[4] = R2;      // Near (LH, z >= 0)
            Planes[5] = R3 - R2; // Far
        }
    };
} // namespace Geometry
