#include "Quat.h"
#include "MathUtility.h"
#include "Matrix.h"
#include "Transform/Rotator.h"
#include <algorithm>
#include <cassert>
#include <cmath>


namespace
{
    constexpr float MatrixConversionTolerance = FMath::SmallNumber;

    inline float ClampFloat(float Value, float MinValue, float MaxValue) noexcept
    {
        return std::max(MinValue, std::min(Value, MaxValue));
    }

    inline FQuat MakeQuatFromNormalizedAxes(const FVector3 &XAxis, const FVector3 &YAxis,
                                            const FVector3 &ZAxis) noexcept
    {
        const float M00 = XAxis.X;
        const float M01 = XAxis.Y;
        const float M02 = XAxis.Z;
        const float M10 = YAxis.X;
        const float M11 = YAxis.Y;
        const float M12 = YAxis.Z;
        const float M20 = ZAxis.X;
        const float M21 = ZAxis.Y;
        const float M22 = ZAxis.Z;

        FQuat       Result;
        const float Trace = M00 + M11 + M22;
        if (Trace > 0.0f)
        {
            const float S = std::sqrt(Trace + 1.0f) * 2.0f;
            Result.W = 0.25f * S;
            Result.X = (M12 - M21) / S;
            Result.Y = (M20 - M02) / S;
            Result.Z = (M01 - M10) / S;
        }
        else if (M00 > M11 && M00 > M22)
        {
            const float S = std::sqrt(1.0f + M00 - M11 - M22) * 2.0f;
            Result.W = (M12 - M21) / S;
            Result.X = 0.25f * S;
            Result.Y = (M10 + M01) / S;
            Result.Z = (M20 + M02) / S;
        }
        else if (M11 > M22)
        {
            const float S = std::sqrt(1.0f + M11 - M00 - M22) * 2.0f;
            Result.W = (M20 - M02) / S;
            Result.X = (M10 + M01) / S;
            Result.Y = 0.25f * S;
            Result.Z = (M21 + M12) / S;
        }
        else
        {
            const float S = std::sqrt(1.0f + M22 - M00 - M11) * 2.0f;
            Result.W = (M01 - M10) / S;
            Result.X = (M20 + M02) / S;
            Result.Y = (M21 + M12) / S;
            Result.Z = 0.25f * S;
        }

        Result.Normalize();
        return Result;
    }

    bool BuildOrthonormalBasisFromXY(const FVector3 &InX, const FVector3 &InY, FVector3 &OutX,
                                     FVector3 &OutY, FVector3 &OutZ) noexcept
    {
        OutX = InX.GetSafeNormal(MatrixConversionTolerance);
        if (OutX.IsNearlyZero(MatrixConversionTolerance))
        {
            return false;
        }

        const FVector3 ProjectedY = InY - OutX * FVector3::DotProduct(InY, OutX);
        OutY = ProjectedY.GetSafeNormal(MatrixConversionTolerance);
        if (OutY.IsNearlyZero(MatrixConversionTolerance))
        {
            return false;
        }

        OutZ = FVector3::CrossProduct(OutX, OutY).GetSafeNormal(MatrixConversionTolerance);
        if (OutZ.IsNearlyZero(MatrixConversionTolerance))
        {
            return false;
        }

        OutY = FVector3::CrossProduct(OutZ, OutX).GetSafeNormal(MatrixConversionTolerance);
        return !OutY.IsNearlyZero(MatrixConversionTolerance);
    }

    bool BuildOrthonormalBasisFromXZ(const FVector3 &InX, const FVector3 &InZ, FVector3 &OutX,
                                     FVector3 &OutY, FVector3 &OutZ) noexcept
    {
        OutX = InX.GetSafeNormal(MatrixConversionTolerance);
        if (OutX.IsNearlyZero(MatrixConversionTolerance))
        {
            return false;
        }

        const FVector3 ProjectedZ = InZ - OutX * FVector3::DotProduct(InZ, OutX);
        OutZ = ProjectedZ.GetSafeNormal(MatrixConversionTolerance);
        if (OutZ.IsNearlyZero(MatrixConversionTolerance))
        {
            return false;
        }

        OutY = FVector3::CrossProduct(OutZ, OutX).GetSafeNormal(MatrixConversionTolerance);
        if (OutY.IsNearlyZero(MatrixConversionTolerance))
        {
            return false;
        }

        OutZ = FVector3::CrossProduct(OutX, OutY).GetSafeNormal(MatrixConversionTolerance);
        return !OutZ.IsNearlyZero(MatrixConversionTolerance);
    }

