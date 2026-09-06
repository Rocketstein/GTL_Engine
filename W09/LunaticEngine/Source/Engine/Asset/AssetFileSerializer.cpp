#include "Engine/Asset/AssetFileSerializer.h"

#include "Engine/Asset/AssetData.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Serialization/WindowsArchive.h"
#include "Object/ObjectFactory.h"

namespace
{
    uint32 GCurrentAssetSerializationVersion = FAssetFileSerializer::AssetVersion;

    void WriteError(FString *OutError, const FString &Message)
    {
        if (OutError)
        {
            *OutError = Message;
        }
    }

    FString ToAssetPathString(const std::filesystem::path &FilePath) { return FPaths::ToUtf8(FilePath.lexically_normal().generic_wstring()); }
} // namespace

namespace FAssetFileSerializer
{
    bool SaveAssetToFile(const std::filesystem::path &FilePath, UAssetData *Asset, FString *OutError)
    {
        if (!Asset)
        {
            WriteError(OutError, "Save failed: Asset is null.");
            return false;
        }

        FWindowsBinWriter Writer(ToAssetPathString(FilePath));
        if (!Writer.IsValid())
        {
            WriteError(OutError, "Save failed: Could not open file for writing.");
            return false;
        }

        uint32  Magic = AssetMagic;
        uint32  Version = AssetVersion;
        FString ClassName = Asset->GetClass()->GetName();

        Writer << Magic;
        Writer << Version;
        Writer << ClassName;
        GCurrentAssetSerializationVersion = Version;
        Asset->Serialize(Writer);
        GCurrentAssetSerializationVersion = AssetVersion;
        return true;
    }

    UAssetData *LoadAssetFromFile(const std::filesystem::path &FilePath, FString *OutError)
    {
        FWindowsBinReader Reader(ToAssetPathString(FilePath));
        if (!Reader.IsValid())
        {
            WriteError(OutError, "Load failed: Could not open file.");
            return nullptr;
        }

        uint32  Magic = 0;
        uint32  Version = 0;
        FString ClassName;

        Reader << Magic;
        Reader << Version;
        Reader << ClassName;

        if (Magic != AssetMagic)
        {
            WriteError(OutError, "Load failed: Unknown .uasset binary format.");
            return nullptr;
        }

        if (Version == 0 || Version > AssetVersion)
        {
            WriteError(OutError, "Load failed: Unsupported .uasset version.");
            return nullptr;
        }

        UObject *Object = FObjectFactory::Get().Create(ClassName);
        if (!Object)
        {
            WriteError(OutError, "Load failed: Unknown asset class: " + ClassName);
            return nullptr;
        }

        UAssetData *Asset = Cast<UAssetData>(Object);
        if (!Asset)
        {
            UObjectManager::Get().DestroyObject(Object);
            WriteError(OutError, "Load failed: File root object is not UAssetData.");
            return nullptr;
        }

        GCurrentAssetSerializationVersion = Version;
        Asset->Serialize(Reader);
        GCurrentAssetSerializationVersion = AssetVersion;
        return Asset;
    }

    uint32 GetCurrentAssetSerializationVersion() { return GCurrentAssetSerializationVersion; }
} // namespace FAssetFileSerializer
