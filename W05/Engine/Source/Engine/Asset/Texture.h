#pragma once

#include "Asset/Cooked/TextureCookedData.h"
#include "Asset/Runtime/TextureRenderResource.h"
#include "Engine/Asset/Asset.h"
#include "Renderer/D3D11/D3D11Device.h"
#include <memory>
#include <unordered_map>
#include <utility>

using namespace Asset;

class UTexture : public UAsset
{
    DECLARE_RTTI(UTexture, UAsset)

  public:
    const std::shared_ptr<FTextureCookedData> &GetCookedData() const { return CookedData; }

    void SetCookedData(std::shared_ptr<FTextureCookedData> InCookedData)
    {
        CookedData = std::move(InCookedData);
    }

    const std::shared_ptr<FTextureRenderResource> &GetRenderResource() const
    {
        return RenderResource;
    }
    const std::shared_ptr<FTextureRenderResource> &
    GetRenderResourceForDistance(float Distance, float ObjectRadius, FD3D11Device &InDevice);

    void SetRenderResource(std::shared_ptr<FTextureRenderResource> InRenderResource)
    {
        RenderResource = std::move(InRenderResource);
    }

    void ResetRenderResource()
    {
        if (RenderResource)
        {
            RenderResource->Reset();
        }
        RenderResource.reset();
    }

    bool LoadFromCooked(const FString                      &InAssetPath,
                        std::shared_ptr<FTextureCookedData> InCookedData, FD3D11Device &InDevice);

    bool IsValidLowLevel() const override;

  private:
    std::shared_ptr<FTextureCookedData>     CookedData;
    std::shared_ptr<FTextureRenderResource> RenderResource;
    std::unordered_map<uint32, std::shared_ptr<FTextureRenderResource>> DistanceLodRenderResources;
    uint32                                   LastSelectedMipLevel = 0;
};
