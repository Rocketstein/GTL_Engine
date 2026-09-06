#include "Viewport/Viewport.h"

#include "Render/Resource/Buffer.h"

#include <cstdio>
#include <cstring>

namespace
{
	constexpr DXGI_FORMAT SceneColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	void SetDebugName(ID3D11DeviceChild* Resource, const char* Name)
	{
		if (!Resource || !Name)
		{
			return;
		}

		Resource->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen(Name)), Name);
	}
}

FViewport::~FViewport()
{
	ReleaseResources();
}

bool FViewport::Initialize(ID3D11Device* InDevice, uint32 InWidth, uint32 InHeight)
{
	Device = InDevice;
	Width = InWidth;
	Height = InHeight;

	return CreateResources();
}

void FViewport::Release()
{
	ReleaseResources();
	Device = nullptr;
	Width = 0;
	Height = 0;
}

void FViewport::Resize(uint32 InWidth, uint32 InHeight)
{
	if (InWidth == 0 || InHeight == 0) return;
	if (InWidth == Width && InHeight == Height) return;

	Width = InWidth;
	Height = InHeight;

	ReleaseResources();
	CreateResources();
}

void FViewport::RequestResize(uint32 InWidth, uint32 InHeight)
{
	if (InWidth == 0 || InHeight == 0) return;
	if (InWidth == Width && InHeight == Height)
	{
		bPendingResize = false;
		return;
	}

	PendingWidth = InWidth;
	PendingHeight = InHeight;
	bPendingResize = true;
}

bool FViewport::ApplyPendingResize()
{
	if (!bPendingResize) return false;

	bPendingResize = false;
	Resize(PendingWidth, PendingHeight);
	return true;
}

void FViewport::BeginRender(ID3D11DeviceContext* Ctx, const float ClearColor[4])
{
	if (!RTV) return;

	const float DefaultColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
	const float* Color = ClearColor ? ClearColor : DefaultColor;
	D3D11_VIEWPORT VPRect = GetViewportRect();

	Ctx->ClearRenderTargetView(RTV, Color);
	if (NormalRTV)
	{
		const float NormalClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		Ctx->ClearRenderTargetView(NormalRTV, NormalClear);
	}
	if (CullingHeatmapRTV)
	{
		const float HeatmapClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		Ctx->ClearRenderTargetView(CullingHeatmapRTV, HeatmapClear);
	}
	Ctx->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
	Ctx->OMSetRenderTargets(1, &RTV, DSV);
	Ctx->RSSetViewports(1, &VPRect);
}

