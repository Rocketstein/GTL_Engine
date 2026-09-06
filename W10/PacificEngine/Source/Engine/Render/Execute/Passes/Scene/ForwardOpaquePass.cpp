#include "Render/Execute/Passes/Scene/ForwardOpaquePass.h"
#include "Component/DecalComponent.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Renderer.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"

void FForwardOpaquePass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewModePassRegistry* Registry = Context.ViewMode.Registry;
    const bool bUsesLighting = Registry && Registry->UsesLightingPass(Context.ViewMode.ActiveViewMode);

    if (bUsesLighting && Context.Resources)
    {
        ID3D11Buffer* GlobalLightBuffer = Context.Resources->GlobalLightBuffer.GetBuffer();
        Context.Context->PSSetConstantBuffers(ECBSlot::Light, 1, &GlobalLightBuffer);

        ID3D11ShaderResourceView* LocalLightsSRV = Context.Resources->LocalLightSRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &LocalLightsSRV);
    }

    if (bUsesLighting && Context.Renderer)
    {
        if (FRenderPass* Pass = Context.Renderer->GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
        {
            FShadowMapPass* ShadowPass = static_cast<FShadowMapPass*>(Pass);
            for (uint32 i = 0; i < ESystemTexSlot::MaxShadowAtlasPages; ++i)
            {
                ID3D11ShaderResourceView* ShadowSRV = ShadowPass->GetShadowAtlasSRV(i);
                Context.Context->PSSetShaderResources(ESystemTexSlot::ShadowAtlasBase + i, 1, &ShadowSRV);
                ID3D11ShaderResourceView* ShadowMomentSRV = ShadowPass->GetShadowMomentSRV(i);
                Context.Context->PSSetShaderResources(ESystemTexSlot::ShadowMomentAtlasBase + i, 1, &ShadowMomentSRV);
            }
        }
    }

    if (Context.StateCache)
    {
        Context.StateCache->bForceAll = true;
    }
}

void FForwardOpaquePass::PrepareTargets(FRenderPipelineContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FForwardOpaquePass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
{
    DrawCommandBuild::BuildMeshDrawCommand(Proxy, ERenderPass::Opaque, Context, *Context.DrawCommandList);
}

void FForwardOpaquePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::Opaque);
}

void FForwardOpaquePass::Cleanup(FRenderPipelineContext& Context)
{
    ID3D11ShaderResourceView* NullSRV = nullptr;
    for (uint32 i = 0; i < ESystemTexSlot::MaxShadowAtlasPages; ++i)
    {
        Context.Context->VSSetShaderResources(ESystemTexSlot::ShadowAtlasBase + i, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::ShadowAtlasBase + i, 1, &NullSRV);
        Context.Context->VSSetShaderResources(ESystemTexSlot::ShadowMomentAtlasBase + i, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::ShadowMomentAtlasBase + i, 1, &NullSRV);
    }
}
