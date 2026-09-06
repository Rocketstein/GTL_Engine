#pragma once

#include "Core/Platform/PlatformTypes.h"
#include "SIMD/DXMathBridge.h"
#include <cassert>
#include <cmath>
#include <cstdint>

struct FVector3
{
  public:
    union
    {
        struct
        {
            float X;
            float Y;
            float Z;
        };

        float XYZ[3];
    };

    /** A zero vector (0,0,0) */
    static const FVector3 ZeroVector;

    /** One vector (1,1,1) */
    static const FVector3 OneVector;

    /** Up vector (0,0,1) */
    static const FVector3 UpVector;

    /** Down vector (0,0,-1) */
    static const FVector3 DownVector;

    /** Forward vector (1,0,0) */
    static const FVector3 ForwardVector;

    /** Backward vector (-1,0,0) */
    static const FVector3 BackwardVector;

    /** Right vector (0,1,0) */
    static const FVector3 RightVector;

    /** Left vector (0,-1,0) */
    static const FVector3 LeftVector;

    /** Unit X axis vector (1,0,0) */
    static const FVector3 XAxisVector;

    /** Unit Y axis vector (0,1,0) */
    static const FVector3 YAxisVector;

    /** Unit Z axis vector (0,0,1) */
    static const FVector3 ZAxisVector;

    static inline FVector3 Zero() { return ZeroVector; }
    static inline FVector3 One() { return OneVector; }
    static inline FVector3 UnitX() { return XAxisVector; }
    static inline FVector3 UnitY() { return YAxisVector; }
    static inline FVector3 UnitZ() { return ZAxisVector; }

    //======================================//
    //				constructor				//
    //======================================//
  public:
    constexpr FVector3() noexcept : X(0.f), Y(0.f), Z(0.f) {}

    constexpr FVector3(const float InX, const float InY, const float InZ) noexcept
        : X(InX), Y(InY), Z(InZ)
    {
    }

    FVector3(const FVector3 &) noexcept = default;
    FVector3(FVector3 &&) noexcept = default;

    //======================================//
    //				operators				//
    //======================================//
  public:
    FVector3 &operator=(const FVector3 &) noexcept = default;
    FVector3 &operator=(FVector3 &&) noexcept = default;

    float &operator[](int32 Index) noexcept
    {
        assert(Index >= 0 && Index < 3);
        return XYZ[Index];
    }

    const float &operator[](int32 Index) const noexcept
    {
        assert(Index >= 0 && Index < 3);
        return XYZ[Index];
    }

