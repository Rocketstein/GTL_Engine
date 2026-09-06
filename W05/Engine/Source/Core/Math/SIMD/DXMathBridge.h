#pragma once

#include <DirectXMath.h>

struct FVector2;
struct FVector3;
struct FVector4;
struct FQuat;
struct FMatrix;

namespace FDXMathBridge
{
    using VectorRegister = DirectX::XMVECTOR;
    using MatrixRegister = DirectX::XMMATRIX;
    using ConstVectorRegister = DirectX::FXMVECTOR;
    using ConstMatrixRegister = DirectX::CXMMATRIX;

    inline VectorRegister LoadVector2(const FVector2 &V, float Z = 0.0f, float W = 0.0f) noexcept;
    inline void           StoreVector2(FVector2 &Out, ConstVectorRegister V) noexcept;

    inline VectorRegister LoadVector(const FVector3 &V, float W = 0.0f) noexcept;
    inline void           StoreVector(FVector3 &Out, ConstVectorRegister V) noexcept;

    inline VectorRegister LoadVector4(const FVector4 &V) noexcept;
    inline void           StoreVector4(FVector4 &Out, ConstVectorRegister V) noexcept;

    inline VectorRegister LoadQuat(const FQuat &Q) noexcept;
    inline void           StoreQuat(FQuat &Out, ConstVectorRegister V) noexcept;

    inline MatrixRegister LoadMatrix(const FMatrix &M) noexcept;
    inline void           StoreMatrix(FMatrix &Out, ConstMatrixRegister M) noexcept;
} // namespace FDXMathBridge