    bool BuildOrthonormalBasisFromYZ(const FVector3 &InY, const FVector3 &InZ, FVector3 &OutX,
                                     FVector3 &OutY, FVector3 &OutZ) noexcept
    {
        OutY = InY.GetSafeNormal(MatrixConversionTolerance);
        if (OutY.IsNearlyZero(MatrixConversionTolerance))
        {
            return false;
        }

        const FVector3 ProjectedZ = InZ - OutY * FVector3::DotProduct(InZ, OutY);
        OutZ = ProjectedZ.GetSafeNormal(MatrixConversionTolerance);
        if (OutZ.IsNearlyZero(MatrixConversionTolerance))
        {
            return false;
        }

        OutX = FVector3::CrossProduct(OutY, OutZ).GetSafeNormal(MatrixConversionTolerance);
        if (OutX.IsNearlyZero(MatrixConversionTolerance))
        {
            return false;
        }

        OutZ = FVector3::CrossProduct(OutX, OutY).GetSafeNormal(MatrixConversionTolerance);
        return !OutZ.IsNearlyZero(MatrixConversionTolerance);
    }
} // namespace

const FQuat FQuat::Identity(0.0f, 0.0f, 0.0f, 1.0f);

FQuat::FQuat(const FRotator &InRotator) noexcept : FQuat(InRotator.Quaternion()) {}

FQuat::FQuat(const FMatrix &InMatrix) noexcept : X(0.0f), Y(0.0f), Z(0.0f), W(1.0f)
{
    const FMatrix  RotationSource = InMatrix.GetMatrixWithoutTranslation();
    const FVector3 XAxis = RotationSource.GetScaledAxis(EAxis::X);
    const FVector3 YAxis = RotationSource.GetScaledAxis(EAxis::Y);
    const FVector3 ZAxis = RotationSource.GetScaledAxis(EAxis::Z);

    FVector3 OrthoX;
    FVector3 OrthoY;
    FVector3 OrthoZ;

    if (BuildOrthonormalBasisFromXY(XAxis, YAxis, OrthoX, OrthoY, OrthoZ) ||
        BuildOrthonormalBasisFromXZ(XAxis, ZAxis, OrthoX, OrthoY, OrthoZ) ||
        BuildOrthonormalBasisFromYZ(YAxis, ZAxis, OrthoX, OrthoY, OrthoZ))
    {
        *this = MakeQuatFromNormalizedAxes(OrthoX, OrthoY, OrthoZ);
        return;
    }

    *this = Identity;
}

FQuat::FQuat(const FVector3 &Axis, float AngleRad) noexcept : X(0.0f), Y(0.0f), Z(0.0f), W(1.0f)
{
    const FVector3 NormalizedAxis = Axis.GetSafeNormal();
    if (!NormalizedAxis.IsNearlyZero())
    {
        const float HalfAngle = 0.5f * AngleRad;
        const float SinHalf = std::sin(HalfAngle);
        const float CosHalf = std::cos(HalfAngle);
        X = NormalizedAxis.X * SinHalf;
        Y = NormalizedAxis.Y * SinHalf;
        Z = NormalizedAxis.Z * SinHalf;
        W = CosHalf;
        Normalize();
    }
}

FQuat FQuat::MakeFromEuler(const FVector3 &InEulerDegrees) noexcept
{
    return FRotator::MakeFromEuler(InEulerDegrees).Quaternion();
}

float FQuat::DotProduct(const FQuat &A, const FQuat &B) noexcept
{
    return A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W;
}

FQuat FQuat::Slerp(const FQuat &A, const FQuat &B, float Alpha) noexcept
{
    FQuat Start = A.GetNormalized();
    FQuat End = B.GetNormalized();

    float CosTheta = DotProduct(Start, End);
    if (CosTheta < 0.0f)
    {
        End = -End;
        CosTheta = -CosTheta;
    }

    constexpr float LinearThreshold = 0.9995f;
    if (CosTheta > LinearThreshold)
    {
        FQuat Result = (Start * (1.0f - Alpha) + End * Alpha).GetNormalized();
        return Result;
    }

    const float Theta = std::acos(ClampFloat(CosTheta, -1.0f, 1.0f));
    const float SinTheta = std::sin(Theta);
    if (std::fabs(SinTheta) <= 1.e-8f)
    {
        return Start;
    }

    const float WeightA = std::sin((1.0f - Alpha) * Theta) / SinTheta;
    const float WeightB = std::sin(Alpha * Theta) / SinTheta;
    return (Start * WeightA + End * WeightB).GetNormalized();
}

