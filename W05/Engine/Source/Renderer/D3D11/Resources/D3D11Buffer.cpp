#include "Renderer/D3D11/Resources/D3D11Buffer.h"
#include "Renderer/D3D11/D3D11Device.h"
#include <cstring>

FD3D11Buffer::~FD3D11Buffer() { Release(); }

UINT FD3D11Buffer::AlignConstantBufferSize(UINT InByteWidth)
{
    return (InByteWidth + 15u) & ~15u;
}

bool FD3D11Buffer::CreateVertexBuffer(FD3D11Device *InDevice, const void *InData, UINT InByteWidth,
                                      bool bImmutable)
{
    Release();

    if (InDevice == nullptr || InData == nullptr || InByteWidth == 0)
    {
        return false;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = InByteWidth;
    Desc.Usage = bImmutable ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = InData;

    const bool bCreated = SUCCEEDED(InDevice->GetDevice()->CreateBuffer(&Desc, &InitData, &Buffer));
    if (bCreated)
    {
        BufferByteWidth = InByteWidth;
    }
    return bCreated;
}

bool FD3D11Buffer::CreateIndexBuffer(FD3D11Device *InDevice, const void *InData, UINT InByteWidth,
                                     bool bImmutable)
{
    Release();

    if (InDevice == nullptr || InData == nullptr || InByteWidth == 0)
    {
        return false;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = InByteWidth;
    Desc.Usage = bImmutable ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = InData;

    const bool bCreated = SUCCEEDED(InDevice->GetDevice()->CreateBuffer(&Desc, &InitData, &Buffer));
    if (bCreated)
    {
        BufferByteWidth = InByteWidth;
    }
    return bCreated;
}

bool FD3D11Buffer::CreateConstantBuffer(FD3D11Device *InDevice, UINT InByteWidth,
                                        const void *InitialData)
{
    Release();

    if (InDevice == nullptr || InByteWidth == 0)
    {
        return false;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = AlignConstantBufferSize(InByteWidth);
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    const bool bCreated =
        SUCCEEDED(InDevice->GetDevice()->CreateBuffer(&Desc, nullptr, &Buffer));
    if (!bCreated)
    {
        return false;
    }

    BufferByteWidth = Desc.ByteWidth;

    if (InitialData != nullptr)
    {
        return UpdateDynamicConstantBuffer(InDevice, InitialData, InByteWidth);
    }

    return true;
}

bool FD3D11Buffer::UpdateDynamicConstantBuffer(FD3D11Device *InDevice, const void *InData,
                                               UINT InByteWidth)
{
    if (InDevice == nullptr || Buffer == nullptr || InData == nullptr || InByteWidth == 0)
    {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (FAILED(InDevice->GetDeviceContext()->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        return false;
    }

    const UINT CopyByteWidth = (InByteWidth < BufferByteWidth) ? InByteWidth : BufferByteWidth;
    std::memcpy(Mapped.pData, InData, CopyByteWidth);

    if (CopyByteWidth < BufferByteWidth)
    {
        std::memset(static_cast<unsigned char *>(Mapped.pData) + CopyByteWidth, 0,
                    BufferByteWidth - CopyByteWidth);
    }

    InDevice->GetDeviceContext()->Unmap(Buffer, 0);
    return true;
}

void FD3D11Buffer::Release()
{
    D3D11Util::SafeRelease(Buffer);
    BufferByteWidth = 0;
}
