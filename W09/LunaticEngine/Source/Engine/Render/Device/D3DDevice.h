#pragma once
#include "Render/Types/RenderTypes.h"
#include "Core/CoreTypes.h"

class FD3DDevice
{
public:
	FD3DDevice() = default;

	void Create(HWND InHWindow);
	void Release();

	// 스왑체인 백버퍼 복귀 — RTV/DSV 클리어 + 뷰포트 세팅
	void BeginFrame();
	void Present();
	void OnResizeViewport(int width, int height);

	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetDeviceContext() const;

	// 오프스크린 텍스처를 스왑체인 백버퍼로 복사 — Game/Shipping 모드에서 ImGui 합성 없이 화면에 출력하기 위해 사용.
	void CopyToBackbuffer(ID3D11Texture2D* Source);
	uint32 GetBackbufferWidth() const { return static_cast<uint32>(ViewportInfo.Width); }
	uint32 GetBackbufferHeight() const { return static_cast<uint32>(ViewportInfo.Height); }

private:
	void CreateDeviceAndSwapChain(HWND InHWindow);
	void ReleaseDeviceAndSwapChain();

	void CreateFrameBuffer();
	void ReleaseFrameBuffer();

	void CreateDepthStencilBuffer();
	void ReleaseDepthStencilBuffer();

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	// --- SwapChain BackBuffer ---
	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;

	ID3D11Texture2D* DepthStencilBuffer = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;

	D3D11_VIEWPORT ViewportInfo = {};
	const float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	BOOL bTearingSupported = FALSE;
	UINT SwapChainFlags = 0;
};
