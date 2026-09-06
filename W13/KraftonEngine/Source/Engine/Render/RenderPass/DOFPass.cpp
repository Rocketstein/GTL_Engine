#include "DOFPass.h"
#include "RenderPassRegistry.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Resource/RenderResources.h"

REGISTER_RENDER_PASS(FDOFPass)

namespace 
{
	// Shared constant buffer for all DOF passes (CoC / Blur / Composite).
	struct FDOFCB
	{
		float Aperture;
		float FocusDist;
		float FocalLength;
		int   DOFSample;
		float CameraNear;
		float CameraFar;
		float SourceTexelSize[2];
	};

	void UnBindSRVSingle(ID3D11DeviceContext* DC)
	{
		ID3D11ShaderResourceView* NullSRV = nullptr;
		DC->VSSetShaderResources(0, 1, &NullSRV);
		DC->PSSetShaderResources(0, 1, &NullSRV);
	}

	void UnBindSRVDouble(ID3D11DeviceContext* DC)
	{
		ID3D11ShaderResourceView* NullSRVs[2] = {};
		DC->VSSetShaderResources(0, 2, NullSRVs);
		DC->PSSetShaderResources(0, 2, NullSRVs);
	}

	void UnBindSRVTriple(ID3D11DeviceContext* DC)
	{
		ID3D11ShaderResourceView* NullSRVs[3] = {};
		DC->VSSetShaderResources(0, 3, NullSRVs);
		DC->PSSetShaderResources(0, 3, NullSRVs);
	}

	// Pass to shared hlsli?
	void BindDOFCB(ID3D11DeviceContext* DC, FConstantBuffer& Buffer)
	{
		ID3D11Buffer* RawCB = Buffer.GetBuffer();
		DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
		DC->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
	}

	void DrawFullscreen(ID3D11DeviceContext* DC, FShader* Shader)
	{
		Shader->Bind(DC);
		DC->Draw(3, 0);
	}

	void SetViewport(ID3D11DeviceContext* DC, uint32 Width, uint32 Height)
	{
		D3D11_VIEWPORT Viewport = {};
		Viewport.TopLeftX = 0.0f;
		Viewport.TopLeftY = 0.0f;
		Viewport.Width = static_cast<float>(Width);
		Viewport.Height = static_cast<float>(Height);
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
		DC->RSSetViewports(1, &Viewport);
	}

} // anonymous namespace

FDOFPass::FDOFPass()
{
	PassType = ERenderPass::DepthOfField;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::Opaque,
					ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}

bool FDOFPass::BeginPass(const FPassContext& Ctx) 
{
	const FFrameContext& FrameContext = Ctx.Frame;
	const FViewportRenderOptions& RenderOptions = FrameContext.RenderOptions;
	if (RenderOptions.ShowFlags.bDOF == false || RenderOptions.PostProcessSettings.Aperture <= 0.001f)		return false;
	if (!FrameContext.SceneColorCopyTexture || !FrameContext.SceneColorCopySRV ||
		!FrameContext.ViewportRenderTexture || !FrameContext.ViewportRTV)			return false;
	if (FrameContext.ViewportWidth <= 0.0f || FrameContext.ViewportHeight <= 0.0f)	return false;

	// Ensure Constant Buffer
	if (!DOFCB.GetBuffer())
	{
		DOFCB.Create(Ctx.Device.GetDevice(), sizeof(FDOFCB), "DOFCB");
		if (!DOFCB.GetBuffer()) return false;
	}

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	// Safeguard
	UnBindSRVDouble(DC);
	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->CopyResource(FrameContext.SceneColorCopyTexture, FrameContext.ViewportRenderTexture);
	DC->CopyResource(FrameContext.DepthCopyTexture, FrameContext.DepthTexture);

	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::NoDepth);
	Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Opaque);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidNoCull);

	ID3D11Buffer* NullVB = nullptr;
	uint32 Zero = 0;
	DC->IASetVertexBuffers(0, 1, &NullVB, &Zero, &Zero);
	DC->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Ctx.Cache.bForceAll = true;
	return true;
}

