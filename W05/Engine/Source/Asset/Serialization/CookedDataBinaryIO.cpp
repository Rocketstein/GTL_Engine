#include "Asset/Serialization/CookedDataBinaryIO.h"
#include "Asset/Core/AssetNaming.h"
#include "Core/Misc/Paths.h"
#include <filesystem>
#include <fstream>

namespace Asset::Binary
{
    namespace
    {
        constexpr uint32 GBinaryMagic = 0x4E494241; // 'ABIN'
        constexpr uint32 GLegacyBinaryVersion = 1;
        constexpr uint32 GCurrentBinaryVersion = 2;

        struct FBinaryHeader
        {
            uint32 Magic = GBinaryMagic;
            uint32 Version = GCurrentBinaryVersion;
            uint32 AssetType = 0;
        };

        struct FBinarySourceMetadata
        {
            uint64  FileSize = 0;
            uint64  LastWriteTimeTicks = 0;
            FString ContentHash;
            bool    bHasContentHash = false;
        };

        FBinarySourceMetadata MakeStoredSourceMetadata(const FSourceRecord &Source)
        {
            FBinarySourceMetadata Metadata;
            Metadata.FileSize = Source.FileSize;
            Metadata.LastWriteTimeTicks = Source.LastWriteTimeTicks;
            Metadata.ContentHash = Source.ContentHash;
            Metadata.bHasContentHash = Source.bHasContentHash;
            return Metadata;
        }

        FStoredSourceMetadata ToPublicMetadata(const FBinarySourceMetadata &Metadata)
        {
            FStoredSourceMetadata Result;
            Result.FileSize = Metadata.FileSize;
            Result.LastWriteTimeTicks = Metadata.LastWriteTimeTicks;
            Result.ContentHash = Metadata.ContentHash;
            Result.bHasContentHash = Metadata.bHasContentHash;
            return Result;
        }

        bool WriteHeader(FArchive &Ar, ECookedBinaryAssetType AssetType,
                         const FSourceRecord *Source)
        {
            FBinaryHeader Header;
            Header.AssetType = static_cast<uint32>(AssetType);
            Ar << Header.Magic;
            Ar << Header.Version;
            Ar << Header.AssetType;
            if (!Ar.IsOk())
            {
                return false;
            }

            FBinarySourceMetadata Metadata;
            if (Source != nullptr)
            {
                Metadata = MakeStoredSourceMetadata(*Source);
            }

            Ar << Metadata.FileSize;
            Ar << Metadata.LastWriteTimeTicks;
            Ar << Metadata.ContentHash;
            Ar << Metadata.bHasContentHash;
            return Ar.IsOk();
        }

        bool ReadHeader(FArchive                 &Ar,
                        ECookedBinaryAssetType    ExpectedAssetType,
                        uint32                   &OutVersion,
                        FBinarySourceMetadata    &OutMetadata)
        {
            FBinaryHeader Header;
            Ar << Header.Magic;
            Ar << Header.Version;
            Ar << Header.AssetType;

            if (!Ar.IsOk() || Header.Magic != GBinaryMagic ||
                Header.AssetType != static_cast<uint32>(ExpectedAssetType))
            {
                return false;
            }

            if (Header.Version != GLegacyBinaryVersion &&
                Header.Version != GCurrentBinaryVersion)
            {
                return false;
            }

            OutVersion = Header.Version;
            OutMetadata = {};

            if (Header.Version >= 2)
            {
                Ar << OutMetadata.FileSize;
                Ar << OutMetadata.LastWriteTimeTicks;
                Ar << OutMetadata.ContentHash;
                Ar << OutMetadata.bHasContentHash;
            }

            return Ar.IsOk();
        }

        template <typename T>
        bool SaveImpl(const T &Data, const std::filesystem::path &Path,
                      ECookedBinaryAssetType AssetType, const FSourceRecord *Source)
        {
            if (Path.empty())
            {
                return false;
            }

            std::error_code             Ec;
            const std::filesystem::path ParentPath = Path.parent_path();
            if (!ParentPath.empty())
            {
                std::filesystem::create_directories(ParentPath, Ec);
                if (Ec)
                {
                    return false;
                }
            }

            FWindowsBinWriter Writer(Path);
            if (!Writer.IsOk() || !WriteHeader(Writer, AssetType, Source))
            {
                return false;
            }

            T Copy = Data;
            Writer << Copy;
            return Writer.IsOk();
        }

