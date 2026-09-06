#include "Asset/Manager/AssetCacheManager.h"
#include "Asset/Builder/AssetBuildReport.h"
#include "Asset/Serialization/CookedDataBinaryIO.h"
#include "Asset/Core/AssetNaming.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include <cctype>
#include <filesystem>

namespace Asset
{
    namespace
    {
        std::filesystem::path CombineIfRelative(const std::filesystem::path &Base,
                                                const std::filesystem::path &Relative)
        {
            if (Relative.empty())
            {
                return {};
            }

            return Relative.is_absolute() ? FPaths::Normalize(Relative)
                                          : FPaths::Combine(Base, Relative);
        }

        bool HasDataRootPrefix(const std::filesystem::path &InPath)
        {
            auto It = InPath.begin();
            if (It == InPath.end())
            {
                return false;
            }

            const std::filesystem::path FirstSegment = *It;
            return FirstSegment == "Data" || FirstSegment == "data";
        }

        std::filesystem::path StripDataRootPrefix(const std::filesystem::path &InPath)
        {
            if (!HasDataRootPrefix(InPath))
            {
                return InPath;
            }

            auto It = InPath.begin();
            ++It;

            std::filesystem::path Result;
            for (; It != InPath.end(); ++It)
            {
                Result /= *It;
            }

            return Result;
        }

        bool IsTextureBakedPath(const std::filesystem::path &InPath)
        {
            if (InPath.empty())
            {
                return false;
            }

            FString Extension = FPaths::Utf8FromPath(InPath.extension());
            for (char &Ch : Extension)
            {
                Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
            }

            FString BakedExtension = GetPrimaryBakedExtension(EAssetFileKind::Texture);
            for (char &Ch : BakedExtension)
            {
                Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
            }

            return !BakedExtension.empty() && Extension == BakedExtension;
        }

        std::filesystem::path ResolveVirtualAssetPath(const FString &Path)
        {
            if (Path.empty())
            {
                return {};
            }

            std::filesystem::path Candidate = FPaths::PathFromUtf8(Path);
            if (Candidate.empty())
            {
                return {};
            }

            if (Candidate.is_absolute())
            {
                return FPaths::Normalize(Candidate);
            }

            const FString Normalized = Path;
            if (Normalized.rfind("/Content/", 0) == 0)
            {
                return FPaths::Combine(FPaths::AppContentDir(),
                                       FPaths::PathFromUtf8(Normalized.substr(9)));
            }
            if (Normalized == "/Content")
            {
                return FPaths::AppContentDir();
            }
            if (Normalized.rfind("/Engine/", 0) == 0)
            {
                return FPaths::Combine(FPaths::EngineContentDir(),
                                       FPaths::PathFromUtf8(Normalized.substr(8)));
            }
            if (Normalized == "/Engine")
            {
                return FPaths::EngineContentDir();
            }
            if (Normalized.rfind("/Game/", 0) == 0)
            {
                return FPaths::Combine(FPaths::AppContentDir(),
                                       FPaths::PathFromUtf8(Normalized.substr(6)));
            }
            if (Normalized == "/Game")
            {
                return FPaths::AppContentDir();
            }

            const std::filesystem::path RelativeCandidate = StripDataRootPrefix(Candidate);

            const std::filesystem::path PathFromContent =
                CombineIfRelative(FPaths::AppContentDir(), Candidate);
            if (std::filesystem::exists(PathFromContent))
            {
                return PathFromContent;
            }

            const std::filesystem::path PathFromStrippedContent =
                CombineIfRelative(FPaths::AppContentDir(), RelativeCandidate);
            if (std::filesystem::exists(PathFromStrippedContent))
            {
                return PathFromStrippedContent;
            }

            const std::filesystem::path PathFromAppRoot =
                CombineIfRelative(FPaths::AppRoot(), Candidate);
            if (std::filesystem::exists(PathFromAppRoot))
            {
                return PathFromAppRoot;
            }

            return PathFromStrippedContent.empty() ? PathFromContent : PathFromStrippedContent;
        }
        const char *DescribeBuildResultSource(EAssetBuildResultSource Source)
        {
            switch (Source)
            {
            case EAssetBuildResultSource::CookedCache:
                return "cached cooked data";
            case EAssetBuildResultSource::BuiltFromCachedIntermediate:
                return "rebuilt cooked data from cached intermediate";
            case EAssetBuildResultSource::BuiltFromFreshIntermediate:
                return "rebuilt cooked data from newly loaded source";
            default:
                return "unknown";
            }
        }

