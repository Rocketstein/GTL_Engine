#pragma once

#include "Core/Containers/String.h"
#include "Core/Platform/PlatformTypes.h"

namespace Asset
{
    enum class EAssetFileKind : uint8
    {
        Unknown = 0,
        Scene,
        Texture,
        StaticMesh,
        MaterialLibrary,
    };

    bool           AssetPathEndsWith(const FString &Path, const char *Suffix);
    EAssetFileKind ClassifyAssetPath(const FString &Path);
    const char    *GetAssetFileKindLabel(EAssetFileKind Kind);

    bool HasSourceExtension(EAssetFileKind Kind, const FString &Path);
    bool HasBakedExtension(EAssetFileKind Kind, const FString &Path);

    FString GetPrimarySourceExtension(EAssetFileKind Kind);
    FString GetPrimaryBakedExtension(EAssetFileKind Kind);

    FString MakeBakedAssetPath(const FString &SourcePath);
} // namespace Asset