        template <typename T>
        bool LoadImpl(const std::filesystem::path &Path, T &OutData,
                      ECookedBinaryAssetType AssetType)
        {
            FWindowsBinReader Reader{Path};
            uint32               Version = 0;
            FBinarySourceMetadata Metadata;
            if (!Reader.IsOk() || !ReadHeader(Reader, AssetType, Version, Metadata))
            {
                return false;
            }

            T Temp;
            Temp.Reset();
            Reader << Temp;
            if (!Reader.IsOk() || !Temp.IsValid())
            {
                return false;
            }

            OutData = std::move(Temp);
            return true;
        }

        template <typename T>
        bool LoadImplIfCurrent(const std::filesystem::path &Path, const FSourceRecord &Source,
                               T &OutData, ECookedBinaryAssetType AssetType)
        {
            FWindowsBinReader Reader{Path};
            uint32               Version = 0;
            FBinarySourceMetadata Metadata;
            if (!Reader.IsOk() || !ReadHeader(Reader, AssetType, Version, Metadata))
            {
                return false;
            }

            if (Version < GCurrentBinaryVersion ||
                !IsStoredSourceMetadataCurrent(ToPublicMetadata(Metadata), Source))
            {
                return false;
            }

            T Temp;
            Temp.Reset();
            Reader << Temp;
            if (!Reader.IsOk() || !Temp.IsValid())
            {
                return false;
            }

            OutData = std::move(Temp);
            return true;
        }
    } // namespace

    bool IsStoredSourceMetadataCurrent(const FStoredSourceMetadata &Stored,
                                       const FSourceRecord         &CurrentSource)
    {
        if (Stored.FileSize != CurrentSource.FileSize)
        {
            return false;
        }

        if (Stored.LastWriteTimeTicks != CurrentSource.LastWriteTimeTicks)
        {
            return false;
        }

        if (Stored.bHasContentHash != CurrentSource.bHasContentHash)
        {
            return false;
        }

        if (Stored.bHasContentHash && Stored.ContentHash != CurrentSource.ContentHash)
        {
            return false;
        }

        return true;
    }

    FString GetSourceExtension(ECookedBinaryAssetType AssetType)
    {
        switch (AssetType)
        {
        case ECookedBinaryAssetType::Texture:
            return Asset::GetPrimarySourceExtension(Asset::EAssetFileKind::Texture);
        case ECookedBinaryAssetType::MaterialLibrary:
            return Asset::GetPrimarySourceExtension(Asset::EAssetFileKind::MaterialLibrary);
        case ECookedBinaryAssetType::StaticMesh:
            return Asset::GetPrimarySourceExtension(Asset::EAssetFileKind::StaticMesh);
        default:
            return {};
        }
    }

    FString GetBinaryExtension(ECookedBinaryAssetType AssetType)
    {
        switch (AssetType)
        {
        case ECookedBinaryAssetType::Texture:
            return Asset::GetPrimaryBakedExtension(Asset::EAssetFileKind::Texture);
        case ECookedBinaryAssetType::MaterialLibrary:
            return Asset::GetPrimaryBakedExtension(Asset::EAssetFileKind::MaterialLibrary);
        case ECookedBinaryAssetType::StaticMesh:
            return Asset::GetPrimaryBakedExtension(Asset::EAssetFileKind::StaticMesh);
        default:
            return {};
        }
    }

    bool TryGetAssetTypeFromSourcePath(const FString &SourcePath, ECookedBinaryAssetType &OutType)
    {
        switch (Asset::ClassifyAssetPath(SourcePath))
        {
        case Asset::EAssetFileKind::Texture:
            OutType = ECookedBinaryAssetType::Texture;
            return true;
        case Asset::EAssetFileKind::MaterialLibrary:
            OutType = ECookedBinaryAssetType::MaterialLibrary;
            return true;
        case Asset::EAssetFileKind::StaticMesh:
            OutType = ECookedBinaryAssetType::StaticMesh;
            return true;
        default:
            OutType = ECookedBinaryAssetType::Unknown;
            return false;
        }
    }

    FString MakeBinaryPathFromSourcePath(const FString &SourcePath)
    {
        return Asset::MakeBakedAssetPath(SourcePath);
    }

