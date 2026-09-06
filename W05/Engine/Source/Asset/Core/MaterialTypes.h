#pragma once

#include "Asset/Core/AssetCommonTypes.h"

namespace Asset
{

    enum class EMaterialTextureSlot : uint8
    {
        Diffuse = 0,
        Normal,
        Specular,
        Opacity,
        Emissive,
    };

} // namespace Asset
