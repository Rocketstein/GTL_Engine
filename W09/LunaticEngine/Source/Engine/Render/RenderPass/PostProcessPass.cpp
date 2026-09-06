#include "PostProcessPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"

REGISTER_RENDER_PASS(FPostProcessPass)
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Resource/RenderResources.h"

FPostProcessPass::FPostProcessPass()
{
	PassType    = ERenderPass::PostProcess;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FPostProcessPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.DepthTexture || !Frame.DepthCopyTexture || !Frame.StencilCopySRV)
		return false;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	DC->OMSetRenderTargets(0, nullptr, nullptr);
	Ctx.Resources.UnbindSystemTextures(Ctx.Device);

	DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);

	if (Frame.SceneColorCopyTexture && Frame.ViewportRenderTexture)
	{
		DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
	}

	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	ID3D11ShaderResourceView* stencilSRV = Frame.StencilCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &stencilSRV);

	if (Frame.SceneColorCopySRV)
	{
		ID3D11ShaderResourceView* sceneColorSRV = Frame.SceneColorCopySRV;
		DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &sceneColorSRV);
	}

	Cache.bForceAll = true;
	return true;
}
