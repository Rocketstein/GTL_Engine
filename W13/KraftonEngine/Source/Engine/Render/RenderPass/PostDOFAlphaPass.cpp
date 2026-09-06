#include "PostDOFAlphaPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FPostAlphaPass)

FPostAlphaPass::FPostAlphaPass()
{
	PassType = ERenderPass::PostDOFAlpha;
	RenderState = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend,
					ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}
