#pragma once

#include "Asset/Cooked/MtlCookedData.h"
#include "Core/Math/Vector3.h"
#include "Renderer/D3D11/Resources/D3D11Buffer.h"
#include "Renderer/D3D11/Resources/D3D11Texture.h"
#include <memory>

class FD3D11Device;
class UTexture;

namespace Asset
{
    struct FMaterialRenderResource
    {
        std::shared_ptr<FD3D11Texture> BaseColorTexture;
        std::shared_ptr<FD3D11Texture> NormalTexture;
        std::shared_ptr<FD3D11Texture> ORMTexture;
        std::shared_ptr<FD3D11Buffer>  ParameterBuffer;

        static std::shared_ptr<FMaterialRenderResource> Create(const FMtlCookedData &CookedData,
                                                               const UTexture       *BaseColorTexture,
                                                               const UTexture       *NormalTexture,
                                                               const UTexture       *ORMTexture,
                                                               FD3D11Device         &Device);

        bool IsValid() const;
        void Reset();

      private:
        struct FMaterialParameters
        {
            FVector3 DiffuseColor = FVector3(1.0f, 1.0f, 1.0f);
            float    Opacity = 1.0f;
            FVector3 AmbientColor = FVector3(0.0f, 0.0f, 0.0f);
            float    Shininess = 0.0f;
            FVector3 SpecularColor = FVector3(0.0f, 0.0f, 0.0f);
            float    Padding = 0.0f;
        };

        static std::shared_ptr<FD3D11Texture> ResolveTextureFromObject(const UTexture *TextureObject);
        static std::shared_ptr<FD3D11Buffer>   CreateParameterBuffer(const FMtlCookedData &CookedData,
                                                                     FD3D11Device         &Device);
    };
} // namespace Asset
