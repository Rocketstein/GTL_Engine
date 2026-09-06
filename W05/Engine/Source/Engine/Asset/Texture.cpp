#include "Engine/Asset/Texture.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include <algorithm>
#include <cmath>
#include <filesystem>


REGISTER_CLASS(, UTexture)

namespace
{
    static FString BuildAssetNameFromPath(const FString &InAssetPath)
    {
        if (InAssetPath.empty())
        {
            return {};
        }

        const std::filesystem::path FilePath = FPaths::PathFromUtf8(InAssetPath);
        return FPaths::Utf8FromPath(FilePath.stem());
    }

    static FString BuildAssetFileNameFromPath(const FString &InAssetPath)
    {
        if (InAssetPath.empty())
        {
            return {};
        }

        const std::filesystem::path FilePath = FPaths::PathFromUtf8(InAssetPath);
        return FPaths::Utf8FromPath(FilePath.filename());
    }

    static uint32 ComputeTextureMipLevelByDistance(const FTextureCookedData &CookedData, float Distance,
                                                   float ObjectRadius)
    {
        const uint32 MipCount = CookedData.GetMipCount();
        if (MipCount <= 1)
        {
            return 0;
        }

        const float ClampedDistance = std::max(0.0f, Distance);
        const float EffectiveRadius = std::max(1.0f, ObjectRadius);
        const float RelativeDistance = ClampedDistance / EffectiveRadius;
        const float LodContinuous = std::max(0.0f, std::log2(1.0f + RelativeDistance));
        const uint32 LodDiscrete = static_cast<uint32>(LodContinuous);
        return std::min(LodDiscrete, MipCount - 1u);
    }
} // namespace

bool UTexture::LoadFromCooked(const FString                      &InAssetPath,
                              std::shared_ptr<FTextureCookedData> InCookedData,
                              FD3D11Device                       &InDevice)
{
    if (InCookedData == nullptr)
    {
        UE_LOG(TextureAsset, ELogLevel::Error, "Texture asset load failed: %s",
               InAssetPath.c_str());
        return false;
    }

    if (CookedData == InCookedData && GetAssetPath() == InAssetPath && RenderResource != nullptr)
    {
        SetLoaded(true);
        return true;
    }

    std::shared_ptr<FTextureRenderResource> NewRenderResource =
        FTextureRenderResource::Create(*InCookedData, InDevice);
    if (NewRenderResource == nullptr)
    {
        UE_LOG(TextureAsset, ELogLevel::Error, "Texture asset load failed: %s",
               InAssetPath.c_str());
        return false;
    }

    SetAssetPath(InAssetPath);
    SetAssetName(BuildAssetNameFromPath(InAssetPath));
    SetCookedData(std::move(InCookedData));
    SetRenderResource(std::move(NewRenderResource));
    DistanceLodRenderResources.clear();
    DistanceLodRenderResources.emplace(0u, RenderResource);

    const uint32 MipCount = CookedData->GetMipCount();
    if (MipCount > 1u)
    {
        for (uint32 MipLevel = 1u; MipLevel < MipCount; ++MipLevel)
        {
            std::shared_ptr<FTextureRenderResource> MipResource =
                FTextureRenderResource::CreateForMipLevel(*CookedData, MipLevel, InDevice);
            if (MipResource != nullptr)
            {
                DistanceLodRenderResources.emplace(MipLevel, std::move(MipResource));
            }
            else
            {
                UE_LOG(TextureAsset, ELogLevel::Warning,
                       "Texture mip preload failed: path=%s, mip=%u", InAssetPath.c_str(),
                       static_cast<unsigned>(MipLevel));
            }
        }
    }

    LastSelectedMipLevel = 0;
    UE_LOG(TextureAsset, ELogLevel::Debug,
           "Texture mip data loaded: path=%s, mipCount=%u, base=%ux%u", InAssetPath.c_str(),
           static_cast<unsigned>(CookedData->GetMipCount()), static_cast<unsigned>(CookedData->Width),
           static_cast<unsigned>(CookedData->Height));
    UE_LOG(TextureAsset, ELogLevel::Info,
           "Texture mip render resources preloaded: path=%s, loadedMips=%u/%u", InAssetPath.c_str(),
           static_cast<unsigned>(DistanceLodRenderResources.size()),
           static_cast<unsigned>(std::max<uint32>(1u, MipCount)));
    SetLoaded(true);
    UE_LOG(TextureAsset, ELogLevel::Debug, "Texture asset load succeeded: %s", InAssetPath.c_str());
    return true;
}

const std::shared_ptr<FTextureRenderResource> &
UTexture::GetRenderResourceForDistance(float Distance, float ObjectRadius, FD3D11Device &InDevice)
{
    (void)InDevice;
    if (CookedData == nullptr || RenderResource == nullptr)
    {
        return RenderResource;
    }

    const uint32 PreviousMip = LastSelectedMipLevel;
    const uint32 SelectedMip = ComputeTextureMipLevelByDistance(*CookedData, Distance, ObjectRadius);
    LastSelectedMipLevel = SelectedMip;

    if (PreviousMip != SelectedMip)
    {
        const FString AssetFileName = BuildAssetFileNameFromPath(GetAssetPath());
        UE_LOG(TextureAsset, ELogLevel::Info,
               "Texture LOD changed: file=%s, mip=%u->%u, distance=%.2f, radius=%.2f, mipCount=%u",
               AssetFileName.c_str(), static_cast<unsigned>(PreviousMip),
               static_cast<unsigned>(SelectedMip), static_cast<double>(Distance),
               static_cast<double>(ObjectRadius), static_cast<unsigned>(CookedData->GetMipCount()));
    }

    const auto Found = DistanceLodRenderResources.find(SelectedMip);
    if (Found != DistanceLodRenderResources.end() && Found->second != nullptr)
    {
        return Found->second;
    }

    UE_LOG(TextureAsset, ELogLevel::Warning,
           "Texture LOD fallback to base mip: path=%s, requestedMip=%u", GetAssetPath().c_str(),
           static_cast<unsigned>(SelectedMip));
    return RenderResource;
}

bool UTexture::IsValidLowLevel() const
{
    if (CookedData == nullptr)
    {
        return false;
    }

    if (!CookedData->IsValid())
    {
        return false;
    }

    if (RenderResource == nullptr)
    {
        return false;
    }

    return true;
}
