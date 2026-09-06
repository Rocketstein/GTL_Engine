#include "Renderer/D3D11/D3D11Device.h"
#include <cassert>

void FD3D11Device::Initialize(HWND hWnd)
{
    CreateDeviceAndSwapChain(hWnd);
    CreateFrameResources();
}

void FD3D11Device::Shutdown()
{
    if (DeviceContext != nullptr)
    {
        DeviceContext->ClearState();
        DeviceContext->Flush();
    }

    ReleaseFrameResources();
    ReleaseDeviceAndSwapChain();
}

void FD3D11Device::BeginFrame()
{
    BindFrameState();
    ClearFrameBuffers();
}

void FD3D11Device::EndFrame()
{
    if (SwapChain != nullptr)
    {
        SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
}

void FD3D11Device::Resize(int32 InWidth, int32 InHeight)
{
    if (SwapChain == nullptr || DeviceContext == nullptr || InWidth <= 0 || InHeight <= 0)
    {
        return;
    }

    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    ReleaseFrameResources();

    const HRESULT Hr = SwapChain->ResizeBuffers(0, static_cast<UINT>(InWidth), static_cast<UINT>(InHeight),
                                 DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
    assert(SUCCEEDED(Hr));

    CreateFrameResources();
}

void FD3D11Device::CreateDeviceAndSwapChain(HWND hWnd)
{
    RECT ClientRect = {};
    GetClientRect(hWnd, &ClientRect);

    const UINT Width = static_cast<UINT>(ClientRect.right - ClientRect.left);
    const UINT Height = static_cast<UINT>(ClientRect.bottom - ClientRect.top);

    DXGI_SWAP_CHAIN_DESC Desc = {};
    Desc.BufferDesc.Width = (Width > 0) ? Width : 1;
    Desc.BufferDesc.Height = (Height > 0) ? Height : 1;
    Desc.BufferDesc.RefreshRate.Numerator = 0;
    Desc.BufferDesc.RefreshRate.Denominator = 1;
    Desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    Desc.BufferCount = 2;
    Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    Desc.OutputWindow = hWnd;
    Desc.Windowed = TRUE;
    Desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    UINT Flags = 0;
#if defined(_DEBUG)
    Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                         D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL CreatedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    const HRESULT Hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, Flags, FeatureLevels, ARRAYSIZE(FeatureLevels),
        D3D11_SDK_VERSION, &Desc, &SwapChain, &Device, &CreatedFeatureLevel, &DeviceContext);
    assert(SUCCEEDED(Hr));

    //UpdateViewport(Desc.BufferDesc.Width, Desc.BufferDesc.Height);
}

void FD3D11Device::ReleaseDeviceAndSwapChain()
{
    D3D11Util::SafeRelease(SwapChain);
    D3D11Util::SafeRelease(DeviceContext);
    D3D11Util::SafeRelease(Device);
}

void FD3D11Device::CreateFrameResources()
{
    CreateFrameBuffer();
    CreateDepthStencilBuffer();
    CreateDepthStencilState();
    CreateRasterizerState();

    if (SwapChain != nullptr)
    {
        DXGI_SWAP_CHAIN_DESC Desc = {};
        const HRESULT        Hr = SwapChain->GetDesc(&Desc);
        assert(SUCCEEDED(Hr));
        UpdateViewport(Desc.BufferDesc.Width, Desc.BufferDesc.Height);
    }
}

void FD3D11Device::ReleaseFrameResources()
{
    D3D11Util::SafeRelease(RasterizerState);
    D3D11Util::SafeRelease(DepthEqualDepthStencilState);
    D3D11Util::SafeRelease(DepthReadOnlyDepthStencilState);
    D3D11Util::SafeRelease(DepthStencilState);
    D3D11Util::SafeRelease(DepthSRV);
    D3D11Util::SafeRelease(DepthStencilView);
    D3D11Util::SafeRelease(DepthStencilBuffer);
    D3D11Util::SafeRelease(FrameBufferRTV);
    D3D11Util::SafeRelease(FrameBuffer);
}

void FD3D11Device::CreateFrameBuffer()
{
    assert(Device != nullptr && SwapChain != nullptr);

    HRESULT Hr =
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&FrameBuffer));
    assert(SUCCEEDED(Hr));

    D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
    RTVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    Hr = Device->CreateRenderTargetView(FrameBuffer, &RTVDesc, &FrameBufferRTV);
    assert(SUCCEEDED(Hr));
}

