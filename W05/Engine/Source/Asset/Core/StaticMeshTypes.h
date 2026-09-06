#pragma once

#include "Asset/Core/AssetCommonTypes.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include <cstddef>

namespace Asset
{
    struct FStaticMeshVertexPT
    {
        FVector3 Position;
        FVector2 TexCoord;
    };


    static_assert(sizeof(FVector3) == 12, "FVector3 must be 12 bytes");
    static_assert(sizeof(FVector2) == 8, "FVector2 must be 8 bytes");
    static_assert(sizeof(FStaticMeshVertexPT) == 20, "PT vertex must be 20 bytes");
    static_assert(offsetof(FStaticMeshVertexPT, Position) == 0,
                  "Position offset must be 0");
    static_assert(offsetof(FStaticMeshVertexPT, TexCoord) == 12,
                  "TexCoord offset must be 12");

    struct FStaticMeshSectionData
    {
        uint32 StartIndex = 0;
        uint32 IndexCount = 0;
        uint32 MaterialIndex = 0;
    };

} // namespace Asset