bool FQuat::operator==(const FQuat &Other) const noexcept
{
    return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
}

bool  FQuat::operator!=(const FQuat &Other) const noexcept { return !(*this == Other); }
FQuat FQuat::operator-() const noexcept { return FQuat(-X, -Y, -Z, -W); }
FQuat FQuat::operator+(const FQuat &Other) const noexcept
{
    return FQuat(X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W);
}
FQuat FQuat::operator-(const FQuat &Other) const noexcept
{
    return FQuat(X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W);
}
FQuat FQuat::operator*(float Scale) const noexcept
{
    return FQuat(X * Scale, Y * Scale, Z * Scale, W * Scale);
}
FQuat FQuat::operator/(float Scale) const noexcept
{
    assert(std::fabs(Scale) > 1.e-8f);
    return FQuat(X / Scale, Y / Scale, Z / Scale, W / Scale);
}

FQuat FQuat::operator*(const FQuat &Other) const noexcept
{
    return FQuat(W * Other.X + X * Other.W + Y * Other.Z - Z * Other.Y,
                 W * Other.Y - X * Other.Z + Y * Other.W + Z * Other.X,
                 W * Other.Z + X * Other.Y - Y * Other.X + Z * Other.W,
                 W * Other.W - X * Other.X - Y * Other.Y - Z * Other.Z);
}

FVector3 FQuat::operator*(const FVector3 &InVector) const noexcept
{
    return RotateVector(InVector);
}

FQuat &FQuat::operator+=(const FQuat &Other) noexcept
{
    X += Other.X;
    Y += Other.Y;
    Z += Other.Z;
    W += Other.W;
    return *this;
}

FQuat &FQuat::operator-=(const FQuat &Other) noexcept
{
    X -= Other.X;
    Y -= Other.Y;
    Z -= Other.Z;
    W -= Other.W;
    return *this;
}

FQuat &FQuat::operator*=(float Scale) noexcept
{
    X *= Scale;
    Y *= Scale;
    Z *= Scale;
    W *= Scale;
    return *this;
}

FQuat &FQuat::operator/=(float Scale) noexcept
{
    assert(std::fabs(Scale) > 1.e-8f);
    X /= Scale;
    Y /= Scale;
    Z /= Scale;
    W /= Scale;
    return *this;
}

FQuat &FQuat::operator*=(const FQuat &Other) noexcept
{
    *this = *this * Other;
    return *this;
}

float FQuat::operator|(const FQuat &Other) const noexcept { return DotProduct(*this, Other); }

bool FQuat::Equals(const FQuat &Other, float Tolerance) const noexcept
{
    return (std::fabs(X - Other.X) <= Tolerance && std::fabs(Y - Other.Y) <= Tolerance &&
            std::fabs(Z - Other.Z) <= Tolerance && std::fabs(W - Other.W) <= Tolerance) ||
           (std::fabs(X + Other.X) <= Tolerance && std::fabs(Y + Other.Y) <= Tolerance &&
            std::fabs(Z + Other.Z) <= Tolerance && std::fabs(W + Other.W) <= Tolerance);
}

bool FQuat::IsIdentity(float Tolerance) const noexcept { return Equals(Identity, Tolerance); }

bool FQuat::ContainsNaN() const noexcept
{
    return !std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z) || !std::isfinite(W);
}

float FQuat::SizeSquared() const noexcept { return X * X + Y * Y + Z * Z + W * W; }
float FQuat::Size() const noexcept { return std::sqrt(SizeSquared()); }

bool FQuat::IsNormalized(float Tolerance) const noexcept
{
    return std::fabs(SizeSquared() - 1.0f) <= Tolerance;
}

void FQuat::Normalize(float Tolerance) noexcept
{
    const float LenSq = SizeSquared();
    if (LenSq <= Tolerance)
    {
        *this = Identity;
        return;
    }

    const float InvLen = 1.0f / std::sqrt(LenSq);
    X *= InvLen;
    Y *= InvLen;
    Z *= InvLen;
    W *= InvLen;
}

FQuat FQuat::GetNormalized(float Tolerance) const noexcept
{
    FQuat Result = *this;
    Result.Normalize(Tolerance);
    return Result;
}

FQuat FQuat::Conjugate() const noexcept { return FQuat(-X, -Y, -Z, W); }

