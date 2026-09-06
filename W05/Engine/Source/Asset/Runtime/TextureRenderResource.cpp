#include "Asset/Runtime/TextureRenderResource.h"
#include <algorithm>

namespace Asset
{
    namespace
    {
        static bool ResolveMipInput(const FTextureCookedData &CookedData, uint32 MipLevel,
                                    uint32 &OutWidth, uint32 &OutHeight, const uint8 *&OutPixels)
        {
            if (CookedData.MipChain.empty())
            {
                if (MipLevel != 0)
                {
                    return false;
                }

                OutWidth = CookedData.Width;
                OutHeight = CookedData.Height;
                OutPixels = CookedData.Pixels.data();
                return true;
            }

            const uint32 ClampedMip = std::min(MipLevel, static_cast<uint32>(CookedData.MipChain.size() - 1));
            const FTextureMipLevel &SelectedMip = CookedData.MipChain[ClampedMip];
            if (!SelectedMip.IsValid())
            {
                return false;
            }

            OutWidth = SelectedMip.Width;
            OutHeight = SelectedMip.Height;
            OutPixels = SelectedMip.Pixels.data();
            return true;
        }
    } // namespace

    std::shared_ptr<FTextureRenderResource>
    FTextureRenderResource::Create(const FTextureCookedData &CookedData, FD3D11Device &Device)
    {
        return CreateForMipLevel(CookedData, 0, Device);
    }

    std::shared_ptr<FTextureRenderResource>
    FTextureRenderResource::CreateForMipLevel(const FTextureCookedData &CookedData, uint32 MipLevel,
                                              FD3D11Device &Device)
    {
        if (!CookedData.IsValid())
        {
            return nullptr;
        }

        uint32 Width = 0;
        uint32 Height = 0;
        const uint8 *Pixels = nullptr;
        if (!ResolveMipInput(CookedData, MipLevel, Width, Height, Pixels) || Pixels == nullptr)
        {
            return nullptr;
        }

        DXGI_FORMAT   DxgiFormat = ResolveDxgiFormat(CookedData.Channels, CookedData.bSRGB);
        uint32        Pitch = Width * CookedData.Channels;
        TArray<uint8> ExpandedRgbaPixels;

        if (CookedData.Channels == 3)
        {
            ExpandedRgbaPixels.resize(static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u);
            for (size_t SourceIndex = 0, TargetIndex = 0;
                 SourceIndex + 2 < static_cast<size_t>(Width) * static_cast<size_t>(Height) * 3u &&
                 TargetIndex + 3 < ExpandedRgbaPixels.size();
                 SourceIndex += 3, TargetIndex += 4)
            {
                ExpandedRgbaPixels[TargetIndex + 0] = Pixels[SourceIndex + 0];
                ExpandedRgbaPixels[TargetIndex + 1] = Pixels[SourceIndex + 1];
                ExpandedRgbaPixels[TargetIndex + 2] = Pixels[SourceIndex + 2];
                ExpandedRgbaPixels[TargetIndex + 3] = 255;
            }

            Pixels = ExpandedRgbaPixels.data();
            Pitch = Width * 4u;
            DxgiFormat =
                CookedData.bSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        if (DxgiFormat == DXGI_FORMAT_UNKNOWN)
        {
            return nullptr;
        }

        std::shared_ptr<FD3D11Texture> Texture2D = std::make_shared<FD3D11Texture>();
        if (!Texture2D->CreateTexture2D(&Device, Width, Height, DxgiFormat, Pixels, Pitch))
        {
            return nullptr;
        }

        std::shared_ptr<FTextureRenderResource> Resource =
            std::make_shared<FTextureRenderResource>();
        Resource->Width = Width;
        Resource->Height = Height;
        Resource->Format = ResolveTexturePixelFormat(CookedData.Channels);
        Resource->Texture = std::move(Texture2D);
        return Resource;
    }

    bool FTextureRenderResource::IsValid() const { return Texture != nullptr; }

    void FTextureRenderResource::Reset()
    {
        Width = 0;
        Height = 0;
        Format = ETexturePixelFormat::Unknown;
        Texture.reset();
    }

    DXGI_FORMAT FTextureRenderResource::ResolveDxgiFormat(uint32 Channels, bool bSRGB)
    {
        switch (Channels)
        {
        case 1:
            return DXGI_FORMAT_R8_UNORM;
        case 2:
            return DXGI_FORMAT_R8G8_UNORM;
        case 3:
            return bSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        case 4:
            return bSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }

    ETexturePixelFormat FTextureRenderResource::ResolveTexturePixelFormat(uint32 Channels)
    {
        switch (Channels)
        {
        case 1:
            return ETexturePixelFormat::R8;
        case 2:
            return ETexturePixelFormat::RG8;
        case 3:
            return ETexturePixelFormat::RGB8;
        case 4:
            return ETexturePixelFormat::RGBA8;
        default:
            return ETexturePixelFormat::Unknown;
        }
    }
} // namespace Asset
