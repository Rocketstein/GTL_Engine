#include "GameOverlayPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FGameOverlayPass)

FGameOverlayPass::FGameOverlayPass()
{
	PassType = ERenderPass::GameOverlay;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
					ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
