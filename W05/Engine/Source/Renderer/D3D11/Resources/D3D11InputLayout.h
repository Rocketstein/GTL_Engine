#pragma once

#include "Renderer/D3D11/D3D11Common.h"

class FD3D11InputLayout
{
  public:
    ~FD3D11InputLayout();

    bool Create(ID3D11Device *Device, const D3D11_INPUT_ELEMENT_DESC *Layout, UINT LayoutCount,
                const void *ShaderBytecode, SIZE_T BytecodeLength);
    void Release();

    ID3D11InputLayout *Get() const { return InputLayout; }

  private:
    ID3D11InputLayout *InputLayout = nullptr;
};
