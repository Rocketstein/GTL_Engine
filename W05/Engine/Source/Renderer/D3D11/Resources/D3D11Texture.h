#pragma once

#include "Renderer/D3D11/D3D11Common.h"

class FD3D11Device;

class FD3D11Texture
{
  public:
    ~FD3D11Texture();

    bool CreateTexture2D(FD3D11Device *InDevice, UINT InWidth, UINT InHeight, DXGI_FORMAT InFormat,
                         const void *InPixels, UINT InPitch);
    void Release();

    ID3D11Texture2D          *GetTexture() const { return Texture; }
    ID3D11ShaderResourceView *GetSRV() const { return SRV; }

  private:
    ID3D11Texture2D          *Texture = nullptr;
    ID3D11ShaderResourceView *SRV = nullptr;
};
