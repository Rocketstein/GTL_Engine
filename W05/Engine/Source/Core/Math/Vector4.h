#pragma once

#include "Core/Platform/PlatformTypes.h"
#include "SIMD/DXMathBridge.h"
#include "Vector3.h"
#include <cassert>
#include <cmath>
#include <cstdint>


struct FMatrix;

struct FVector4
{
  public:
    union
    {
        struct
        {
            float X;
            float Y;
            float Z;
            float W;
        };

        float XYZW[4];
    };

  public:
    constexpr FVector4() noexcept : X(0.0f), Y(0.0f), Z(0.0f), W(0.0f) {}

    constexpr FVector4(const float InX, const float InY, const float InZ, const float InW) noexcept
        : X(InX), Y(InY), Z(InZ), W(InW)
    {
    }

    constexpr FVector4(const FVector3 &InVec, const float InW = 0.0f) noexcept
        : X(InVec.X), Y(InVec.Y), Z(InVec.Z), W(InW)
    {
    }

    FVector4(const FVector4 &) noexcept = default;
    FVector4(FVector4 &&) noexcept = default;
    FVector4 &operator=(const FVector4 &) noexcept = default;
    FVector4 &operator=(FVector4 &&) noexcept = default;
    ~FVector4() = default;

    static constexpr FVector4 Zero() noexcept { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static constexpr FVector4 One() noexcept { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static constexpr FVector4 Up() noexcept { return {0.0f, 0.0f, 1.0f, 0.0f}; }
    static constexpr FVector4 Right() noexcept { return {0.0f, 1.0f, 0.0f, 0.0f}; }
    static constexpr FVector4 Forward() noexcept { return {1.0f, 0.0f, 0.0f, 0.0f}; }
    static constexpr FVector4 Point() noexcept { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    float &operator[](int32 Index) noexcept
    {
        assert(Index >= 0 && Index < 4);
        return XYZW[Index];
    }

    const float &operator[](int32 Index) const noexcept
    {
        assert(Index >= 0 && Index < 4);
        return XYZW[Index];
    }

    constexpr FVector4 operator-() const noexcept { return {-X, -Y, -Z, -W}; }

    constexpr FVector4 operator+(const FVector4 &Other) const noexcept
    {
        return {X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W};
    }

    constexpr FVector4 operator-(const FVector4 &Other) const noexcept
    {
        return {X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W};
    }

    constexpr FVector4 operator*(const float S) const noexcept
    {
        return {X * S, Y * S, Z * S, W * S};
    }

    constexpr FVector4 operator/(const float S) const noexcept
    {
        assert(S != 0.0f);
        return {X / S, Y / S, Z / S, W / S};
    }

    FVector4 &operator+=(const FVector4 &Other) noexcept
    {
        X += Other.X;
        Y += Other.Y;
        Z += Other.Z;
        W += Other.W;
        return *this;
    }

    FVector4 &operator-=(const FVector4 &Other) noexcept
    {
        X -= Other.X;
        Y -= Other.Y;
        Z -= Other.Z;
        W -= Other.W;
        return *this;
    }

    FVector4 &operator*=(const float S) noexcept
    {
        X *= S;
        Y *= S;
        Z *= S;
        W *= S;
        return *this;
    }

    FVector4 &operator/=(const float S) noexcept
    {
        assert(S != 0.0f);
        X /= S;
        Y /= S;
        Z /= S;
        W /= S;
        return *this;
    }

    constexpr bool operator==(const FVector4 &Other) const noexcept
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
    }

    constexpr bool operator!=(const FVector4 &Other) const noexcept { return !(*this == Other); }

    DirectX::XMVECTOR ToXMVector() const noexcept { return FDXMathBridge::LoadVector4(*this); }

    [[nodiscard]] float    Dot(const FVector4 &Other) const noexcept;
    [[nodiscard]] FVector4 Cross(const FVector4 &Other) const noexcept;
    [[nodiscard]] float    LengthSquared() const noexcept;
    [[nodiscard]] float    Length() const noexcept;
    [[nodiscard]] FVector4 Normalize() const noexcept;
    [[nodiscard]] bool     IsNearlyEqual(const FVector4 &Other,
                                         float           Tolerance = 1.e-6f) const noexcept;
    [[nodiscard]] bool     IsPoint(float Tolerance = 1.e-6f) const noexcept;
    [[nodiscard]] bool     IsVector(float Tolerance = 1.e-6f) const noexcept;

    FVector4 operator*(const FMatrix &Mat) const;
};