bool FViewport::CreateResources()
{
	if (!Device || Width == 0 || Height == 0) return false;

	// ── HDR SceneColor 렌더 타깃 텍스처 ──
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = Width;
	TexDesc.Height = Height;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = SceneColorFormat;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &RTTexture);
	if (FAILED(hr)) return false;
	RTTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorTexture")), "ViewportSceneColorTexture");

	hr = Device->CreateRenderTargetView(RTTexture, nullptr, &RTV);
	if (FAILED(hr)) return false;
	RTV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorRTV")), "ViewportSceneColorRTV");

	hr = Device->CreateShaderResourceView(RTTexture, nullptr, &SRV);
	if (FAILED(hr)) return false;
	SRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorSRV")), "ViewportSceneColorSRV");

	// ── SceneColor 복사 텍스처 (FXAA 등 PostProcess용 CopyResource 대상) ──
	D3D11_TEXTURE2D_DESC SceneColorCopyDesc = TexDesc;
	SceneColorCopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;  // SRV 읽기 전용
	hr = Device->CreateTexture2D(&SceneColorCopyDesc, nullptr, &SceneColorCopyTexture);
	if (FAILED(hr)) return false;
	SceneColorCopyTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorCopyTexture")), "ViewportSceneColorCopyTexture");

	// ── 뎁스/스텐실 (TYPELESS → DSV + StencilSRV) ──
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = Width;
	DepthDesc.Height = Height;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	hr = Device->CreateTexture2D(&DepthDesc, nullptr, &DepthTexture);
	if (FAILED(hr)) return false;
	DepthTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDepthTexture")), "ViewportDepthTexture");

	// DSV: D24_UNORM_S8_UINT 로 해석 (기존과 동일한 뎁스/스텐실 동작)
	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
	DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;

	hr = Device->CreateDepthStencilView(DepthTexture, &DSVDesc, &DSV);
	if (FAILED(hr)) return false;
	DSV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDSV")), "ViewportDSV");

	// SRV 포맷 (DepthCopy/StencilCopy 생성에 재사용)
	D3D11_SHADER_RESOURCE_VIEW_DESC DepthSRVDesc = {};
	DepthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	DepthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	DepthSRVDesc.Texture2D.MipLevels = 1;
	DepthSRVDesc.Texture2D.MostDetailedMip = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC StencilSRVDesc = {};
	StencilSRVDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
	StencilSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	StencilSRVDesc.Texture2D.MipLevels = 1;
	StencilSRVDesc.Texture2D.MostDetailedMip = 0;

	// ── Depth 복사 텍스처 (CopyResource 대상, SRV 전용) ──
	D3D11_TEXTURE2D_DESC CopyDesc = {};
	CopyDesc.Width = Width;
	CopyDesc.Height = Height;
	CopyDesc.MipLevels = 1;
	CopyDesc.ArraySize = 1;
	CopyDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	CopyDesc.SampleDesc.Count = 1;
	CopyDesc.Usage = D3D11_USAGE_DEFAULT;
	CopyDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	hr = Device->CreateTexture2D(&CopyDesc, nullptr, &DepthCopyTexture);
	if (FAILED(hr)) return false;
	DepthCopyTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDepthCopyTexture")), "ViewportDepthCopyTexture");

	hr = Device->CreateShaderResourceView(DepthCopyTexture, &DepthSRVDesc, &DepthCopySRV);
	if (FAILED(hr)) return false;
	DepthCopySRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDepthCopySRV")), "ViewportDepthCopySRV");

	hr = Device->CreateShaderResourceView(DepthCopyTexture, &StencilSRVDesc, &StencilCopySRV);
	if (FAILED(hr)) return false;
	StencilCopySRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportStencilCopySRV")), "ViewportStencilCopySRV");

	D3D11_TEXTURE2D_DESC OcclusionCopyDesc = CopyDesc;
	OcclusionCopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	hr = Device->CreateTexture2D(&OcclusionCopyDesc, nullptr, &OcclusionDepthCopyTexture);
	if (FAILED(hr)) return false;
	OcclusionDepthCopyTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportOcclusionDepthCopyTexture")), "ViewportOcclusionDepthCopyTexture");

	hr = Device->CreateShaderResourceView(OcclusionDepthCopyTexture, &DepthSRVDesc, &OcclusionDepthCopySRV);
	if (FAILED(hr)) return false;
	OcclusionDepthCopySRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportOcclusionDepthCopySRV")), "ViewportOcclusionDepthCopySRV");

	D3D11_SHADER_RESOURCE_VIEW_DESC SceneColorCopySRVDesc = {};
	SceneColorCopySRVDesc.Format = SceneColorFormat;
	SceneColorCopySRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SceneColorCopySRVDesc.Texture2D.MipLevels = 1;
	SceneColorCopySRVDesc.Texture2D.MostDetailedMip = 0;

	hr = Device->CreateShaderResourceView(SceneColorCopyTexture, &SceneColorCopySRVDesc, &SceneColorCopySRV);
	if (FAILED(hr)) return false;
	SceneColorCopySRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorCopySRV")), "ViewportSceneColorCopySRV");

	// ── GBuffer Normal RT (R16G16B16A16_FLOAT — 음수 지원) ──
	D3D11_TEXTURE2D_DESC NormalDesc = {};
	NormalDesc.Width = Width;
	NormalDesc.Height = Height;
	NormalDesc.MipLevels = 1;
	NormalDesc.ArraySize = 1;
	NormalDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	NormalDesc.SampleDesc.Count = 1;
	NormalDesc.Usage = D3D11_USAGE_DEFAULT;
	NormalDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = Device->CreateTexture2D(&NormalDesc, nullptr, &NormalTexture);
	if (FAILED(hr)) return false;
	NormalTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportNormalTexture")), "ViewportNormalTexture");

	hr = Device->CreateRenderTargetView(NormalTexture, nullptr, &NormalRTV);
	if (FAILED(hr)) return false;
	NormalRTV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportNormalRTV")), "ViewportNormalRTV");

	hr = Device->CreateShaderResourceView(NormalTexture, nullptr, &NormalSRV);
	if (FAILED(hr)) return false;
	NormalSRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportNormalSRV")), "ViewportNormalSRV");

	// ── Culling Heatmap RT (R8G8B8A8_UNORM — 히트맵 색상) ──
	D3D11_TEXTURE2D_DESC HeatmapDesc = {};
	HeatmapDesc.Width = Width;
	HeatmapDesc.Height = Height;
	HeatmapDesc.MipLevels = 1;
	HeatmapDesc.ArraySize = 1;
	HeatmapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	HeatmapDesc.SampleDesc.Count = 1;
	HeatmapDesc.Usage = D3D11_USAGE_DEFAULT;
	HeatmapDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = Device->CreateTexture2D(&HeatmapDesc, nullptr, &CullingHeatmapTexture);
	if (FAILED(hr)) return false;
	CullingHeatmapTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportCullingHeatmapTexture")), "ViewportCullingHeatmapTexture");

	hr = Device->CreateRenderTargetView(CullingHeatmapTexture, nullptr, &CullingHeatmapRTV);
	if (FAILED(hr)) return false;
	CullingHeatmapRTV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportCullingHeatmapRTV")), "ViewportCullingHeatmapRTV");

	hr = Device->CreateShaderResourceView(CullingHeatmapTexture, nullptr, &CullingHeatmapSRV);
	if (FAILED(hr)) return false;
	CullingHeatmapSRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportCullingHeatmapSRV")), "ViewportCullingHeatmapSRV");

	if (!CreateBloomResources())
	{
		return false;
	}

	if (!CreateDOFResources())
	{
		return false;
	}

	// ── 뷰포트 렉트 ──
	ViewportRect.TopLeftX = 0.0f;
	ViewportRect.TopLeftY = 0.0f;
	ViewportRect.Width = static_cast<float>(Width);
	ViewportRect.Height = static_cast<float>(Height);
	ViewportRect.MinDepth = 0.0f;
	ViewportRect.MaxDepth = 1.0f;

	return true;
}

