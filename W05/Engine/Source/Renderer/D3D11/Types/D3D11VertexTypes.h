#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <cstddef>

struct FD3D11VertexP
{
    FVector3 Position;
};

struct FD3D11VertexPC
{
    FVector3 Position;
    FVector4 Color;
};

struct FD3D11VertexPT
{
    FVector3 Position;
    FVector2 UV;
};

static_assert(sizeof(FD3D11VertexPT) == 20, "D3D11 PT vertex must be 20 bytes");
static_assert(offsetof(FD3D11VertexPT, Position) == 0, "Position offset must be 0");
static_assert(offsetof(FD3D11VertexPT, UV) == 12, "UV offset must be 12");
