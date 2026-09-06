#include "Vector4.h"
#include "Core/Platform/PlatformTypes.h"
#include "MathUtility.h"
#include "Matrix.h"
#include <cassert>
#include <cmath>


float FVector4::Dot(const FVector4 &Other) const noexcept
{
    return X * Other.X + Y * Other.Y + Z * Other.Z + W * Other.W;
}

FVector4 FVector4::Cross(const FVector4 &Other) const noexcept
{
    return {Y * Other.Z - Z * Other.Y, Z * Other.X - X * Other.Z, X * Other.Y - Y * Other.X, 0.0f};
}

float FVector4::LengthSquared() const noexcept { return X * X + Y * Y + Z * Z + W * W; }

float FVector4::Length() const noexcept { return std::sqrt(LengthSquared()); }

FVector4 FVector4::Normalize() const noexcept
{
    const float LenSq = LengthSquared();
    if (LenSq < FMath::Epsilon)
    {
        return Zero();
    }

    const float InvLen = 1.0f / std::sqrt(LenSq);
    return {X * InvLen, Y * InvLen, Z * InvLen, W * InvLen};
}

bool FVector4::IsNearlyEqual(const FVector4 &Other, float Tolerance) const noexcept
{
    return (std::fabs(X - Other.X) <= Tolerance) && (std::fabs(Y - Other.Y) <= Tolerance) &&
           (std::fabs(Z - Other.Z) <= Tolerance) && (std::fabs(W - Other.W) <= Tolerance);
}

bool FVector4::IsPoint(float Tolerance) const noexcept { return std::fabs(W - 1.0f) <= Tolerance; }

bool FVector4::IsVector(float Tolerance) const noexcept { return std::fabs(W) <= Tolerance; }

FVector4 FVector4::operator*(const FMatrix &Mat) const
{
    FVector4 NewVec4;

    for (int32 Col = 0; Col < 4; ++Col)
    {
        NewVec4.XYZW[Col] =
            X * Mat.M[0][Col] + Y * Mat.M[1][Col] + Z * Mat.M[2][Col] + W * Mat.M[3][Col];
    }
    return NewVec4;
}