bool FViewport::CreateBloomResources()
{
	ReleaseBloomResources();

	for (uint32 MipIndex = 0; MipIndex < EBloom::MaxMipCount; ++MipIndex)
	{
		uint32 MipWidth = Width >> (MipIndex + 1);
		uint32 MipHeight = Height >> (MipIndex + 1);
		if (MipWidth == 0) MipWidth = 1;
		if (MipHeight == 0) MipHeight = 1;

		char DebugName[64] = {};
		std::snprintf(DebugName, sizeof(DebugName), "ViewportBloomMip%u", MipIndex);
		if (!CreateBloomMip(BloomResources.Mips[MipIndex], MipWidth, MipHeight, DebugName))
		{
			ReleaseBloomResources();
			return false;
		}

		std::snprintf(DebugName, sizeof(DebugName), "ViewportBloomTempMip%u", MipIndex);
		if (!CreateBloomMip(BloomResources.TempMips[MipIndex], MipWidth, MipHeight, DebugName))
		{
			ReleaseBloomResources();
			return false;
		}
	}

	return true;
}

void FViewport::ReleaseBloomResources()
{
	for (uint32 MipIndex = 0; MipIndex < EBloom::MaxMipCount; ++MipIndex)
	{
		ReleaseBloomMip(BloomResources.Mips[MipIndex]);
		ReleaseBloomMip(BloomResources.TempMips[MipIndex]);
	}
}

bool FViewport::CreateBloomMip(FBloomMipResource& OutResource, uint32 InWidth, uint32 InHeight, const char* DebugName)
{
	ReleaseBloomMip(OutResource);

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = InWidth;
	Desc.Height = InHeight;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = EBloom::Format;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, &OutResource.Texture);
	if (FAILED(hr))
	{
		return false;
	}
	SetDebugName(OutResource.Texture, DebugName);

	hr = Device->CreateRenderTargetView(OutResource.Texture, nullptr, &OutResource.RTV);
	if (FAILED(hr))
	{
		ReleaseBloomMip(OutResource);
		return false;
	}
	SetDebugName(OutResource.RTV, DebugName);

	hr = Device->CreateShaderResourceView(OutResource.Texture, nullptr, &OutResource.SRV);
	if (FAILED(hr))
	{
		ReleaseBloomMip(OutResource);
		return false;
	}
	SetDebugName(OutResource.SRV, DebugName);

	OutResource.Width = InWidth;
	OutResource.Height = InHeight;
	return true;
}

void FViewport::ReleaseBloomMip(FBloomMipResource& Resource)
{
	if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	if (Resource.RTV) { Resource.RTV->Release(); Resource.RTV = nullptr; }
	if (Resource.Texture) { Resource.Texture->Release(); Resource.Texture = nullptr; }
	Resource.Width = 0;
	Resource.Height = 0;
}

