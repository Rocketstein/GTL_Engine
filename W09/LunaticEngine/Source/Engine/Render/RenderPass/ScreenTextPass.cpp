#include "ScreenTextPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FScreenTextPass)

FScreenTextPass::FScreenTextPass()
{
	PassType    = ERenderPass::ScreenText;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
