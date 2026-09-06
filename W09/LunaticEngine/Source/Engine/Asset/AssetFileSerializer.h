#pragma once

#include "Core/CoreTypes.h"
#include <filesystem>

class UAssetData;

namespace FAssetFileSerializer
{
    static constexpr uint32 AssetMagic = 0x5453414A; // 'JAST' little-endian: Jungle Asset
    static constexpr uint32 AssetVersion = 2;

    bool        SaveAssetToFile(const std::filesystem::path &FilePath, UAssetData *Asset, FString *OutError = nullptr);
    UAssetData *LoadAssetFromFile(const std::filesystem::path &FilePath, FString *OutError = nullptr);
    uint32      GetCurrentAssetSerializationVersion();
} // namespace FAssetFileSerializer
