#include "UIPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FUIPass)

FUIPass::FUIPass()
{
	PassType = ERenderPass::UI;
	RenderState = {
		EDepthStencilState::NoDepth,
		EBlendState::AlphaBlend,
		ERasterizerState::SolidBackCull,
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		false
	};
}
