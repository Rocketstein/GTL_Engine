#pragma once

#include "Core/Containers/String.h"
#include <sstream>


namespace Asset
{
    struct FTextureBuildSettings
    {
        bool bSRGB = true;
        bool bGenerateMips = true;

        FString ToKeyString() const
        {
            std::ostringstream Oss;
            Oss << "SRGB=" << (bSRGB ? 1 : 0) << ";Mips=" << (bGenerateMips ? 1 : 0);
            return Oss.str();
        }
    };

    struct FStaticMeshBuildSettings
    {
        bool bFlipV = true;

        FString ToKeyString() const
        {
            std::ostringstream Oss;
            Oss << "FlipV=" << (bFlipV ? 1 : 0);
            return Oss.str();
        }
    };

} // namespace Asset
