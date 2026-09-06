#pragma once

#include "Asset/Cooked/MtlCookedData.h"
#include "Asset/Runtime/MaterialRenderResource.h"
#include "Engine/Asset/Asset.h"
#include "Renderer/D3D11/D3D11Device.h"
#include <filesystem>
#include <memory>
#include <utility>

using namespace Asset;

class UTexture;

class UMaterial : public UAsset
{
    DECLARE_RTTI(UMaterial, UAsset)

  public:
    bool LoadFromCooked(const FString &InAssetPath, std::shared_ptr<FMtlCookedData> InCookedData,
                        FD3D11Device &InDevice);

    bool RebuildRenderResource(FD3D11Device &InDevice);

    const std::shared_ptr<FMtlCookedData> &GetCookedData() const { return CookedData; }

    void SetCookedData(std::shared_ptr<FMtlCookedData> InCookedData)
    {
        CookedData = std::move(InCookedData);
    }

    const std::shared_ptr<FMaterialRenderResource> &GetRenderResource() const
    {
        return RenderResource;
    }

    void SetRenderResource(std::shared_ptr<FMaterialRenderResource> InRenderResource)
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

    void SetTextureForSlot(EMaterialTextureSlot Slot, UTexture *InTexture);

    UTexture *GetTextureForSlot(EMaterialTextureSlot Slot) const;
    UTexture *GetBaseColorTexture() const { return BaseColorTexture; }
    UTexture *GetNormalTexture() const { return NormalTexture; }
    UTexture *GetORMTexture() const { return ORMTexture; }

    bool IsValidLowLevel() const override;

  private:
    std::shared_ptr<FMtlCookedData>          CookedData;
    std::shared_ptr<FMaterialRenderResource> RenderResource;
    UTexture                                *BaseColorTexture = nullptr;
    UTexture                                *NormalTexture = nullptr;
    UTexture                                *ORMTexture = nullptr;
};
