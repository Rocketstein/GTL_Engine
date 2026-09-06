#pragma once

#include <Windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

class FImGuiLayer
{
  public:
    FImGuiLayer() = default;
    ~FImGuiLayer() = default;

    bool Initialize(HWND InHwnd, ID3D11Device *InDevice, ID3D11DeviceContext *InDeviceContext);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Render();

    bool IsInitialized() const { return bInitialized; }

  private:
    bool bInitialized = false;
};