        void LogBuildReport(const char *AssetType, const FString &Path,
                            const FAssetBuildReport &Report)
        {
            UE_LOG(AssetCache, ELogLevel::Debug,
                   "%s asset ready: %s (%s, cachedIntermediate=%d, cachedCooked=%d, "
                   "builtNewCooked=%d)",
                   AssetType, Path.c_str(), DescribeBuildResultSource(Report.ResultSource),
                   Report.bUsedCachedIntermediate ? 1 : 0, Report.bUsedCachedCooked ? 1 : 0,
                   Report.bBuiltNewCooked ? 1 : 0);
        }
    } // namespace

    FString FAssetCacheManager::StringFromPath(const std::filesystem::path &Path)
    {
        if (Path.empty())
        {
            return {};
        }

        return FPaths::Utf8FromPath(Path);
    }

    std::filesystem::path FAssetCacheManager::ResolveAssetPath(const FString &Path)
    {
        if (Path.empty())
        {
            return {};
        }

        std::filesystem::path Candidate = FPaths::PathFromUtf8(Path);
        if (Candidate.is_absolute())
        {
            return Candidate;
        }

        return ResolveVirtualAssetPath(Path);
    }

    FAssetCacheManager::FAssetCacheManager()
        : BuildCache(), TextureBuilder(BuildCache), MaterialBuilder(BuildCache),
          StaticMeshBuilder(BuildCache)
    {
    }

    std::shared_ptr<FTextureCookedData>
    FAssetCacheManager::BuildTexture(const FString &Path, const FTextureBuildSettings &Settings)
    {
        const std::filesystem::path AbsolutePath = ResolveAssetPath(Path);
        if (AbsolutePath.empty())
        {
            UE_LOG(AssetCache, ELogLevel::Error, "Texture asset path resolve failed: %s",
                   Path.c_str());
            return nullptr;
        }

        UE_LOG(AssetCache, ELogLevel::Debug, "Texture asset path resolved: %s -> %s", Path.c_str(),
               StringFromPath(AbsolutePath).c_str());

        return BuildTextureAbsolute(AbsolutePath, Settings);
    }

    std::shared_ptr<FMtlCookedData> FAssetCacheManager::BuildMaterial(const FString &Path)
    {
        FString LibraryPath = Path;
        FString MaterialName;
        FMaterialBuilder::SplitMaterialAssetPath(Path, LibraryPath, MaterialName);

        const std::filesystem::path AbsolutePath = ResolveAssetPath(LibraryPath);
        if (AbsolutePath.empty())
        {
            UE_LOG(AssetCache, ELogLevel::Error, "Material asset path resolve failed: %s",
                   Path.c_str());
            return nullptr;
        }

        const FString ResolvedPath =
            MaterialName.empty()
                ? StringFromPath(AbsolutePath)
                : FMaterialBuilder::MakeMaterialAssetPath(AbsolutePath, MaterialName);
        UE_LOG(AssetCache, ELogLevel::Debug, "Material asset path resolved: %s -> %s", Path.c_str(),
               ResolvedPath.c_str());

        return BuildMaterialAbsolute(AbsolutePath, MaterialName);
    }

