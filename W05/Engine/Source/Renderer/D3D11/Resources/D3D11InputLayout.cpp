#include "Renderer/D3D11/Resources/D3D11InputLayout.h"

FD3D11InputLayout::~FD3D11InputLayout() { Release(); }

bool FD3D11InputLayout::Create(ID3D11Device *Device, const D3D11_INPUT_ELEMENT_DESC *Layout,
                               UINT LayoutCount, const void *ShaderBytecode, SIZE_T BytecodeLength)
{
    Release();

    if (Device == nullptr || Layout == nullptr || LayoutCount == 0 || ShaderBytecode == nullptr ||
        BytecodeLength == 0)
    {
        return false;
    }

    return SUCCEEDED(Device->CreateInputLayout(Layout, LayoutCount, ShaderBytecode, BytecodeLength,
                                               &InputLayout));
}

void FD3D11InputLayout::Release() { D3D11Util::SafeRelease(InputLayout); }
