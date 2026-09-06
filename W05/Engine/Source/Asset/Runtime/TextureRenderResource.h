#pragma once

#include "Asset/Cooked/TextureCookedData.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/D3D11/D3D11Device.h"
#include "Renderer/D3D11/Resources/D3D11Texture.h"
#include <memory>

namespace Asset
{
    enum class ETexturePixelFormat : uint8
    {
        Unknown,
        R8,
        RG8,
        RGB8,
        RGBA8,
    };

    struct FTextureRenderResource
    {
        uint32              Width = 0;
        uint32              Height = 0;
        ETexturePixelFormat Format = ETexturePixelFormat::Unknown;

        std::shared_ptr<FD3D11Texture> Texture;

        static std::shared_ptr<FTextureRenderResource> Create(const FTextureCookedData &CookedData,
                                                              FD3D11Device             &Device);
        static std::shared_ptr<FTextureRenderResource>
        CreateForMipLevel(const FTextureCookedData &CookedData, uint32 MipLevel, FD3D11Device &Device);

        bool IsValid() const;
        void Reset();

        ID3D11ShaderResourceView *GetSRV() const { return Texture ? Texture->GetSRV() : nullptr; }

      private:
        static DXGI_FORMAT         ResolveDxgiFormat(uint32 Channels, bool bSRGB);
        static ETexturePixelFormat ResolveTexturePixelFormat(uint32 Channels);
    };
} // namespace Asset