FQuat FQuat::Inverse() const noexcept
{
    const float LenSq = SizeSquared();
    if (LenSq <= 1.e-8f)
    {
        return Identity;
    }
    return Conjugate() / LenSq;
}

FVector3 FQuat::RotateVector(const FVector3 &InVector) const noexcept
{
    const FVector3 QVec(X, Y, Z);
    const FVector3 UV = FVector3::CrossProduct(QVec, InVector);
    const FVector3 UUV = FVector3::CrossProduct(QVec, UV);
    return InVector + ((UV * W) + UUV) * 2.0f;
}

FVector3 FQuat::UnrotateVector(const FVector3 &InVector) const noexcept
{
    return Inverse().RotateVector(InVector);
}

float FQuat::GetAngle() const noexcept
{
    const FQuat Q = GetNormalized();
    return 2.0f * std::acos(ClampFloat(Q.W, -1.0f, 1.0f));
}

FVector3 FQuat::GetRotationAxis(float Tolerance) const noexcept
{
    const FQuat Q = GetNormalized();
    const float SinSq = std::max(0.0f, 1.0f - Q.W * Q.W);
    if (SinSq <= Tolerance)
    {
        return FVector3::ForwardVector;
    }
    const float InvSin = 1.0f / std::sqrt(SinSq);
    return FVector3(Q.X * InvSin, Q.Y * InvSin, Q.Z * InvSin);
}

FVector3 FQuat::Euler() const noexcept { return Rotator().Euler(); }

FVector3 FQuat::GetAxisX() const noexcept { return RotateVector(FVector3::ForwardVector); }
FVector3 FQuat::GetAxisY() const noexcept { return RotateVector(FVector3::RightVector); }
FVector3 FQuat::GetAxisZ() const noexcept { return RotateVector(FVector3::UpVector); }
FVector3 FQuat::GetForwardVector() const noexcept { return GetAxisX(); }
FVector3 FQuat::GetRightVector() const noexcept { return GetAxisY(); }
FVector3 FQuat::GetUpVector() const noexcept { return GetAxisZ(); }

float FQuat::AngularDistance(const FQuat &Other) const noexcept
{
    const float Dot = std::fabs(DotProduct(GetNormalized(), Other.GetNormalized()));
    return 2.0f * std::acos(ClampFloat(Dot, -1.0f, 1.0f));
}

void FQuat::EnforceShortestArcWith(const FQuat &Other) noexcept
{
    if (DotProduct(*this, Other) < 0.0f)
    {
        X = -X;
        Y = -Y;
        Z = -Z;
        W = -W;
    }
}

FMatrix FQuat::ToMatrix() const noexcept
{
    const FQuat Q = GetNormalized();
    const float XX = Q.X * Q.X;
    const float YY = Q.Y * Q.Y;
    const float ZZ = Q.Z * Q.Z;
    const float XY = Q.X * Q.Y;
    const float XZ = Q.X * Q.Z;
    const float YZ = Q.Y * Q.Z;
    const float WX = Q.W * Q.X;
    const float WY = Q.W * Q.Y;
    const float WZ = Q.W * Q.Z;

    return FMatrix(1.0f - 2.0f * (YY + ZZ), 2.0f * (XY + WZ), 2.0f * (XZ - WY), 0.0f,
                   2.0f * (XY - WZ), 1.0f - 2.0f * (XX + ZZ), 2.0f * (YZ + WX), 0.0f,
                   2.0f * (XZ + WY), 2.0f * (YZ - WX), 1.0f - 2.0f * (XX + YY), 0.0f, 0.0f, 0.0f,
                   0.0f, 1.0f);
}

FRotator FQuat::Rotator() const noexcept
{
    const FQuat Q = GetNormalized();

    const float SinPitch = ClampFloat(2.0f * (Q.W * Q.Y - Q.Z * Q.X), -1.0f, 1.0f);
    const float Pitch = std::asin(SinPitch);
    const float Yaw =
        std::atan2(2.0f * (Q.W * Q.Z + Q.X * Q.Y), 1.0f - 2.0f * (Q.Y * Q.Y + Q.Z * Q.Z));
    const float Roll =
        std::atan2(2.0f * (Q.W * Q.X + Q.Y * Q.Z), 1.0f - 2.0f * (Q.X * Q.X + Q.Y * Q.Y));

    return FRotator(FMath::RadiansToDegrees(Pitch), FMath::RadiansToDegrees(Yaw),
                    FMath::RadiansToDegrees(Roll));
}
