#include "Resource/TextureManager.h"
#include "Resource/ResourceLoaderUtils.h"
#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include <filesystem>

FTextureManager::~FTextureManager()
{
    ReleaseGPUResources();
}

void FTextureManager::SetDevice(ID3D11Device* InDevice)
{
    Device = InDevice;
}

FTextureResource* FTextureManager::Load(const FString& Path)
{
    // If already exists, return it
    FString Key = FPaths::FromPath(std::filesystem::path(FPaths::ToWide(Path)).stem());
    auto It = TextureResources.find(Key);
    if (It != TextureResources.end())
    {
        return &It->second;
    }

    FTextureResource Resource;
    Resource.Name = FName(Key);
    Resource.Path = Path;
    Resource.Columns = 1;
    Resource.Rows = 1;
    Resource.SRV = nullptr;

    if (Device)
    {
        ResourceLoaderUtils::LoadTextureSRV(Device, Resource);
    }

    TextureResources[Key] = Resource;
    return &TextureResources[Key];
}

FTextureResource* FTextureManager::Find(const FString& Key)
{
    auto It = TextureResources.find(Key);
    return (It != TextureResources.end()) ? &It->second : nullptr;
}

void FTextureManager::Unload(const FString& Key)
{
    auto It = TextureResources.find(Key);
    if (It != TextureResources.end())
    {
        ResourceLoaderUtils::ReleaseTextureSRV(It->second);
        TextureResources.erase(It);
    }
}

void FTextureManager::UnloadAll()
{
    ReleaseGPUResources();
    TextureResources.clear();
}

void FTextureManager::ScanAssets()
{
    const std::filesystem::path AssetRoot = FPaths::AssetDir();
    if (!std::filesystem::exists(AssetRoot)) return;

    const std::filesystem::path ProjectRoot(FPaths::RootDir());
    const std::unordered_set<std::wstring> ValidExtensions = { L".png", L".jpg", L".jpeg", L".dds", L".bmp", L".tga" };

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
    {
        if (!Entry.is_regular_file()) continue;

        const std::filesystem::path& Path = Entry.path();
        std::wstring Ext = Path.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::tolower);

        if (ValidExtensions.find(Ext) == ValidExtensions.end()) continue;

        // Skip sidecar textures if they are in specific folders or have specific suffixes? 
        // For now, scan all as textures.

        FString Key = FPaths::ToUtf8(Path.stem().wstring());
        FString RelativePath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());

        if (TextureResources.find(Key) == TextureResources.end())
        {
            FTextureResource Resource;
            Resource.Name = FName(Key);
            Resource.Path = RelativePath;
            Resource.Columns = 1;
            Resource.Rows = 1;
            Resource.SRV = nullptr;
            
            // Note: We don't load SRV here. It will be loaded when LoadGPUResources is called or on demand.
            TextureResources[Key] = Resource;
        }
    }
    UE_LOG(Resource, Info, "TextureManager scanned %zu textures.", TextureResources.size());
}

FTextureResource* FTextureManager::FindTexture(const FName& TextureName)
{
    return Find(TextureName.ToString());
}

const FTextureResource* FTextureManager::FindTexture(const FName& TextureName) const
{
    auto It = TextureResources.find(TextureName.ToString());
    return (It != TextureResources.end()) ? &It->second : nullptr;
}

TArray<FString> FTextureManager::GetTextureNames() const
{
    TArray<FString> Names;
    for (const auto& [Key, Resource] : TextureResources)
    {
        if (!FPaths::IsEditorAssetPath(Resource.Path))
        {
            Names.push_back(Key);
        }
    }
    return Names;
}

TArray<FString> FTextureManager::GetEditorTextureNames() const
{
    TArray<FString> Names;
    for (const auto& [Key, Resource] : TextureResources)
    {
        if (FPaths::IsEditorAssetPath(Resource.Path))
        {
            Names.push_back(Key);
        }
    }
    return Names;
}

void FTextureManager::ReleaseGPUResources()
{
    for (auto& [Key, Resource] : TextureResources)
    {
        ResourceLoaderUtils::ReleaseTextureSRV(Resource);
    }
}
