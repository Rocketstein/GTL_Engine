#pragma once

#include "Asset/Cache/AssetKey.h"
#include "Asset/Cache/AssetTags.h"
#include "Asset/Cooked/MtlCookedData.h"
#include "Asset/Cooked/ObjCookedData.h"
#include "Asset/Cooked/TextureCookedData.h"
#include "Asset/Intermediate/IntermediateMtlData.h"
#include "Asset/Intermediate/IntermediateObjData.h"
#include "Asset/Intermediate/IntermediateTextureData.h"

namespace Asset
{

    template <typename TTag> struct TAssetCacheTraits;

    template <> struct TAssetCacheTraits<FTextureAssetTag>
    {
        using SourceKey = FTextureSourceKey;
        using IntermediateKey = FTextureIntermediateKey;
        using CookedKey = FTextureCookedKey;
        using IntermediateType = FIntermediateTextureData;
        using CookedType = FTextureCookedData;
    };

    template <> struct TAssetCacheTraits<FMaterialAssetTag>
    {
        using SourceKey = FMaterialSourceKey;
        using IntermediateKey = FMaterialIntermediateKey;
        using CookedKey = FMaterialCookedKey;
        using IntermediateType = FIntermediateMtlLibraryData;
        using CookedType = FMtlCookedLibraryData;
    };

    template <> struct TAssetCacheTraits<FStaticMeshAssetTag>
    {
        using SourceKey = FStaticMeshSourceKey;
        using IntermediateKey = FStaticMeshIntermediateKey;
        using CookedKey = FStaticMeshCookedKey;
        using IntermediateType = FIntermediateObjData;
        using CookedType = FObjCookedData;
    };

    template <typename TTag> using TSourceKeyOf = typename TAssetCacheTraits<TTag>::SourceKey;
    template <typename TTag>
    using TIntermediateKeyOf = typename TAssetCacheTraits<TTag>::IntermediateKey;
    template <typename TTag> using TCookedKeyOf = typename TAssetCacheTraits<TTag>::CookedKey;
    template <typename TTag>
    using TIntermediateTypeOf = typename TAssetCacheTraits<TTag>::IntermediateType;
    template <typename TTag> using TCookedTypeOf = typename TAssetCacheTraits<TTag>::CookedType;

} // namespace Asset
