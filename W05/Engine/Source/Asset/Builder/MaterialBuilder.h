#pragma once

#include "Asset/Builder/AssetBuildReport.h"
#include "Asset/Cache/AssetBuildCache.h"
#include "Asset/Cooked/MtlCookedData.h"
#include "Asset/Intermediate/IntermediateMtlData.h"
#include <filesystem>
#include <memory>

namespace Asset
{

    class FMaterialBuilder
    {
      public:
        explicit FMaterialBuilder(FAssetBuildCache &InCache) : Cache(InCache) {}

        std::shared_ptr<FMtlCookedLibraryData> BuildLibrary(const std::filesystem::path &Path);
        std::shared_ptr<FMtlCookedLibraryData> BuildLibrary(const FWString &Path)
        {
            return BuildLibrary(std::filesystem::path(Path));
        }

        std::shared_ptr<FMtlCookedData> BuildMaterial(const std::filesystem::path &Path,
                                                      const FString &MaterialName = {});
        std::shared_ptr<FMtlCookedData> BuildMaterial(const FWString &Path,
                                                      const FString  &MaterialName = {})
        {
            return BuildMaterial(std::filesystem::path(Path), MaterialName);
        }

        static FString MakeMaterialAssetPath(const std::filesystem::path &LibraryPath,
                                             const FString               &MaterialName);
        static bool    SplitMaterialAssetPath(const FString &InAssetPath, FString &OutLibraryPath,
                                              FString &OutMaterialName);

        const FAssetBuildReport &GetLastBuildReport() const { return LastBuildReport; }

      private:
        std::shared_ptr<FIntermediateMtlLibraryData>
        ParseMaterialLibrary(const FSourceRecord &Source);
        std::shared_ptr<FMtlCookedLibraryData>
        CookMaterialLibrary(const FSourceRecord               &Source,
                            const FIntermediateMtlLibraryData &Intermediate);

        static bool    ReadAllText(const std::filesystem::path &Path, FString &OutText);
        static FString Trim(const FString &Value);
        static EMaterialTextureSlot ResolveTextureSlot(const FString &SlotName);
        static FWString             ResolveRelativePath(const std::filesystem::path &BasePath,
                                                        const FString               &RelativePath);

      private:
        FAssetBuildCache &Cache;
        FAssetBuildReport LastBuildReport;
    };

} // namespace Asset
