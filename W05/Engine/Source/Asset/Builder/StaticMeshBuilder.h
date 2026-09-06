#pragma once

#include "Asset/Builder/AssetBuildReport.h"
#include "Asset/Cache/AssetBuildCache.h"
#include "Asset/Cache/BuildSettings.h"
#include "Asset/Cooked/ObjCookedData.h"
#include "Asset/Intermediate/IntermediateObjData.h"
#include <filesystem>
#include <memory>

namespace Asset
{

    class FStaticMeshBuilder
    {
      public:
        explicit FStaticMeshBuilder(FAssetBuildCache &InCache) : Cache(InCache) {}

        std::shared_ptr<FObjCookedData> Build(const std::filesystem::path    &Path,
                                              const FStaticMeshBuildSettings &Settings = {});
        std::shared_ptr<FObjCookedData> Build(const FWString                 &Path,
                                              const FStaticMeshBuildSettings &Settings = {})
        {
            return Build(std::filesystem::path(Path), Settings);
        }

        const FAssetBuildReport &GetLastBuildReport() const { return LastBuildReport; }

      private:
        std::shared_ptr<FIntermediateObjData> ParseObj(const FSourceRecord &Source);
        std::shared_ptr<FObjCookedData>       CookMesh(const FSourceRecord            &Source,
                                                       const FIntermediateObjData     &Intermediate,
                                                       const FStaticMeshBuildSettings &Settings);

      private:
        FAssetBuildCache &Cache;
        FAssetBuildReport LastBuildReport;
    };

} // namespace Asset
