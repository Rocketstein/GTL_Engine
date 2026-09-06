#include "Asset/Core/AssetNaming.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace Asset
{
    namespace
    {
        struct FAssetNamingPolicy
        {
            EAssetFileKind              Kind = EAssetFileKind::Unknown;
            const char                 *Label = "Unknown";
            std::array<const char *, 4> SourceSuffixes{};
            size_t                      SourceSuffixCount = 0;
            std::array<const char *, 2> BakedSuffixes{};
            size_t                      BakedSuffixCount = 0;
        };

        constexpr FAssetNamingPolicy Policies[] = {
            {EAssetFileKind::Scene,
             "Scene",
             {".scene", nullptr, nullptr, nullptr},
             1,
             {".scene", nullptr},
             1},
            {EAssetFileKind::Texture,
             "Texture",
             {".png", ".jpg", ".jpeg", ".bmp"},
             4,
             {".texture", nullptr},
             1},
            {EAssetFileKind::StaticMesh,
             "Static Mesh",
             {".obj", nullptr, nullptr, nullptr},
             1,
             {".obj.bin", nullptr},
             1},
            {EAssetFileKind::MaterialLibrary,
             "Material Library",
             {".mtl", nullptr, nullptr, nullptr},
             1,
             {".mtl.bin", nullptr},
             1},
        };

        FString ToLower(FString Value)
        {
            std::transform(Value.begin(), Value.end(), Value.begin(),
                           [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
            return Value;
        }

        const FAssetNamingPolicy *FindPolicy(EAssetFileKind Kind)
        {
            for (const FAssetNamingPolicy &Policy : Policies)
            {
                if (Policy.Kind == Kind)
                {
                    return &Policy;
                }
            }

            return nullptr;
        }

        bool HasAnySuffix(const FString &Path, const std::array<const char *, 4> &Suffixes,
                          size_t SuffixCount)
        {
            for (size_t Index = 0; Index < SuffixCount; ++Index)
            {
                if (AssetPathEndsWith(Path, Suffixes[Index]))
                {
                    return true;
                }
            }

            return false;
        }

        bool HasAnySuffix(const FString &Path, const std::array<const char *, 2> &Suffixes,
                          size_t SuffixCount)
        {
            for (size_t Index = 0; Index < SuffixCount; ++Index)
            {
                if (AssetPathEndsWith(Path, Suffixes[Index]))
                {
                    return true;
                }
            }

            return false;
        }

        FString ReplaceSuffix(const FString &Path, const char *SourceSuffix,
                              const char *TargetSuffix)
        {
            if (!AssetPathEndsWith(Path, SourceSuffix))
            {
                return {};
            }

            return Path.substr(0, Path.size() - std::strlen(SourceSuffix)) + TargetSuffix;
        }
    } // namespace

    bool AssetPathEndsWith(const FString &Path, const char *Suffix)
    {
        if (Suffix == nullptr || *Suffix == '\0')
        {
            return false;
        }

        const FString LowerPath = ToLower(Path);
        const FString LowerSuffix = ToLower(FString(Suffix));
        return LowerPath.size() >= LowerSuffix.size() &&
               LowerPath.compare(LowerPath.size() - LowerSuffix.size(), LowerSuffix.size(),
                                 LowerSuffix) == 0;
    }

    EAssetFileKind ClassifyAssetPath(const FString &Path)
    {
        for (const FAssetNamingPolicy &Policy : Policies)
        {
            if (HasAnySuffix(Path, Policy.BakedSuffixes, Policy.BakedSuffixCount) ||
                HasAnySuffix(Path, Policy.SourceSuffixes, Policy.SourceSuffixCount))
            {
                return Policy.Kind;
            }
        }

        return EAssetFileKind::Unknown;
    }

    const char *GetAssetFileKindLabel(EAssetFileKind Kind)
    {
        if (const FAssetNamingPolicy *Policy = FindPolicy(Kind))
        {
            return Policy->Label;
        }

        return "Unknown";
    }

    bool HasSourceExtension(EAssetFileKind Kind, const FString &Path)
    {
        const FAssetNamingPolicy *Policy = FindPolicy(Kind);
        return Policy != nullptr &&
               HasAnySuffix(Path, Policy->SourceSuffixes, Policy->SourceSuffixCount);
    }

    bool HasBakedExtension(EAssetFileKind Kind, const FString &Path)
    {
        const FAssetNamingPolicy *Policy = FindPolicy(Kind);
        return Policy != nullptr &&
               HasAnySuffix(Path, Policy->BakedSuffixes, Policy->BakedSuffixCount);
    }

    FString GetPrimarySourceExtension(EAssetFileKind Kind)
    {
        if (const FAssetNamingPolicy *Policy = FindPolicy(Kind);
            Policy != nullptr && Policy->SourceSuffixCount > 0)
        {
            return Policy->SourceSuffixes[0];
        }

        return {};
    }

    FString GetPrimaryBakedExtension(EAssetFileKind Kind)
    {
        if (const FAssetNamingPolicy *Policy = FindPolicy(Kind);
            Policy != nullptr && Policy->BakedSuffixCount > 0)
        {
            return Policy->BakedSuffixes[0];
        }

        return {};
    }

    FString MakeBakedAssetPath(const FString &SourcePath)
    {
        const EAssetFileKind      Kind = ClassifyAssetPath(SourcePath);
        const FAssetNamingPolicy *Policy = FindPolicy(Kind);
        if (Policy == nullptr || Policy->SourceSuffixCount == 0 || Policy->BakedSuffixCount == 0)
        {
            return {};
        }

        for (size_t Index = 0; Index < Policy->SourceSuffixCount; ++Index)
        {
            if (FString Converted = ReplaceSuffix(SourcePath, Policy->SourceSuffixes[Index],
                                                  Policy->BakedSuffixes[0]);
                !Converted.empty())
            {
                return Converted;
            }
        }

        return {};
    }
} // namespace Asset
