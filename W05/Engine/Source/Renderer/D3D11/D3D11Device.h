#pragma once

#include "Renderer/D3D11/D3D11Common.h"

class FD3D11Device
{
  public:
    void Initialize(HWND hWnd);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Resize(int32 InWidth, int32 InHeight);

    ID3D11Device           *GetDevice() const { return Device; }
    ID3D11DeviceContext    *GetDeviceContext() const { return DeviceContext; }
    IDXGISwapChain         *GetSwapChain() const { return SwapChain; }
    ID3D11RenderTargetView *GetRenderTargetView() const { return FrameBufferRTV; }
    ID3D11DepthStencilView  *GetDepthStencilView() const { return DepthStencilView; }
    ID3D11ShaderResourceView*GetDepthSRV() const { return DepthSRV; }
    ID3D11DepthStencilState *GetDepthWriteState() const { return DepthStencilState; }
    ID3D11DepthStencilState *GetDepthReadOnlyState() const { return DepthReadOnlyDepthStencilState; }
    ID3D11DepthStencilState *GetDepthEqualState() const { return DepthEqualDepthStencilState; }
    const D3D11_VIEWPORT   &GetViewport() const { return ViewportInfo; }

  private:
    void CreateDeviceAndSwapChain(HWND hWnd);
    void ReleaseDeviceAndSwapChain();

    void CreateFrameResources();
    void ReleaseFrameResources();
    void CreateFrameBuffer();
    void CreateDepthStencilBuffer();
    void CreateDepthStencilState();
    void CreateRasterizerState();

    void BindFrameState();
    void ClearFrameBuffers();
    void UpdateViewport(UINT Width, UINT Height);

  private:
    ID3D11Device        *Device = nullptr;
    ID3D11DeviceContext *DeviceContext = nullptr;
    IDXGISwapChain      *SwapChain = nullptr;

    ID3D11Texture2D        *FrameBuffer = nullptr;
    ID3D11RenderTargetView *FrameBufferRTV = nullptr;

    ID3D11Texture2D          *DepthStencilBuffer = nullptr;
    ID3D11DepthStencilView   *DepthStencilView = nullptr;
    ID3D11ShaderResourceView *DepthSRV = nullptr;
    ID3D11DepthStencilState  *DepthStencilState = nullptr;
    ID3D11DepthStencilState  *DepthReadOnlyDepthStencilState = nullptr;
    ID3D11DepthStencilState  *DepthEqualDepthStencilState = nullptr;
    ID3D11RasterizerState   *RasterizerState = nullptr;

    D3D11_VIEWPORT ViewportInfo = {};
    FLOAT          ClearColor[4] = {0.025f, 0.025f, 0.025f, 1.0f};
};