void FD3D11Device::CreateDepthStencilBuffer()
{
    assert(Device != nullptr && SwapChain != nullptr);

    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
    HRESULT              Hr = SwapChain->GetDesc(&SwapChainDesc);
    assert(SUCCEEDED(Hr));

    // Typeless so we can bind as both DSV and SRV (needed for Hi-Z)
    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width = SwapChainDesc.BufferDesc.Width;
    Desc.Height = SwapChainDesc.BufferDesc.Height;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    Hr = Device->CreateTexture2D(&Desc, nullptr, &DepthStencilBuffer);
    assert(SUCCEEDED(Hr));

    D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
    DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    Hr = Device->CreateDepthStencilView(DepthStencilBuffer, &DSVDesc, &DepthStencilView);
    assert(SUCCEEDED(Hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;
    Hr = Device->CreateShaderResourceView(DepthStencilBuffer, &SRVDesc, &DepthSRV);
    assert(SUCCEEDED(Hr));
}

void FD3D11Device::CreateDepthStencilState()
{
    assert(Device != nullptr);

    D3D11_DEPTH_STENCIL_DESC Desc = {};
    Desc.DepthEnable = TRUE;
    Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    Desc.StencilEnable = FALSE;

    HRESULT Hr = Device->CreateDepthStencilState(&Desc, &DepthStencilState);
    assert(SUCCEEDED(Hr));

    Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    Hr = Device->CreateDepthStencilState(&Desc, &DepthReadOnlyDepthStencilState);
    assert(SUCCEEDED(Hr));

    // Depth-equal: no writes, pass only fragments that exactly match the prepass depth
    Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    Desc.DepthFunc = D3D11_COMPARISON_EQUAL;
    Hr = Device->CreateDepthStencilState(&Desc, &DepthEqualDepthStencilState);
    assert(SUCCEEDED(Hr));
}

void FD3D11Device::CreateRasterizerState()
{
    assert(Device != nullptr);

    D3D11_RASTERIZER_DESC Desc = {};
    Desc.FillMode = D3D11_FILL_SOLID;
    Desc.CullMode = D3D11_CULL_BACK;
    Desc.DepthClipEnable = TRUE;

    const HRESULT Hr = Device->CreateRasterizerState(&Desc, &RasterizerState);
    assert(SUCCEEDED(Hr));
}

void FD3D11Device::BindFrameState()
{
    assert(DeviceContext != nullptr);

    DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);
    DeviceContext->OMSetDepthStencilState(DepthStencilState, 0);
    DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    DeviceContext->RSSetState(RasterizerState);
    DeviceContext->RSSetViewports(1, &ViewportInfo);
}

void FD3D11Device::ClearFrameBuffers()
{
    assert(DeviceContext != nullptr);

    if (FrameBufferRTV != nullptr)
    {
        DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
    }

    if (DepthStencilView != nullptr)
    {
        DeviceContext->ClearDepthStencilView(DepthStencilView,
                                             D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
}

void FD3D11Device::UpdateViewport(UINT Width, UINT Height)
{
    ViewportInfo.TopLeftX = 0.0f;
    ViewportInfo.TopLeftY = 0.0f;
    ViewportInfo.Width = static_cast<FLOAT>((Width > 0) ? Width : 1);
    ViewportInfo.Height = static_cast<FLOAT>((Height > 0) ? Height : 1);
    ViewportInfo.MinDepth = 0.0f;
    ViewportInfo.MaxDepth = 1.0f;
}
