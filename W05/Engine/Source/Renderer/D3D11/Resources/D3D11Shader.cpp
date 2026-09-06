#include "Renderer/D3D11/Resources/D3D11Shader.h"

FD3D11Shader::~FD3D11Shader() { Release(); }

bool FD3D11Shader::CompileBlob(const wchar_t *FileName, const char *EntryPoint, const char *Target,
                               ID3DBlob **OutBlob)
{
    if (OutBlob == nullptr)
    {
        return false;
    }

    *OutBlob = nullptr;
    ID3DBlob     *ErrorBlob = nullptr;
    const HRESULT Hr = D3DCompileFromFile(FileName, nullptr, nullptr, EntryPoint, Target, 0, 0,
                                          OutBlob, &ErrorBlob);
    D3D11Util::SafeRelease(ErrorBlob);
    return SUCCEEDED(Hr) && *OutBlob != nullptr;
}

bool FD3D11Shader::CompileVertexShader(ID3D11Device *Device, const wchar_t *FileName,
                                       const char *EntryPoint, const char *Target)
{
    if (Device == nullptr)
    {
        return false;
    }

    D3D11Util::SafeRelease(VertexShader);
    D3D11Util::SafeRelease(VertexShaderBlob);

    if (!CompileBlob(FileName, EntryPoint, Target, &VertexShaderBlob))
    {
        return false;
    }

    return SUCCEEDED(Device->CreateVertexShader(VertexShaderBlob->GetBufferPointer(),
                                                VertexShaderBlob->GetBufferSize(), nullptr,
                                                &VertexShader));
}

bool FD3D11Shader::CompilePixelShader(ID3D11Device *Device, const wchar_t *FileName,
                                      const char *EntryPoint, const char *Target)
{
    if (Device == nullptr)
    {
        return false;
    }

    D3D11Util::SafeRelease(PixelShader);
    D3D11Util::SafeRelease(PixelShaderBlob);

    if (!CompileBlob(FileName, EntryPoint, Target, &PixelShaderBlob))
    {
        return false;
    }

    return SUCCEEDED(Device->CreatePixelShader(PixelShaderBlob->GetBufferPointer(),
                                               PixelShaderBlob->GetBufferSize(), nullptr,
                                               &PixelShader));
}

void FD3D11Shader::Release()
{
    D3D11Util::SafeRelease(VertexShader);
    D3D11Util::SafeRelease(PixelShader);
    D3D11Util::SafeRelease(VertexShaderBlob);
    D3D11Util::SafeRelease(PixelShaderBlob);
}
