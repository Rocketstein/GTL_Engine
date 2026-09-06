#include "Resource/SpriteVfxManager.h"
#include "Resource/ResourceLoaderUtils.h"
#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"
#include <filesystem>
#include <fstream>

FSpriteVfxManager::~FSpriteVfxManager()
{
    ReleaseGPUResources();
}

void FSpriteVfxManager::SetDevice(ID3D11Device* InDevice)
{
    Device = InDevice;
}

FSpriteVfxResource* FSpriteVfxManager::Load(const FString& Path)
{
    FString Key = FPaths::FromPath(std::filesystem::path(FPaths::ToWide(Path)).stem());
    auto It = SpriteVfxResources.find(Key);
    if (It != SpriteVfxResources.end()) return &It->second;

    std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
    if (!File.is_open()) return nullptr;

    FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    json::JSON Root = json::JSON::Load(Content);

    FSpriteVfxResource Resource;
    Resource.Name = FName(Key);
    Resource.Path = Root["Path"].ToString();
    Resource.Columns = static_cast<uint32>(Root["Columns"].ToInt());
    Resource.Rows = static_cast<uint32>(Root["Rows"].ToInt());
    Resource.SRV = nullptr;

    if (Device)
    {
        ResourceLoaderUtils::LoadTextureSRV(Device, Resource);
    }

    SpriteVfxResources[Key] = Resource;
    return &SpriteVfxResources[Key];
}

FSpriteVfxResource* FSpriteVfxManager::Find(const FString& Key)
{
    auto It = SpriteVfxResources.find(Key);
    return (It != SpriteVfxResources.end()) ? &It->second : nullptr;
}

void FSpriteVfxManager::Unload(const FString& Key)
{
    auto It = SpriteVfxResources.find(Key);
    if (It != SpriteVfxResources.end())
    {
        ResourceLoaderUtils::ReleaseTextureSRV(It->second);
        SpriteVfxResources.erase(It);
    }
}

void FSpriteVfxManager::UnloadAll()
{
    ReleaseGPUResources();
    SpriteVfxResources.clear();
}

void FSpriteVfxManager::ScanAssets()
{
    const std::filesystem::path AssetRoot = FPaths::AssetDir();
    if (!std::filesystem::exists(AssetRoot)) return;

    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
    {
        if (!Entry.is_regular_file()) continue;
        if (Entry.path().extension() != L".spritevfx") continue;

        FString SpriteVfxFilePath = FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());
        Load(SpriteVfxFilePath);
    }
    UE_LOG(Resource, Info, "SpriteVfxManager scanned %zu sprite vfx.", SpriteVfxResources.size());
}

FSpriteVfxResource* FSpriteVfxManager::FindSpriteVfx(const FName& SpriteVfxName)
{
    return Find(SpriteVfxName.ToString());
}

const FSpriteVfxResource* FSpriteVfxManager::FindSpriteVfx(const FName& SpriteVfxName) const
{
    auto It = SpriteVfxResources.find(SpriteVfxName.ToString());
    return (It != SpriteVfxResources.end()) ? &It->second : nullptr;
}

TArray<FString> FSpriteVfxManager::GetSpriteVfxNames() const
{
    TArray<FString> Names;
    for (const auto& [Key, _] : SpriteVfxResources) Names.push_back(Key);
    return Names;
}

void FSpriteVfxManager::ReleaseGPUResources()
{
    for (auto& [Key, Resource] : SpriteVfxResources)
    {
        ResourceLoaderUtils::ReleaseTextureSRV(Resource);
    }
}

bool FSpriteVfxManager::LoadGPUResources(ID3D11Device* InDevice)
{
    Device = InDevice;
    bool bSuccess = true;
    for (auto& [Key, Resource] : SpriteVfxResources)
    {
        if (!ResourceLoaderUtils::LoadTextureSRV(Device, Resource))
        {
            bSuccess = false;
        }
    }
    return bSuccess;
}
