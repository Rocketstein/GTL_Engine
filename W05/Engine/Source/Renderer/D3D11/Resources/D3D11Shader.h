#pragma once

#include "Renderer/D3D11/D3D11Common.h"

class FD3D11Shader
{
  public:
    ~FD3D11Shader();

    bool CompileVertexShader(ID3D11Device *Device, const wchar_t *FileName, const char *EntryPoint,
                             const char *Target);
    bool CompilePixelShader(ID3D11Device *Device, const wchar_t *FileName, const char *EntryPoint,
                            const char *Target);
    void Release();

    ID3D11VertexShader *GetVertexShader() const { return VertexShader; }
    ID3D11PixelShader  *GetPixelShader() const { return PixelShader; }
    ID3DBlob           *GetVertexShaderBlob() const { return VertexShaderBlob; }

  private:
    bool CompileBlob(const wchar_t *FileName, const char *EntryPoint, const char *Target,
                     ID3DBlob **OutBlob);

  private:
    ID3D11VertexShader *VertexShader = nullptr;
    ID3D11PixelShader  *PixelShader = nullptr;
    ID3DBlob           *VertexShaderBlob = nullptr;
    ID3DBlob           *PixelShaderBlob = nullptr;
};
