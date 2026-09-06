#pragma once

#include "Asset/Core/AssetCommonTypes.h"
#include "Core/Containers/Array.h"
#include "Core/Platform/PlatformTypes.h"
#include <filesystem>

namespace Asset
{
    struct FTextureMipLevel
    {
        uint32        Width = 0;
        uint32        Height = 0;
        TArray<uint8> Pixels;

        bool IsValid() const { return Width > 0 && Height > 0 && !Pixels.empty(); }
    };

    struct FTextureCookedData
    {
        std::filesystem::path SourcePath;
        uint32                Width = 0;
        uint32                Height = 0;
        uint32                Channels = 0;
        bool                  bSRGB = true;
        TArray<uint8>         Pixels;
        TArray<FTextureMipLevel> MipChain;

        bool IsValid() const { return Width > 0 && Height > 0 && !Pixels.empty(); }
        uint32 GetMipCount() const { return static_cast<uint32>(MipChain.size()); }

        void Reset()
        {
            SourcePath.clear();
            Width = 0;
            Height = 0;
            Channels = 0;
            bSRGB = true;
            Pixels.clear();
            MipChain.clear();
        }
    };
} // namespace Asset
