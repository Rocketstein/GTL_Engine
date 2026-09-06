#include "Editor/UI/Asset/Rml/RmlLivePreview.h"

#include "Platform/Paths.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/RenderPass/RenderPassBase.h"
#include "UI/UIManager.h"

#ifdef GetNextSibling
#undef GetNextSibling
#endif
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace
{
	std::filesystem::path ToProjectPath(const FString& Path)
	{
		std::filesystem::path Result(FPaths::ToWide(Path));
		if (Result.is_relative())
		{
			Result = std::filesystem::path(FPaths::RootDir()) / Result;
		}
		return Result.lexically_normal();
	}

	Rml::String ToRmlPath(const std::filesystem::path& Path)
	{
		return FPaths::ToUtf8(Path.generic_wstring());
	}
}

FRmlLivePreview::FRmlLivePreview()
{
	char Buffer[64] = {};
	std::snprintf(Buffer, sizeof(Buffer), "RmlWidgetPreview_%p", static_cast<void*>(this));
	ContextName = Buffer;
}

FRmlLivePreview::~FRmlLivePreview()
{
	Shutdown();
}

bool FRmlLivePreview::Initialize(ID3D11Device* InDevice)
{
	if (bInitialized)
	{
		return true;
	}

	if (!InDevice)
	{
		return false;
	}

	RenderInterface = std::make_unique<FRmlRenderInterfaceD3D11>(InDevice);
	Context = Rml::CreateContext(ContextName, Rml::Vector2i(1, 1), RenderInterface.get());
	if (!Context)
	{
		RenderInterface.reset();
		return false;
	}

	bInitialized = true;
	return true;
}

void FRmlLivePreview::Shutdown()
{
	UnloadDocument();

	if (Context)
	{
		Rml::RemoveContext(ContextName);
		Context = nullptr;
	}

	RenderInterface.reset();
	ReleaseRenderTarget();
	bInitialized = false;
}

bool FRmlLivePreview::LoadDocument(const FString& DocumentPath, FString* OutStatus)
{
	if (!Context)
	{
		if (OutStatus)
		{
			*OutStatus = "Live preview context is not initialized.";
		}
		return false;
	}

	if (DocumentPath.empty())
	{
		if (OutStatus)
		{
			*OutStatus = "Live preview needs an RML document. Open the .rml file, or keep a same-name .rml next to the .rcss.";
		}
		return false;
	}

	const std::filesystem::path FullPath = ToProjectPath(DocumentPath);
	if (!std::filesystem::exists(FullPath))
	{
		if (OutStatus)
		{
			*OutStatus = "Live preview document not found: " + DocumentPath;
		}
		return false;
	}

	UnloadDocument();
	Rml::Factory::ClearStyleSheetCache();

	Document = Context->LoadDocument(ToRmlPath(FullPath));
	if (!Document)
	{
		if (OutStatus)
		{
			*OutStatus = "Live preview failed to load: " + DocumentPath;
		}
		return false;
	}

	Document->Show();
	LoadedDocumentPath = DocumentPath;
	if (OutStatus)
	{
		*OutStatus = "Live preview loaded: " + DocumentPath;
	}
	return true;
}

bool FRmlLivePreview::ReloadDocument(FString* OutStatus)
{
	if (LoadedDocumentPath.empty())
	{
		return false;
	}

	const FString Path = LoadedDocumentPath;
	return LoadDocument(Path, OutStatus);
}

void FRmlLivePreview::UnloadDocument()
{
	if (Document)
	{
		Document->Close();
		Document = nullptr;
	}
	LoadedDocumentPath.clear();
}

ID3D11ShaderResourceView* FRmlLivePreview::Render(FRenderer& Renderer, int32 Width, int32 Height, FString* OutStatus)
{
	Width = (std::max)(1, Width);
	Height = (std::max)(1, Height);

	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
	ID3D11DeviceContext* DC = Renderer.GetFD3DDevice().GetDeviceContext();
	if (!Device || !DC || !Context || !Document || !RenderInterface)
	{
		return nullptr;
	}

	ID3D11ShaderResourceView* NullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
	DC->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, NullSRVs);

	if (!EnsureRenderTarget(Device, Width, Height))
	{
		if (OutStatus)
		{
			*OutStatus = "Live preview render target creation failed.";
		}
		return nullptr;
	}

	ID3D11RenderTargetView* OldRTV = nullptr;
	ID3D11DepthStencilView* OldDSV = nullptr;
	DC->OMGetRenderTargets(1, &OldRTV, &OldDSV);

	D3D11_VIEWPORT OldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
	UINT OldViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	DC->RSGetViewports(&OldViewportCount, OldViewports);

	const float ClearColor[4] = { 0.024f, 0.024f, 0.028f, 1.0f };
	DC->OMSetRenderTargets(1, &RenderTargetView, nullptr);
	DC->ClearRenderTargetView(RenderTargetView, ClearColor);

	FFrameContext Frame;
	Frame.ViewportWidth = static_cast<float>(Width);
	Frame.ViewportHeight = static_cast<float>(Height);
	Frame.ViewportRTV = RenderTargetView;
	Frame.ViewportDSV = nullptr;

	FStateCache Cache;
	Cache.Reset();
	Cache.RTV = RenderTargetView;
	Cache.DSV = nullptr;

	FDrawCommandList EmptyCommandList;
	FPassContext PassContext{
		Renderer.GetFD3DDevice(),
		Frame,
		Cache,
		Renderer.GetResources(),
		EmptyCommandList,
		&Renderer,
		nullptr,
		nullptr
	};

	Context->SetDimensions({ Width, Height });
	Context->Update();
	RenderInterface->BeginFrame(PassContext);
	Context->Render();
	RenderInterface->EndFrame();

	Cache.Cleanup(DC);
	DC->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, NullSRVs);
	DC->OMSetRenderTargets(1, &OldRTV, OldDSV);
	if (OldViewportCount > 0)
	{
		DC->RSSetViewports(OldViewportCount, OldViewports);
	}

	if (OldRTV) OldRTV->Release();
	if (OldDSV) OldDSV->Release();

	return ShaderResourceView;
}

bool FRmlLivePreview::EnsureRenderTarget(ID3D11Device* Device, int32 Width, int32 Height)
{
	if (RenderTexture && RenderTargetWidth == Width && RenderTargetHeight == Height)
	{
		return true;
	}

	ReleaseRenderTarget();

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = static_cast<UINT>(Width);
	Desc.Height = static_cast<UINT>(Height);
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&Desc, nullptr, &RenderTexture)) || !RenderTexture)
	{
		ReleaseRenderTarget();
		return false;
	}

	if (FAILED(Device->CreateRenderTargetView(RenderTexture, nullptr, &RenderTargetView)) || !RenderTargetView)
	{
		ReleaseRenderTarget();
		return false;
	}

	if (FAILED(Device->CreateShaderResourceView(RenderTexture, nullptr, &ShaderResourceView)) || !ShaderResourceView)
	{
		ReleaseRenderTarget();
		return false;
	}

	RenderTargetWidth = Width;
	RenderTargetHeight = Height;
	return true;
}

void FRmlLivePreview::ReleaseRenderTarget()
{
	if (ShaderResourceView)
	{
		ShaderResourceView->Release();
		ShaderResourceView = nullptr;
	}
	if (RenderTargetView)
	{
		RenderTargetView->Release();
		RenderTargetView = nullptr;
	}
	if (RenderTexture)
	{
		RenderTexture->Release();
		RenderTexture = nullptr;
	}
	RenderTargetWidth = 0;
	RenderTargetHeight = 0;
}
