#pragma once

#include "Renderer/D3D11/D3D11Common.h"

class FD3D11Device;

class FD3D11Buffer
{
  public:
    FD3D11Buffer() = default;
    ~FD3D11Buffer();

    bool CreateVertexBuffer(FD3D11Device *InDevice, const void *InData, UINT InByteWidth,
                            bool bImmutable = true);
    bool CreateIndexBuffer(FD3D11Device *InDevice, const void *InData, UINT InByteWidth,
                           bool bImmutable = true);
    bool CreateConstantBuffer(FD3D11Device *InDevice, UINT InByteWidth,
                              const void *InitialData = nullptr);
    bool UpdateDynamicConstantBuffer(FD3D11Device *InDevice, const void *InData, UINT InByteWidth);
    void Release();

    ID3D11Buffer *Get() const { return Buffer; }

  private:
    static UINT AlignConstantBufferSize(UINT InByteWidth);

  private:
    ID3D11Buffer *Buffer = nullptr;
    UINT          BufferByteWidth = 0;
};
