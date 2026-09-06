#pragma once

namespace Asset
{
    enum class EAssetBuildResultSource
    {
        None,
        CookedCache,
        BuiltFromCachedIntermediate,
        BuiltFromFreshIntermediate
    };

    struct FAssetBuildReport
    {
        EAssetBuildResultSource ResultSource = EAssetBuildResultSource::None;
        bool                    bUsedCachedIntermediate = false;
        bool                    bUsedCachedCooked = false;
        bool                    bBuiltNewCooked = false;

        void Reset() { *this = {}; }
    };
} // namespace Asset
