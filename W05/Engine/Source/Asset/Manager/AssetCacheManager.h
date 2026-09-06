#pragma once

#include "Asset/Builder/MaterialBuilder.h"
#include "Asset/Builder/StaticMeshBuilder.h"
#include "Asset/Builder/TextureBuilder.h"
#include "Asset/Cache/AssetBuildCache.h"
#include "Asset/Cache/BuildSettings.h"
#include <filesystem>
#include <memory>

namespace Asset
{

    class FAssetCacheManager
    {
      public:
        FAssetCacheManager();

        std::shared_ptr<FTextureCookedData>
        BuildTexture(const FString &Path, const FTextureBuildSettings &Settings = {});
        std::shared_ptr<FMtlCookedData> BuildMaterial(const FString &Path);
        std::shared_ptr<FObjCookedData>
        BuildStaticMesh(const FString &Path, const FStaticMeshBuildSettings &Settings = {});

        template <typename TTag> const FSourceRecord *GetSource(TTag Tag, const FString &Path)
        {
            const std::filesystem::path AbsolutePath = ResolveAssetPath(Path);
            if (AbsolutePath.empty())
            {
                return nullptr;
            }
            return BuildCache.GetSource(Tag, AbsolutePath);
        }

        template <typename TTag> void InvalidateSource(TTag Tag, const FString &Path)
        {
            const std::filesystem::path AbsolutePath = ResolveAssetPath(Path);
            if (AbsolutePath.empty())
            {
                return;
            }
            BuildCache.InvalidateSource(Tag, AbsolutePath);
        }

        void ClearAll();

        FAssetBuildCache       &GetBuildCache() { return BuildCache; }
        const FAssetBuildCache &GetBuildCache() const { return BuildCache; }

      private:
        std::shared_ptr<FTextureCookedData>
        BuildTextureAbsolute(const std::filesystem::path &AbsolutePath,
                             const FTextureBuildSettings &Settings = {});
        std::shared_ptr<FMtlCookedData>
        BuildMaterialAbsolute(const std::filesystem::path &AbsolutePath,
                              const FString               &MaterialName = {});
        std::shared_ptr<FObjCookedData>
        BuildStaticMeshAbsolute(const std::filesystem::path    &AbsolutePath,
                                const FStaticMeshBuildSettings &Settings = {});

        static std::filesystem::path ResolveAssetPath(const FString &Path);
        static FString               StringFromPath(const std::filesystem::path &Path);

      private:
        FAssetBuildCache   BuildCache;
        FTextureBuilder    TextureBuilder;
        FMaterialBuilder   MaterialBuilder;
        FStaticMeshBuilder StaticMeshBuilder;
    };

} // namespace Asset
