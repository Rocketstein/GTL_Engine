#include "Asset/Runtime/MaterialRenderResource.h"
#include "Engine/Asset/Texture.h"
#include "Renderer/D3D11/D3D11Device.h"

namespace Asset
{
    std::shared_ptr<FMaterialRenderResource>
    FMaterialRenderResource::Create(const FMtlCookedData &CookedData,
                                    const UTexture       *BaseColorTextureObject,
                                    const UTexture       *NormalTextureObject,
                                    const UTexture       *ORMTextureObject,
                                    FD3D11Device         &Device)
    {
        if (!CookedData.IsValid())
        {
            return nullptr;
        }

        std::shared_ptr<FMaterialRenderResource> Resource =
            std::make_shared<FMaterialRenderResource>();

        Resource->BaseColorTexture = ResolveTextureFromObject(BaseColorTextureObject);
        Resource->NormalTexture = ResolveTextureFromObject(NormalTextureObject);
        Resource->ORMTexture = ResolveTextureFromObject(ORMTextureObject);
        Resource->ParameterBuffer = CreateParameterBuffer(CookedData, Device);

        if (!Resource->IsValid())
        {
            return nullptr;
        }

        return Resource;
    }

    bool FMaterialRenderResource::IsValid() const
    {
        return ParameterBuffer != nullptr || BaseColorTexture != nullptr ||
               NormalTexture != nullptr || ORMTexture != nullptr;
    }

    void FMaterialRenderResource::Reset()
    {
        BaseColorTexture.reset();
        NormalTexture.reset();
        ORMTexture.reset();
        ParameterBuffer.reset();
    }

    std::shared_ptr<FD3D11Texture>
    FMaterialRenderResource::ResolveTextureFromObject(const UTexture *TextureObject)
    {
        if (TextureObject == nullptr)
        {
            return nullptr;
        }

        const auto &TextureRenderResource = TextureObject->GetRenderResource();
        if (TextureRenderResource == nullptr || !TextureRenderResource->IsValid())
        {
            return nullptr;
        }

        return TextureRenderResource->Texture;
    }

    std::shared_ptr<FD3D11Buffer>
    FMaterialRenderResource::CreateParameterBuffer(const FMtlCookedData &CookedData,
                                                   FD3D11Device         &Device)
    {
        FMaterialParameters Parameters;
        Parameters.DiffuseColor = CookedData.DiffuseColor;
        Parameters.Opacity = CookedData.Opacity;
        Parameters.AmbientColor = CookedData.AmbientColor;
        Parameters.Shininess = CookedData.Shininess;
        Parameters.SpecularColor = CookedData.SpecularColor;

        std::shared_ptr<FD3D11Buffer> ParameterBuffer = std::make_shared<FD3D11Buffer>();
        if (!ParameterBuffer->CreateConstantBuffer(&Device, sizeof(FMaterialParameters),
                                                   &Parameters))
        {
            return nullptr;
        }

        return ParameterBuffer;
    }
} // namespace Asset
