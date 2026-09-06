#include "Resource/ResourceLoaderUtils.h"
#include "Core/ResourceTypes.h"
#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "Profiling/MemoryStats.h"

#include <d3d11.h>
#include <filesystem>
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

namespace ResourceLoaderUtils
{
    bool LoadTextureSRV(ID3D11Device* Device, FTextureAtlasResource& Resource)
    {
        if (!Device)
        {
            UE_LOG(Resource, Error, "LoadTextureSRV failed: device is null.");
            return false;
        }

        // Release existing if any
        ReleaseTextureSRV(Resource);

        std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Resource.Path));

        // Extension check
        std::filesystem::path Ext = std::filesystem::path(Resource.Path).extension();
        FString ExtStr = Ext.string();
        for (char& c : ExtStr)
            c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

        HRESULT hr;
        if (ExtStr == ".dds")
        {
            hr = DirectX::CreateDDSTextureFromFileEx(
                Device,
                FullPath.c_str(),
                0,
                D3D11_USAGE_IMMUTABLE,
                D3D11_BIND_SHADER_RESOURCE,
                0, 0,
                DirectX::DDS_LOADER_DEFAULT,
                nullptr,
                &Resource.SRV);
        }
        else
        {
            hr = DirectX::CreateWICTextureFromFileEx(
                Device,
                FullPath.c_str(),
                0,
                D3D11_USAGE_IMMUTABLE,
                D3D11_BIND_SHADER_RESOURCE,
                0, 0,
                DirectX::WIC_LOADER_FORCE_RGBA32,
                nullptr,
                &Resource.SRV);
        }

        if (FAILED(hr) || !Resource.SRV)
        {
            UE_LOG(Resource, Error, "Failed to create SRV for resource '%s' at path '%s'. HR=0x%08X",
                   Resource.Name.ToString().c_str(), Resource.Path.c_str(), hr);
            return false;
        }

        // Track memory
        ID3D11Resource* TextureResource = nullptr;
        Resource.SRV->GetResource(&TextureResource);
        Resource.TrackedMemoryBytes = MemoryStats::CalculateTextureMemory(TextureResource);
        if (TextureResource)
        {
            TextureResource->Release();
        }

        if (Resource.TrackedMemoryBytes > 0)
        {
            MemoryStats::AddTextureMemory(Resource.TrackedMemoryBytes);
        }

        return true;
    }

    void ReleaseTextureSRV(FTextureAtlasResource& Resource)
    {
        if (Resource.TrackedMemoryBytes > 0)
        {
            MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
            Resource.TrackedMemoryBytes = 0;
        }

        if (Resource.SRV)
        {
            Resource.SRV->Release();
            Resource.SRV = nullptr;
        }
    }
}