void FDOFPass::Execute(const FPassContext& Ctx)
{
	FShader* CoCShader = FShaderManager::Get().GetOrCreate(EShaderPath::DOFCoC);
	FShader* BlurShader = FShaderManager::Get().GetOrCreate(EShaderPath::DOFBlur);
	FShader* CompositeShader = FShaderManager::Get().GetOrCreate(EShaderPath::DOFComposite);

	if (!CoCShader || !CoCShader->IsValid() ||
		!BlurShader || !BlurShader->IsValid() ||
		!CompositeShader || !CompositeShader->IsValid()) return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	const FFrameContext& FrameContext = Ctx.Frame;
	const FViewportRenderOptions& RenderOptions = FrameContext.RenderOptions;

	FDOFCB DOFCBData = {};
	DOFCBData.Aperture	= RenderOptions.PostProcessSettings.Aperture;
	DOFCBData.FocusDist = RenderOptions.PostProcessSettings.FocusDistance;
	DOFCBData.FocalLength = RenderOptions.PostProcessSettings.FocalLength;
	DOFCBData.DOFSample = RenderOptions.PostProcessSettings.DOFSample;
	DOFCBData.CameraNear = FrameContext.NearClip;
	DOFCBData.CameraFar = FrameContext.FarClip;
	// SourceTexelSize stays full-res: the CoC pass gathers full-res depth and the
	// blur's radius is expressed in full-res pixels, so sampling the half-res CoC
	// texture at these UVs preserves the on-screen blur extent.
	DOFCBData.SourceTexelSize[0] = 1.0f / FrameContext.ViewportWidth;
	DOFCBData.SourceTexelSize[1] = 1.0f / FrameContext.ViewportHeight;

	const FDOFResources& CoCResources = FrameContext.DOFResources->CoCResources;
	const FDOFResources& BlurResources = FrameContext.DOFResources->BlurResources;

	// CoC Pass (half-res)
	DC->OMSetRenderTargets(1, &CoCResources.RTV, nullptr);
	SetViewport(DC, CoCResources.Width, CoCResources.Height);
	DOFCB.Update(DC, &DOFCBData, sizeof(DOFCBData));
	BindDOFCB(DC, DOFCB);
	ID3D11ShaderResourceView* CoCSRVs[2] = { FrameContext.SceneColorCopySRV, FrameContext.DepthCopySRV };
	DC->PSSetShaderResources(0, 2, CoCSRVs);
	DrawFullscreen(DC, CoCShader);
	UnBindSRVDouble(DC);

	// Blur Pass (half-res, same viewport as CoC)
	DC->OMSetRenderTargets(1, &BlurResources.RTV, nullptr);
	DC->PSSetShaderResources(0, 1, &CoCResources.SRV);
	DrawFullscreen(DC, BlurShader);
	UnBindSRVSingle(DC);

	// Composite Pass (full-res, upscales the blurred layer)
	DC->OMSetRenderTargets(1, &FrameContext.ViewportRTV, nullptr);
	SetViewport(DC, static_cast<uint32>(FrameContext.ViewportWidth), static_cast<uint32>(FrameContext.ViewportHeight));
	ID3D11ShaderResourceView* CompositeSRVs[2] = {
		FrameContext.SceneColorCopySRV,                  // sharp
		FrameContext.DOFResources->BlurResources.SRV,    // blurred
	};
	DC->PSSetShaderResources(0, 2, CompositeSRVs);
	DrawFullscreen(DC, CompositeShader);
	UnBindSRVDouble(DC);
}

void FDOFPass::EndPass(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	// Restore the main render target and full-res viewport
	ID3D11RenderTargetView* ViewportRTV = Ctx.Frame.ViewportRTV;
	DC->OMSetRenderTargets(1, &ViewportRTV, Ctx.Frame.ViewportDSV);
	SetViewport(DC, static_cast<uint32>(Ctx.Frame.ViewportWidth), static_cast<uint32>(Ctx.Frame.ViewportHeight));
	Ctx.Cache.bForceAll = true;
}