    constexpr bool operator==(const FVector3 &Other) const noexcept
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z;
    }

    constexpr bool operator!=(const FVector3 &Other) const noexcept { return !(*this == Other); }

    constexpr FVector3 operator-() const noexcept { return {-X, -Y, -Z}; }

    constexpr FVector3 operator+(const FVector3 &Other) const noexcept
    {
        return {X + Other.X, Y + Other.Y, Z + Other.Z};
    }

    constexpr FVector3 operator-(const FVector3 &Other) const noexcept
    {
        return {X - Other.X, Y - Other.Y, Z - Other.Z};
    }

    constexpr FVector3 operator*(float Scalar) const noexcept
    {
        return {X * Scalar, Y * Scalar, Z * Scalar};
    }

    constexpr FVector3 operator/(float Scalar) const noexcept
    {
        assert(Scalar != 0.f);
        return {X / Scalar, Y / Scalar, Z / Scalar};
    }

    FVector3 &operator+=(const FVector3 &Other) noexcept
    {
        X += Other.X;
        Y += Other.Y;
        Z += Other.Z;
        return *this;
    }

    FVector3 &operator-=(const FVector3 &Other) noexcept
    {
        X -= Other.X;
        Y -= Other.Y;
        Z -= Other.Z;
        return *this;
    }

    FVector3 &operator*=(float Scalar) noexcept
    {
        X *= Scalar;
        Y *= Scalar;
        Z *= Scalar;
        return *this;
    }

    FVector3 &operator/=(float Scalar) noexcept
    {
        assert(Scalar != 0.f);
        X /= Scalar;
        Y /= Scalar;
        Z /= Scalar;
        return *this;
    }

    //======================================//
    //				  method				//
    //======================================//
  public:
    // 현재 벡터를 DirectX::XMVECTOR 형식으로 변환함
    // W 성분은 기본값 0.f를 사용하며 필요 시 지정할 수 있음
    DirectX::XMVECTOR ToXMVector(float W = 0.f) const noexcept
    {
        return FDXMathBridge::LoadVector(*this, W);
    }

    // 허용 오차(Tolerance) 범위 내에서 두 벡터가 같은지 비교함
    bool Equals(const FVector3 &V, float Tolerance = 1.e-6f) const noexcept
    {
        return DirectX::XMVector3NearEqual(ToXMVector(), V.ToXMVector(),
                                           DirectX::XMVectorReplicate(Tolerance));
    }

    // 모든 성분이 정확히 0인지 확인함
    bool IsZero() const noexcept { return X == 0.f && Y == 0.f && Z == 0.f; }

    // 모든 성분이 허용 오차(Tolerance) 이하인지 확인함
    bool IsNearlyZero(float Tolerance = 1.e-6f) const noexcept
    {
        return DirectX::XMVector3NearEqual(ToXMVector(), DirectX::XMVectorZero(),
                                           DirectX::XMVectorReplicate(Tolerance));
    }

    // 벡터 길이의 제곱 값을 구함
    // 제곱근 연산이 없어서 Size()보다 빠름
    float SizeSquared() const noexcept
    {
        return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(ToXMVector()));
    }

    // 벡터의 길이(크기)를 구함
    float Size() const noexcept
    {
        return DirectX::XMVectorGetX(DirectX::XMVector3Length(ToXMVector()));
    }

    // XY 평면에서의 벡터 길이 제곱 값을 구함
    float SizeSquared2D() const noexcept
    {
        return DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(ToXMVector()));
    }

    // XY 평면에서의 벡터 길이(크기)를 구함
    float Size2D() const noexcept
    {
        return DirectX::XMVectorGetX(DirectX::XMVector2Length(ToXMVector()));
    }

    // 현재 벡터를 정규화함
    // 길이가 너무 작으면 영벡터로 만들고 false를 반환함
    bool Normalize(float Tolerance = 1.e-8f) noexcept
    {
        const DirectX::XMVECTOR Vector = ToXMVector();
        const float SquareSum = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Vector));
        if (SquareSum > Tolerance)
        {
            FDXMathBridge::StoreVector(*this, DirectX::XMVector3Normalize(Vector));
            return true;
        }

        X = 0.f;
        Y = 0.f;
        Z = 0.f;
        return false;
    }

    // 정규화된 벡터를 반환함
    // 길이가 너무 작으면 ZeroVector를 반환함
    FVector3 GetSafeNormal(float Tolerance = 1.e-8f) const noexcept
    {
        const DirectX::XMVECTOR Vector = ToXMVector();
        const float SquareSum = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Vector));
        if (SquareSum > Tolerance)
        {
            FVector3 Result;
            FDXMathBridge::StoreVector(Result, DirectX::XMVector3Normalize(Vector));
            return Result;
        }

        return ZeroVector;
    }

    // XY 평면 기준으로 정규화된 벡터를 반환함
    // Z는 0으로 설정되며 길이가 너무 작으면 ZeroVector를 반환함
    FVector3 GetSafeNormal2D(float Tolerance = 1.e-8f) const noexcept
    {
        const DirectX::XMVECTOR Vector = ToXMVector();
        const float SquareSum = DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(Vector));
        if (SquareSum > Tolerance)
        {
            const DirectX::XMVECTOR Normalized = DirectX::XMVector2Normalize(Vector);
            return FVector3(DirectX::XMVectorGetX(Normalized), DirectX::XMVectorGetY(Normalized),
                            0.0f);
        }

        return ZeroVector;
    }

  public:
    // 두 벡터의 내적(Dot Product)을 구함
    static float DotProduct(const FVector3 &A, const FVector3 &B) noexcept
    {
        return DirectX::XMVectorGetX(DirectX::XMVector3Dot(A.ToXMVector(), B.ToXMVector()));
    }

    // 두 벡터의 외적(Cross Product)을 구함
    static FVector3 CrossProduct(const FVector3 &A, const FVector3 &B) noexcept
    {
        FVector3 Result;
        FDXMathBridge::StoreVector(Result, DirectX::XMVector3Cross(A.ToXMVector(), B.ToXMVector()));
        return Result;
    }

    // 두 벡터 사이 거리의 제곱 값을 구함
    // 거리 비교만 필요할 때 Dist()보다 효율적임
    static float DistSquared(const FVector3 &A, const FVector3 &B) noexcept
    {
        const DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(A.ToXMVector(), B.ToXMVector());
        return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Delta));
    }

    // 두 벡터의 거리를 구함
    static float Dist(const FVector3 &A, const FVector3 &B) noexcept
    {
        const DirectX::XMVECTOR Delta = DirectX::XMVectorSubtract(A.ToXMVector(), B.ToXMVector());
        return DirectX::XMVectorGetX(DirectX::XMVector3Length(Delta));
    }

    static FVector3 Lerp(const FVector3 &A, const FVector3 &B, float Alpha)
    {
        return A + (B - A) * Alpha;
    }
};

// ---- FDXMathBridge inline implementations for FVector3 ----
namespace FDXMathBridge
{
    inline VectorRegister LoadVector(const FVector3 &V, float W) noexcept
    {
        return DirectX::XMVectorSet(V.X, V.Y, V.Z, W);
    }

    inline void StoreVector(FVector3 &Out, ConstVectorRegister V) noexcept
    {
        DirectX::XMFLOAT3 Temp;
        DirectX::XMStoreFloat3(&Temp, V);
        Out.X = Temp.x;
        Out.Y = Temp.y;
        Out.Z = Temp.z;
    }
} // namespace FDXMathBridge
