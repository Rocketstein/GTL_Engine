#pragma once

#include "Core/Platform/PlatformTypes.h"
#include "SIMD/DXMathBridge.h"
#include "Vector3.h"
#include <cassert>
#include <cmath>
#include <cstdint>

struct FVector2
{
  public:
    union
    {
        struct
        {
            float X;
            float Y;
        };

        float XY[2];
    };

    static const FVector2 ZeroVector;
    static const FVector2 OneVector;

    static inline FVector2 Zero() { return ZeroVector; }
    static inline FVector2 One() { return OneVector; }
    static inline FVector2 UnitX() { return {1.f, 0.f}; }
    static inline FVector2 UnitY() { return {0.f, 1.f}; }

    //======================================//
    //				constructor				//
    //======================================//
  public:
    constexpr FVector2() noexcept : X(0.f), Y(0.f) {}

    constexpr FVector2(const float InX, const float InY) noexcept : X(InX), Y(InY) {}

    FVector2(const FVector2 &) noexcept = default;
    FVector2(FVector2 &&) noexcept = default;

    //======================================//
    //				operators				//
    //======================================//
  public:
    FVector2 &operator=(const FVector2 &) noexcept = default;
    FVector2 &operator=(FVector2 &&) noexcept = default;

    float &operator[](int32 Index) noexcept
    {
        assert(Index >= 0 && Index < 2);
        return XY[Index];
    }

    const float &operator[](int32 Index) const noexcept
    {
        assert(Index >= 0 && Index < 2);
        return XY[Index];
    }

    constexpr bool operator==(const FVector2 &Other) const noexcept
    {
        return X == Other.X && Y == Other.Y;
    }

    constexpr bool operator!=(const FVector2 &Other) const noexcept { return !(*this == Other); }

    constexpr FVector2 operator-() const noexcept { return {-X, -Y}; }

    constexpr FVector2 operator+(const FVector2 &Other) const noexcept
    {
        return {X + Other.X, Y + Other.Y};
    }

    constexpr FVector2 operator-(const FVector2 &Other) const noexcept
    {
        return {X - Other.X, Y - Other.Y};
    }

    constexpr FVector2 operator*(float Scalar) const noexcept { return {X * Scalar, Y * Scalar}; }

    constexpr FVector2 operator/(float Scalar) const noexcept
    {
        assert(Scalar != 0.f);
        return {X / Scalar, Y / Scalar};
    }

    FVector2 &operator+=(const FVector2 &Other) noexcept
    {
        X += Other.X;
        Y += Other.Y;
        return *this;
    }

    FVector2 &operator-=(const FVector2 &Other) noexcept
    {
        X -= Other.X;
        Y -= Other.Y;
        return *this;
    }

    FVector2 &operator*=(float Scalar) noexcept
    {
        X *= Scalar;
        Y *= Scalar;
        return *this;
    }

    FVector2 &operator/=(float Scalar) noexcept
    {
        assert(Scalar != 0.f);
        X /= Scalar;
        Y /= Scalar;
        return *this;
    }

    //======================================//
    //				  method				//
    //======================================//
  public:
    // 현재 벡터를 DirectX::XMVECTOR 형식으로 변환함
    DirectX::XMVECTOR ToXMVector(float Z = 0.f, float W = 0.f) const noexcept
    {
        return FDXMathBridge::LoadVector2(*this, Z, W);
    }

    // 허용 오차(Tolerance) 범위 내에서 두 벡터가 같은지 비교함
    bool Equals(const FVector2 &V, float Tolerance = 1.e-6f) const noexcept
    {
        return DirectX::XMVector2NearEqual(ToXMVector(), V.ToXMVector(),
                                           DirectX::XMVectorReplicate(Tolerance));
    }

    // 모든 성분이 정확히 0인지 확인함
    bool IsZero() const noexcept { return X == 0.f && Y == 0.f; }

    // 모든 성분이 허용 오차(Tolerance) 이하인지 확인함
    bool IsNearlyZero(float Tolerance = 1.e-6f) const noexcept
    {
        return DirectX::XMVector2NearEqual(ToXMVector(), DirectX::XMVectorZero(),
                                           DirectX::XMVectorReplicate(Tolerance));
    }

    // 벡터 길이의 제곱 값을 구함
    // 제곱근 연산이 없어서 Size()보다 빠름
    float SizeSquared() const noexcept
    {
        return DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(ToXMVector()));
    }

    // 벡터의 길이(크기)를 구함
    float Size() const noexcept
    {
        return DirectX::XMVectorGetX(DirectX::XMVector2Length(ToXMVector()));
    }

    // 현재 벡터를 정규화함
    // 길이가 너무 작으면 영벡터로 만들고 false를 반환함
    bool Normalize(float Tolerance = 1.e-8f) noexcept
    {
        const DirectX::XMVECTOR Vector = ToXMVector();
        const float SquareSum = DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(Vector));
        if (SquareSum > Tolerance)
        {
            FDXMathBridge::StoreVector2(*this, DirectX::XMVector2Normalize(Vector));
            return true;
        }

        X = 0.f;
        Y = 0.f;
        return false;
    }

    // 정규화된 벡터를 반환함
    // 길이가 너무 작으면 ZeroVector를 반환함
    FVector2 GetSafeNormal(float Tolerance = 1.e-8f) const noexcept
    {
        const DirectX::XMVECTOR Vector = ToXMVector();
        const float SquareSum = DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(Vector));

        if (SquareSum > Tolerance)
        {
            FVector2 Result;
            FDXMathBridge::StoreVector2(Result, DirectX::XMVector2Normalize(Vector));
            return Result;
        }

        return ZeroVector;
    }

  public:
    // 두 벡터의 내적(Dot Product)을 구함
    static float DotProduct(const FVector2 &A, const FVector2 &B) noexcept
    {
        return DirectX::XMVectorGetX(DirectX::XMVector2Dot(A.ToXMVector(), B.ToXMVector()));
    }

    // 두 벡터의 외적(Cross Product)을 구함
    static FVector3 CrossProduct(const FVector2 &A, const FVector2 &B) noexcept
    {
        FVector3 Result;
        FDXMathBridge::StoreVector(Result, DirectX::XMVector2Cross(A.ToXMVector(), B.ToXMVector()));
        return Result;
    }

    // 두 벡터 사이 거리의 제곱 값을 구함
    // 거리 비교만 필요할 때 Dist()보다 효율적임
    static float DistSquared(const FVector2 &A, const FVector2 &B) noexcept
    {
        const DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(A.ToXMVector(), B.ToXMVector());
        return DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(Delta));
    }

    // 두 벡터의 거리를 구함
    static float Dist(const FVector2 &A, const FVector2 &B) noexcept
    {
        const DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(A.ToXMVector(), B.ToXMVector());
        return DirectX::XMVectorGetX(DirectX::XMVector2Length(Delta));
    }
};

// ---- FDXMathBridge inline implementations for FVector2 ----
namespace FDXMathBridge
{
    inline VectorRegister LoadVector2(const FVector2 &V, float Z, float W) noexcept
    {
        return DirectX::XMVectorSet(V.X, V.Y, Z, W);
    }

    inline void StoreVector2(FVector2 &Out, ConstVectorRegister V) noexcept
    {
        DirectX::XMFLOAT2 Temp;
        DirectX::XMStoreFloat2(&Temp, V);
        Out.X = Temp.x;
        Out.Y = Temp.y;
    }
} // namespace FDXMathBridge
