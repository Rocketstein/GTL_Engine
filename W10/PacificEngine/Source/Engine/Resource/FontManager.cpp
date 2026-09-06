#include "Resource/FontManager.h"
#include "Resource/ResourceLoaderUtils.h"
#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"
#include <filesystem>
#include <fstream>

FFontManager::~FFontManager()
{
    ReleaseGPUResources();
}

void FFontManager::SetDevice(ID3D11Device* InDevice)
{
    Device = InDevice;
}

FFontResource* FFontManager::Load(const FString& Path)
{
    FString Key = FPaths::FromPath(std::filesystem::path(FPaths::ToWide(Path)).stem());
    auto It = FontResources.find(Key);
    if (It != FontResources.end()) return &It->second;

    std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
    if (!File.is_open()) return nullptr;

    FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    json::JSON Root = json::JSON::Load(Content);

    FFontResource Resource;
    Resource.Name = FName(Key);
    Resource.Path = Root["Path"].ToString();
    Resource.Columns = static_cast<uint32>(Root["Columns"].ToInt());
    Resource.Rows = static_cast<uint32>(Root["Rows"].ToInt());
    Resource.SRV = nullptr;

    if (Device)
    {
        ResourceLoaderUtils::LoadTextureSRV(Device, Resource);
    }

    FontResources[Key] = Resource;
    return &FontResources[Key];
}

FFontResource* FFontManager::Find(const FString& Key)
{
    auto It = FontResources.find(Key);
    return (It != FontResources.end()) ? &It->second : nullptr;
}

void FFontManager::Unload(const FString& Key)
{
    auto It = FontResources.find(Key);
    if (It != FontResources.end())
    {
        ResourceLoaderUtils::ReleaseTextureSRV(It->second);
        FontResources.erase(It);
    }
}

void FFontManager::UnloadAll()
{
    ReleaseGPUResources();
    FontResources.clear();
}

void FFontManager::ScanAssets()
{
    const std::filesystem::path AssetRoot = FPaths::AssetDir();
    if (!std::filesystem::exists(AssetRoot)) return;

    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
    {
        if (!Entry.is_regular_file()) continue;
        if (Entry.path().extension() != L".font") continue;

        FString FontFilePath = FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());
        Load(FontFilePath);
    }
    UE_LOG(Resource, Info, "FontManager scanned %zu fonts.", FontResources.size());
}

FFontResource* FFontManager::FindFont(const FName& FontName)
{
    return Find(FontName.ToString());
}

const FFontResource* FFontManager::FindFont(const FName& FontName) const
{
    auto It = FontResources.find(FontName.ToString());
    return (It != FontResources.end()) ? &It->second : nullptr;
}

TArray<FString> FFontManager::GetFontNames() const
{
    TArray<FString> Names;
    for (const auto& [Key, _] : FontResources) Names.push_back(Key);
    return Names;
}

void FFontManager::ReleaseGPUResources()
{
    for (auto& [Key, Resource] : FontResources)
    {
        ResourceLoaderUtils::ReleaseTextureSRV(Resource);
    }
}

bool FFontManager::LoadGPUResources(ID3D11Device* InDevice)
{
    Device = InDevice;
    bool bSuccess = true;
    for (auto& [Key, Resource] : FontResources)
    {
        if (!ResourceLoaderUtils::LoadTextureSRV(Device, Resource))
        {
            bSuccess = false;
        }
    }
    return bSuccess;
}
