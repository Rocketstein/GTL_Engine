#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"

struct FLinearColor
{
    union
    {
        struct
        {
            float R, G, B, A;
        };
        float RGBA[4];
    };

    FLinearColor()
        : R(0.0f), G(0.0f), B(0.0f), A(1.0f)
    {
    }

    FLinearColor(float InR, float InG, float InB, float InA = 1.0f)
        : R(InR), G(InG), B(InB), A(InA)
    {
    }

    explicit FLinearColor(const FVector& Vector)
        : R(Vector.X), G(Vector.Y), B(Vector.Z), A(1.0f)
    {
    }

    explicit FLinearColor(const FVector4& Vector)
        : R(Vector.X), G(Vector.Y), B(Vector.Z), A(Vector.W)
    {
    }

    explicit FLinearColor(const FColor& Color)
        : R(Color.R / 255.0f), G(Color.G / 255.0f), B(Color.B / 255.0f), A(Color.A / 255.0f)
    {
    }

    FVector4 ToVector4() const
    {
        return FVector4(R, G, B, A);
    }

    FColor ToFColor() const
    {
        return FColor(
            static_cast<uint32>(FMath::Clamp(R, 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<uint32>(FMath::Clamp(G, 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<uint32>(FMath::Clamp(B, 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<uint32>(FMath::Clamp(A, 0.0f, 1.0f) * 255.0f + 0.5f));
    }

    float& Component(int32 Index)
    {
        return RGBA[Index];
    }

    const float& Component(int32 Index) const
    {
        return RGBA[Index];
    }

    FLinearColor operator+(const FLinearColor& Other) const
    {
        return FLinearColor(R + Other.R, G + Other.G, B + Other.B, A + Other.A);
    }

    FLinearColor& operator+=(const FLinearColor& Other)
    {
        R += Other.R;
        G += Other.G;
        B += Other.B;
        A += Other.A;
        return *this;
    }

    FLinearColor operator-(const FLinearColor& Other) const
    {
        return FLinearColor(R - Other.R, G - Other.G, B - Other.B, A - Other.A);
    }

    FLinearColor& operator-=(const FLinearColor& Other)
    {
        R -= Other.R;
        G -= Other.G;
        B -= Other.B;
        A -= Other.A;
        return *this;
    }

    FLinearColor operator*(const FLinearColor& Other) const
    {
        return FLinearColor(R * Other.R, G * Other.G, B * Other.B, A * Other.A);
    }

    FLinearColor& operator*=(const FLinearColor& Other)
    {
        R *= Other.R;
        G *= Other.G;
        B *= Other.B;
        A *= Other.A;
        return *this;
    }

    FLinearColor operator*(float Scalar) const
    {
        return FLinearColor(R * Scalar, G * Scalar, B * Scalar, A * Scalar);
    }

    FLinearColor& operator*=(float Scalar)
    {
        R *= Scalar;
        G *= Scalar;
        B *= Scalar;
        A *= Scalar;
        return *this;
    }

    FLinearColor operator/(const FLinearColor& Other) const
    {
        return FLinearColor(R / Other.R, G / Other.G, B / Other.B, A / Other.A);
    }

    FLinearColor& operator/=(const FLinearColor& Other)
    {
        R /= Other.R;
        G /= Other.G;
        B /= Other.B;
        A /= Other.A;
        return *this;
    }

    FLinearColor operator/(float Scalar) const
    {
        const float InvScalar = 1.0f / Scalar;
        return FLinearColor(R * InvScalar, G * InvScalar, B * InvScalar, A * InvScalar);
    }

    FLinearColor& operator/=(float Scalar)
    {
        const float InvScalar = 1.0f / Scalar;
        R *= InvScalar;
        G *= InvScalar;
        B *= InvScalar;
        A *= InvScalar;
        return *this;
    }

    bool operator==(const FLinearColor& Other) const
    {
        return R == Other.R && G == Other.G && B == Other.B && A == Other.A;
    }

    bool operator!=(const FLinearColor& Other) const
    {
        return !(*this == Other);
    }

    FLinearColor GetClamped(float InMin = 0.0f, float InMax = 1.0f) const
    {
        return FLinearColor(
            FMath::Clamp(R, InMin, InMax),
            FMath::Clamp(G, InMin, InMax),
            FMath::Clamp(B, InMin, InMax),
            FMath::Clamp(A, InMin, InMax));
    }

    FLinearColor CopyWithNewOpacity(float NewOpacity) const
    {
        return FLinearColor(R, G, B, NewOpacity);
    }

    static FLinearColor White() { return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); }
    static FLinearColor Gray() { return FLinearColor(0.5f, 0.5f, 0.5f, 1.0f); }
    static FLinearColor Black() { return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f); }
    static FLinearColor Transparent() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f); }
    static FLinearColor Red() { return FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); }
    static FLinearColor Green() { return FLinearColor(0.0f, 1.0f, 0.0f, 1.0f); }
    static FLinearColor Blue() { return FLinearColor(0.0f, 0.0f, 1.0f, 1.0f); }
    static FLinearColor Yellow() { return FLinearColor(1.0f, 1.0f, 0.0f, 1.0f); }
};

inline FLinearColor operator*(float Scalar, const FLinearColor& Color)
{
    return Color * Scalar;
}