bool FViewport::CreateDOFResources()
{
	ReleaseDOFResources();

	// CoC/Blur run at reduced resolution; Composite upscales the blurred layer.
	uint32 DOFWidth = ceil(Width / EDOF::Downscale);
	uint32 DOFHeight = ceil(Height / EDOF::Downscale);
	if (DOFWidth == 0) DOFWidth = 1;
	if (DOFHeight == 0) DOFHeight = 1;

	if (!CreateDOFResource(DOFResources.CoCResources, DOFWidth, DOFHeight, "ViewportDOFCoC"))
	{
		ReleaseDOFResources();
		return false;
	}

	if (!CreateDOFResource(DOFResources.BlurResources, DOFWidth, DOFHeight, "ViewportDOFBlur"))
	{
		ReleaseDOFResources();
		return false;
	}

	return true;
}

void FViewport::ReleaseDOFResources()
{
	ReleaseDOFResource(DOFResources.CoCResources);
	ReleaseDOFResource(DOFResources.BlurResources);
}

bool FViewport::CreateDOFResource(FDOFResources& OutResource, uint32 InWidth, uint32 InHeight, const char* DebugName)
{
	ReleaseDOFResource(OutResource);

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = InWidth;
	Desc.Height = InHeight;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = SceneColorFormat;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, &OutResource.Texture);
	if (FAILED(hr))
	{
		return false;
	}
	SetDebugName(OutResource.Texture, DebugName);

	hr = Device->CreateRenderTargetView(OutResource.Texture, nullptr, &OutResource.RTV);
	if (FAILED(hr))
	{
		ReleaseDOFResource(OutResource);
		return false;
	}
	SetDebugName(OutResource.RTV, DebugName);

	hr = Device->CreateShaderResourceView(OutResource.Texture, nullptr, &OutResource.SRV);
	if (FAILED(hr))
	{
		ReleaseDOFResource(OutResource);
		return false;
	}
	SetDebugName(OutResource.SRV, DebugName);

	OutResource.Width = InWidth;
	OutResource.Height = InHeight;
	return true;
}

void FViewport::ReleaseDOFResource(FDOFResources& Resource)
{
	if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	if (Resource.RTV) { Resource.RTV->Release(); Resource.RTV = nullptr; }
	if (Resource.Texture) { Resource.Texture->Release(); Resource.Texture = nullptr; }
	Resource.Width = 0;
	Resource.Height = 0;
}

void FViewport::ReleaseResources()
{
	ReleaseBloomResources();
	ReleaseDOFResources();

	if (CullingHeatmapSRV) { CullingHeatmapSRV->Release(); CullingHeatmapSRV = nullptr; }
	if (CullingHeatmapRTV) { CullingHeatmapRTV->Release(); CullingHeatmapRTV = nullptr; }
	if (CullingHeatmapTexture) { CullingHeatmapTexture->Release(); CullingHeatmapTexture = nullptr; }
	if (NormalSRV) { NormalSRV->Release(); NormalSRV = nullptr; }
	if (NormalRTV) { NormalRTV->Release(); NormalRTV = nullptr; }
	if (NormalTexture) { NormalTexture->Release(); NormalTexture = nullptr; }
	if (StencilCopySRV) { StencilCopySRV->Release(); StencilCopySRV = nullptr; }
	if (DepthCopySRV) { DepthCopySRV->Release(); DepthCopySRV = nullptr; }
	if (DepthCopyTexture) { DepthCopyTexture->Release(); DepthCopyTexture = nullptr; }
	if (OcclusionDepthCopySRV) { OcclusionDepthCopySRV->Release(); OcclusionDepthCopySRV = nullptr; }
	if (OcclusionDepthCopyTexture) { OcclusionDepthCopyTexture->Release(); OcclusionDepthCopyTexture = nullptr; }
	if (DSV) { DSV->Release(); DSV = nullptr; }
	if (DepthTexture) { DepthTexture->Release(); DepthTexture = nullptr; }
	if (SRV) { SRV->Release(); SRV = nullptr; }
	if (RTV) { RTV->Release(); RTV = nullptr; }
	if (RTTexture) { RTTexture->Release(); RTTexture = nullptr; }
	if (SceneColorCopySRV) { SceneColorCopySRV->Release(); SceneColorCopySRV = nullptr; }
	if (SceneColorCopyTexture) { SceneColorCopyTexture->Release(); SceneColorCopyTexture = nullptr; }
}
