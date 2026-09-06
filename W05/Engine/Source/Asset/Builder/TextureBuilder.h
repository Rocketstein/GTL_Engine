#pragma once

#include "Asset/Builder/AssetBuildReport.h"
#include "Asset/Cache/AssetBuildCache.h"
#include "Asset/Cache/BuildSettings.h"
#include "Asset/Cooked/TextureCookedData.h"
#include "Asset/Intermediate/IntermediateTextureData.h"
#include <filesystem>
#include <memory>

namespace Asset
{
    class FTextureBuilder
    {
      public:
        explicit FTextureBuilder(FAssetBuildCache &InCache);

        std::shared_ptr<FTextureCookedData> Build(const std::filesystem::path &Path,
                                                  const FTextureBuildSettings &Settings = {});
        std::shared_ptr<FTextureCookedData> Build(const FWString              &Path,
                                                  const FTextureBuildSettings &Settings = {})
        {
            return Build(std::filesystem::path(Path), Settings);
        }

        const FAssetBuildReport &GetLastBuildReport() const { return LastBuildReport; }

      private:
        bool DecodeTexture(const FSourceRecord &Source, FIntermediateTextureData &OutData) const;

        std::shared_ptr<FTextureCookedData>
        CookTexture(const FSourceRecord &Source, const FIntermediateTextureData &Intermediate,
                    const FTextureBuildSettings &Settings) const;

      private:
        FAssetBuildCache &Cache;
        FAssetBuildReport LastBuildReport;
    };
} // namespace Asset
