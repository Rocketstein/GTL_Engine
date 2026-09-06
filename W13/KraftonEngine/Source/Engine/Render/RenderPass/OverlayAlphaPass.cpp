#include "OverlayAlphaPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FOverlayAlphaPass)

FOverlayAlphaPass::FOverlayAlphaPass()
{
	PassType = ERenderPass::OverlayAlpha;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
					ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}