    bool SaveTexture(const FTextureCookedData &Data, const FString &Path,
                     const FSourceRecord      *Source)
    {
        return SaveTexture(Data, FPaths::PathFromUtf8(Path), Source);
    }

    bool SaveTexture(const FTextureCookedData &Data, const std::filesystem::path &Path,
                     const FSourceRecord      *Source)
    {
        return SaveImpl(Data, Path, ECookedBinaryAssetType::Texture, Source);
    }

    bool LoadTexture(const FString &Path, FTextureCookedData &OutData)
    {
        return LoadTexture(FPaths::PathFromUtf8(Path), OutData);
    }

    bool LoadTexture(const std::filesystem::path &Path, FTextureCookedData &OutData)
    {
        return LoadImpl(Path, OutData, ECookedBinaryAssetType::Texture);
    }

    bool LoadTextureIfCurrent(const FString &Path, const FSourceRecord &Source,
                              FTextureCookedData &OutData)
    {
        return LoadTextureIfCurrent(FPaths::PathFromUtf8(Path), Source, OutData);
    }

    bool LoadTextureIfCurrent(const std::filesystem::path &Path, const FSourceRecord &Source,
                              FTextureCookedData          &OutData)
    {
        return LoadImplIfCurrent(Path, Source, OutData, ECookedBinaryAssetType::Texture);
    }

    bool SaveMaterialLibrary(const FMtlCookedLibraryData &Data, const FString &Path,
                             const FSourceRecord         *Source)
    {
        return SaveMaterialLibrary(Data, FPaths::PathFromUtf8(Path), Source);
    }

    bool SaveMaterialLibrary(const FMtlCookedLibraryData &Data,
                             const std::filesystem::path &Path,
                             const FSourceRecord         *Source)
    {
        return SaveImpl(Data, Path, ECookedBinaryAssetType::MaterialLibrary, Source);
    }

    bool LoadMaterialLibrary(const FString &Path, FMtlCookedLibraryData &OutData)
    {
        return LoadMaterialLibrary(FPaths::PathFromUtf8(Path), OutData);
    }

    bool LoadMaterialLibrary(const std::filesystem::path &Path, FMtlCookedLibraryData &OutData)
    {
        return LoadImpl(Path, OutData, ECookedBinaryAssetType::MaterialLibrary);
    }

    bool LoadMaterialLibraryIfCurrent(const FString &Path, const FSourceRecord &Source,
                                      FMtlCookedLibraryData &OutData)
    {
        return LoadMaterialLibraryIfCurrent(FPaths::PathFromUtf8(Path), Source, OutData);
    }

    bool LoadMaterialLibraryIfCurrent(const std::filesystem::path &Path,
                                      const FSourceRecord         &Source,
                                      FMtlCookedLibraryData       &OutData)
    {
        return LoadImplIfCurrent(Path, Source, OutData,
                                 ECookedBinaryAssetType::MaterialLibrary);
    }

    bool SaveStaticMesh(const FObjCookedData &Data, const FString &Path,
                        const FSourceRecord  *Source)
    {
        return SaveStaticMesh(Data, FPaths::PathFromUtf8(Path), Source);
    }

    bool SaveStaticMesh(const FObjCookedData &Data, const std::filesystem::path &Path,
                        const FSourceRecord  *Source)
    {
        return SaveImpl(Data, Path, ECookedBinaryAssetType::StaticMesh, Source);
    }

    bool LoadStaticMesh(const FString &Path, FObjCookedData &OutData)
    {
        return LoadStaticMesh(FPaths::PathFromUtf8(Path), OutData);
    }

    bool LoadStaticMesh(const std::filesystem::path &Path, FObjCookedData &OutData)
    {
        return LoadImpl(Path, OutData, ECookedBinaryAssetType::StaticMesh);
    }

    bool LoadStaticMeshIfCurrent(const FString &Path, const FSourceRecord &Source,
                                 FObjCookedData &OutData)
    {
        return LoadStaticMeshIfCurrent(FPaths::PathFromUtf8(Path), Source, OutData);
    }

    bool LoadStaticMeshIfCurrent(const std::filesystem::path &Path,
                                 const FSourceRecord         &Source,
                                 FObjCookedData              &OutData)
    {
        return LoadImplIfCurrent(Path, Source, OutData, ECookedBinaryAssetType::StaticMesh);
    }

} // namespace Asset::Binary
