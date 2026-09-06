#pragma once

#include "Asset/Serialization/CookedDataSerialization.h"
#include "Asset/Serialization/WindowsBinArchive.h"
#include "Asset/Source/SourceRecord.h"
#include <filesystem>

namespace Asset::Binary
{
    enum class ECookedBinaryAssetType : uint32
    {
        Unknown = 0,
        Texture,
        MaterialLibrary,
        StaticMesh,
    };

    struct FStoredSourceMetadata
    {
        uint64  FileSize = 0;
        uint64  LastWriteTimeTicks = 0;
        FString ContentHash;
        bool    bHasContentHash = false;
    };

    FString GetSourceExtension(ECookedBinaryAssetType AssetType);
    FString GetBinaryExtension(ECookedBinaryAssetType AssetType);
    bool TryGetAssetTypeFromSourcePath(const FString &SourcePath, ECookedBinaryAssetType &OutType);
    FString MakeBinaryPathFromSourcePath(const FString &SourcePath);

    bool IsStoredSourceMetadataCurrent(const FStoredSourceMetadata &Stored,
                                       const FSourceRecord         &CurrentSource);

    bool SaveTexture(const FTextureCookedData &Data, const FString &Path,
                     const FSourceRecord      *Source = nullptr);
    bool SaveTexture(const FTextureCookedData      &Data, const std::filesystem::path &Path,
                     const FSourceRecord           *Source = nullptr);
    bool LoadTexture(const FString &Path, FTextureCookedData &OutData);
    bool LoadTexture(const std::filesystem::path &Path, FTextureCookedData &OutData);
    bool LoadTextureIfCurrent(const FString &Path, const FSourceRecord &Source,
                              FTextureCookedData &OutData);
    bool LoadTextureIfCurrent(const std::filesystem::path &Path, const FSourceRecord &Source,
                              FTextureCookedData          &OutData);

    bool SaveMaterialLibrary(const FMtlCookedLibraryData &Data, const FString &Path,
                             const FSourceRecord         *Source = nullptr);
    bool SaveMaterialLibrary(const FMtlCookedLibraryData     &Data,
                             const std::filesystem::path     &Path,
                             const FSourceRecord             *Source = nullptr);
    bool LoadMaterialLibrary(const FString &Path, FMtlCookedLibraryData &OutData);
    bool LoadMaterialLibrary(const std::filesystem::path &Path, FMtlCookedLibraryData &OutData);
    bool LoadMaterialLibraryIfCurrent(const FString &Path, const FSourceRecord &Source,
                                      FMtlCookedLibraryData &OutData);
    bool LoadMaterialLibraryIfCurrent(const std::filesystem::path &Path,
                                      const FSourceRecord         &Source,
                                      FMtlCookedLibraryData       &OutData);

    bool SaveStaticMesh(const FObjCookedData &Data, const FString &Path,
                        const FSourceRecord  *Source = nullptr);
    bool SaveStaticMesh(const FObjCookedData       &Data, const std::filesystem::path &Path,
                        const FSourceRecord        *Source = nullptr);
    bool LoadStaticMesh(const FString &Path, FObjCookedData &OutData);
    bool LoadStaticMesh(const std::filesystem::path &Path, FObjCookedData &OutData);
    bool LoadStaticMeshIfCurrent(const FString &Path, const FSourceRecord &Source,
                                 FObjCookedData &OutData);
    bool LoadStaticMeshIfCurrent(const std::filesystem::path &Path, const FSourceRecord &Source,
                                 FObjCookedData              &OutData);

} // namespace Asset::Binary
