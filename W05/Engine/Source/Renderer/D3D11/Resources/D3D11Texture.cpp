#include "Renderer/D3D11/Resources/D3D11Texture.h"
#include "Renderer/D3D11/D3D11Device.h"

FD3D11Texture::~FD3D11Texture() { Release(); }

bool FD3D11Texture::CreateTexture2D(FD3D11Device *InDevice, UINT InWidth, UINT InHeight,
                                    DXGI_FORMAT InFormat, const void *InPixels, UINT InPitch)
{
    Release();

    if (InDevice == nullptr || InPixels == nullptr || InWidth == 0 || InHeight == 0 || InPitch == 0)
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width = InWidth;
    Desc.Height = InHeight;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = InFormat;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitialData = {};
    InitialData.pSysMem = InPixels;
    InitialData.SysMemPitch = InPitch;

    if (FAILED(InDevice->GetDevice()->CreateTexture2D(&Desc, &InitialData, &Texture)))
    {
        Release();
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Format = Desc.Format;
    SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MipLevels = 1;

    if (FAILED(InDevice->GetDevice()->CreateShaderResourceView(Texture, &SrvDesc, &SRV)))
    {
        Release();
        return false;
    }

    return true;
}

void FD3D11Texture::Release()
{
    D3D11Util::SafeRelease(SRV);
    D3D11Util::SafeRelease(Texture);
}
