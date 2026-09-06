#include "FogPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"

REGISTER_RENDER_PASS(FFogPass)
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Command/DrawCommandList.h"

FFogPass::FFogPass()
{
	PassType = ERenderPass::Fog;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
					ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FFogPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.RenderOptions.ShowFlags.bFog) return false;
	if (!Frame.DepthTexture || !Frame.DepthCopyTexture || !Frame.DepthCopySRV)
		return false;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	// SceneDepthTexture(t16) views DepthCopyTexture — refresh it with current scene
	// depth, then restore the scene-color RT + DSV before the fog fullscreen draw.
	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	ID3D11ShaderResourceView* depthSRV = Frame.DepthCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &depthSRV);

	Cache.bForceAll = true;
	return true;
}
