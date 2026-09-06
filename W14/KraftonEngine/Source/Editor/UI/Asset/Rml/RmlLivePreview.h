#pragma once

#include "Core/Types/CoreTypes.h"

#include <d3d11.h>
#include <memory>

class FRenderer;
class FRmlRenderInterfaceD3D11;

namespace Rml
{
	class Context;
	class ElementDocument;
}

class FRmlLivePreview
{
public:
	FRmlLivePreview();
	~FRmlLivePreview();

	bool Initialize(ID3D11Device* InDevice);
	void Shutdown();

	bool LoadDocument(const FString& DocumentPath, FString* OutStatus = nullptr);
	bool ReloadDocument(FString* OutStatus = nullptr);
	void UnloadDocument();

	ID3D11ShaderResourceView* Render(FRenderer& Renderer, int32 Width, int32 Height, FString* OutStatus = nullptr);

	bool IsInitialized() const { return bInitialized; }
	bool IsDocumentLoaded() const { return Document != nullptr; }
	const FString& GetLoadedDocumentPath() const { return LoadedDocumentPath; }

private:
	bool EnsureRenderTarget(ID3D11Device* Device, int32 Width, int32 Height);
	void ReleaseRenderTarget();

private:
	ID3D11Texture2D* RenderTexture = nullptr;
	ID3D11RenderTargetView* RenderTargetView = nullptr;
	ID3D11ShaderResourceView* ShaderResourceView = nullptr;
	int32 RenderTargetWidth = 0;
	int32 RenderTargetHeight = 0;

	Rml::Context* Context = nullptr;
	Rml::ElementDocument* Document = nullptr;
	std::unique_ptr<FRmlRenderInterfaceD3D11> RenderInterface;
	FString ContextName;
	FString LoadedDocumentPath;
	bool bInitialized = false;
};
