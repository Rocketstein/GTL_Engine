#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Vector4.h"
#include "Core/Platform/PlatformTypes.h"

struct alignas(16) FD3D11PerViewConstants
{
    FMatrix View;
    FMatrix Projection;
};

struct alignas(16) FD3D11PerObjectConstants
{
    FMatrix World;
    FMatrix WorldViewProjection;
    FVector4 SelectionTint; // rgb=tint multiplier, a=unused
};

struct alignas(256) FD3D11PerObjectConstantsAligned
{
    FMatrix  World;
    FMatrix  WorldViewProjection;
    FVector4 SelectionTint; // rgb=tint multiplier, a=unused
};

static_assert(sizeof(FD3D11PerObjectConstantsAligned) == 256,
              "Per-object constant payload must be 256-byte aligned for batched offset binding.");

struct alignas(16) FD3D11GridLineConstants
{
    FMatrix ViewProjection;
};

struct alignas(16) FD3D11GridQuadConstants
{
    FMatrix  ViewProjection;
    FMatrix  InverseViewProjection;
    FVector4 GridParams;      // x=spacing, y=majorLineEvery, z=minorThickness, w=majorThickness
    FVector4 EyePositionFade; // xyz=eye position, w=fade distance
    FVector4 RenderParams;    // x=mode(0=world quad, 1=screen-space infinite), y=unused
};
