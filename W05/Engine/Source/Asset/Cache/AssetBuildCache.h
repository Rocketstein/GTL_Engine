#pragma once

#include "Asset/Cache/AssetCache.h"
#include "Asset/Cache/AssetCacheTraits.h"
#include "Asset/Source/SourceCache.h"
#include <filesystem>

namespace Asset
{
    class FAssetBuildCache
    {
      public:
        template <typename TTag>
        const FSourceRecord *GetSource(TTag Tag, const std::filesystem::path &Path)
        {
            using TSourceKey = TSourceKeyOf<TTag>;

            auto                       &CacheRef = GetSourceCacheImpl(Tag);
            const std::filesystem::path NormalizedPath = SourceCacheDetail::NormalizePath(Path);
            if (NormalizedPath.empty())
            {
                return nullptr;
            }

            const TSourceKey     SourceKey = TSourceKey::FromPath(NormalizedPath);
            const FSourceRecord *Source = CacheRef.GetOrLoad(SourceKey);
            if (Source == nullptr)
            {
                return nullptr;
            }
            if (!CacheRef.EnsureContentHashLoaded(SourceKey))
            {
                return nullptr;
            }
            return CacheRef.Find(SourceKey);
        }

        template <typename TTag> const FSourceRecord *GetSource(TTag Tag, const FWString &Path)
        {
            return GetSource(Tag, std::filesystem::path(Path));
        }

        template <typename TTag> void InvalidateSource(TTag Tag, const std::filesystem::path &Path)
        {
            using TSourceKey = TSourceKeyOf<TTag>;
            const std::filesystem::path NormalizedPath = SourceCacheDetail::NormalizePath(Path);
            if (NormalizedPath.empty())
            {
                return;
            }
            GetSourceCacheImpl(Tag).Invalidate(TSourceKey::FromPath(NormalizedPath));
        }

        template <typename TTag> void InvalidateSource(TTag Tag, const FWString &Path)
        {
            InvalidateSource(Tag, std::filesystem::path(Path));
        }

        void ClearAll()
        {
            TextureSourceCache.Clear();
            MaterialSourceCache.Clear();
            StaticMeshSourceCache.Clear();

            IntermediateTextureCache.Clear();
            IntermediateMaterialCache.Clear();
            IntermediateStaticMeshCache.Clear();

            TextureCookedCache.Clear();
            MaterialCookedCache.Clear();
            StaticMeshCookedCache.Clear();
        }

        template <typename TTag>
        TIntermediateCache<TIntermediateKeyOf<TTag>, TIntermediateTypeOf<TTag>> &
        GetIntermediateCache(TTag Tag)
        {
            return GetIntermediateCacheImpl(Tag);
        }

        template <typename TTag>
        TCookedCache<TCookedKeyOf<TTag>, TCookedTypeOf<TTag>> &GetCookedCache(TTag Tag)
        {
            return GetCookedCacheImpl(Tag);
        }

      private:
        TSourceCache<FTextureSourceKey> &GetSourceCacheImpl(FTextureAssetTag)
        {
            return TextureSourceCache;
        }
        TSourceCache<FMaterialSourceKey> &GetSourceCacheImpl(FMaterialAssetTag)
        {
            return MaterialSourceCache;
        }
        TSourceCache<FStaticMeshSourceKey> &GetSourceCacheImpl(FStaticMeshAssetTag)
        {
            return StaticMeshSourceCache;
        }

        TIntermediateCache<FTextureIntermediateKey, FIntermediateTextureData> &
        GetIntermediateCacheImpl(FTextureAssetTag)
        {
            return IntermediateTextureCache;
        }
        TIntermediateCache<FMaterialIntermediateKey, FIntermediateMtlLibraryData> &
        GetIntermediateCacheImpl(FMaterialAssetTag)
        {
            return IntermediateMaterialCache;
        }
        TIntermediateCache<FStaticMeshIntermediateKey, FIntermediateObjData> &
        GetIntermediateCacheImpl(FStaticMeshAssetTag)
        {
            return IntermediateStaticMeshCache;
        }

        TCookedCache<FTextureCookedKey, FTextureCookedData> &GetCookedCacheImpl(FTextureAssetTag)
        {
            return TextureCookedCache;
        }
        TCookedCache<FMaterialCookedKey, FMtlCookedLibraryData> &
        GetCookedCacheImpl(FMaterialAssetTag)
        {
            return MaterialCookedCache;
        }
        TCookedCache<FStaticMeshCookedKey, FObjCookedData> &GetCookedCacheImpl(FStaticMeshAssetTag)
        {
            return StaticMeshCookedCache;
        }

      private:
        TSourceCache<FTextureSourceKey>    TextureSourceCache;
        TSourceCache<FMaterialSourceKey>   MaterialSourceCache;
        TSourceCache<FStaticMeshSourceKey> StaticMeshSourceCache;

        TIntermediateCache<FTextureIntermediateKey, FIntermediateTextureData>
            IntermediateTextureCache;
        TIntermediateCache<FMaterialIntermediateKey, FIntermediateMtlLibraryData>
            IntermediateMaterialCache;
        TIntermediateCache<FStaticMeshIntermediateKey, FIntermediateObjData>
            IntermediateStaticMeshCache;

        TCookedCache<FTextureCookedKey, FTextureCookedData>     TextureCookedCache;
        TCookedCache<FMaterialCookedKey, FMtlCookedLibraryData> MaterialCookedCache;
        TCookedCache<FStaticMeshCookedKey, FObjCookedData>      StaticMeshCookedCache;
    };

} // namespace Asset