    std::shared_ptr<FObjCookedData>
    FAssetCacheManager::BuildStaticMesh(const FString                  &Path,
                                        const FStaticMeshBuildSettings &Settings)
    {
        const std::filesystem::path AbsolutePath = ResolveAssetPath(Path);
        if (AbsolutePath.empty())
        {
            UE_LOG(AssetCache, ELogLevel::Error, "Static mesh asset path resolve failed: %s",
                   Path.c_str());
            return nullptr;
        }

        UE_LOG(AssetCache, ELogLevel::Debug, "Static mesh asset path resolved: %s -> %s",
               Path.c_str(), StringFromPath(AbsolutePath).c_str());

        return BuildStaticMeshAbsolute(AbsolutePath, Settings);
    }

    std::shared_ptr<FTextureCookedData>
    FAssetCacheManager::BuildTextureAbsolute(const std::filesystem::path &AbsolutePath,
                                             const FTextureBuildSettings &Settings)
    {
        std::shared_ptr<FTextureCookedData> Result;

        if (IsTextureBakedPath(AbsolutePath))
        {
            auto LoadedCooked = std::make_shared<FTextureCookedData>();
            if (Binary::LoadTexture(AbsolutePath, *LoadedCooked) && LoadedCooked->IsValid())
            {
                Result = std::move(LoadedCooked);
                UE_LOG(AssetCache, ELogLevel::Debug,
                       "Texture cooked data loaded directly from baked binary: %s",
                       StringFromPath(AbsolutePath).c_str());
            }
        }
        else
        {
            Result = TextureBuilder.Build(AbsolutePath, Settings);
        }

        if (Result)
        {
            UE_LOG(AssetCache, ELogLevel::Debug, "Texture cooked data ready: %s",
                   StringFromPath(AbsolutePath).c_str());
            if (!IsTextureBakedPath(AbsolutePath))
            {
                LogBuildReport("Texture", StringFromPath(AbsolutePath),
                               TextureBuilder.GetLastBuildReport());
            }
        }
        else
        {
            UE_LOG(AssetCache, ELogLevel::Error, "Texture asset load failed: %s",
                   StringFromPath(AbsolutePath).c_str());
        }

        return Result;
    }

    std::shared_ptr<FMtlCookedData>
    FAssetCacheManager::BuildMaterialAbsolute(const std::filesystem::path &AbsolutePath,
                                              const FString               &MaterialName)
    {
        std::shared_ptr<FMtlCookedData> Result =
            MaterialBuilder.BuildMaterial(AbsolutePath, MaterialName);

        const FString LogPath =
            MaterialName.empty()
                ? StringFromPath(AbsolutePath)
                : FMaterialBuilder::MakeMaterialAssetPath(AbsolutePath, MaterialName);
        if (Result)
        {
            UE_LOG(AssetCache, ELogLevel::Debug, "Material cooked data ready: %s", LogPath.c_str());
            LogBuildReport("Material", LogPath, MaterialBuilder.GetLastBuildReport());
        }
        else
        {
            UE_LOG(AssetCache, ELogLevel::Error, "Material asset load failed: %s", LogPath.c_str());
        }

        return Result;
    }

    std::shared_ptr<FObjCookedData>
    FAssetCacheManager::BuildStaticMeshAbsolute(const std::filesystem::path    &AbsolutePath,
                                                const FStaticMeshBuildSettings &Settings)
    {
        std::shared_ptr<FObjCookedData> Result = StaticMeshBuilder.Build(AbsolutePath, Settings);

        if (Result)
        {
            UE_LOG(AssetCache, ELogLevel::Debug, "Static mesh cooked data ready: %s",
                   StringFromPath(AbsolutePath).c_str());
            LogBuildReport("Static mesh", StringFromPath(AbsolutePath),
                           StaticMeshBuilder.GetLastBuildReport());
        }
        else
        {
            UE_LOG(AssetCache, ELogLevel::Error, "Static mesh asset load failed: %s",
                   StringFromPath(AbsolutePath).c_str());
        }

        return Result;
    }

    void FAssetCacheManager::ClearAll() { BuildCache.ClearAll(); }

} // namespace Asset
