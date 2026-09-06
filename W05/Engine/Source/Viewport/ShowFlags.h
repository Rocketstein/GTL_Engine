#pragma once

#include "Core/Containers/Array.h"
#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Platform/PlatformTypes.h"
#include "Renderer/D3D11/Resources/D3D11Buffer.h"
#include <memory>

class UMaterial;

enum class EShowFlags : uint32
{
    SF_None = 0,
    SF_Primitives = 1u << 0,
    SF_Lines = 1u << 1,
    SF_Grid = 1u << 2,
    SF_WorldAxis = 1u << 3,
    SF_Gizmo = 1u << 4,
    SF_SelectedAABB = 1u << 5,
    SF_All = ~0u,
};

inline bool IsFlagSet(EShowFlags Flags, EShowFlags Flag)
{
    return (static_cast<uint32>(Flags) & static_cast<uint32>(Flag)) != 0;
}
