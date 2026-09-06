#include "Render/Execute/Passes/Scene/UIPass.h"

#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"

void FUIPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FUIPass::PrepareTargets(FRenderPipelineContext& Context)
{
    if (!Context.Context)
    {
        return;
    }

    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    ID3D11DepthStencilView* DSV = Context.GetViewportDSV();
    Context.Context->OMSetRenderTargets(1, &RTV, DSV);

    if (Context.StateCache)
    {
        Context.StateCache->RTV = RTV;
        Context.StateCache->DSV = DSV;
    }
}

void FUIPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    DrawCommandBuild::BuildUIDrawCommands(Context, *Context.DrawCommandList);
}

void FUIPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    uint32 Start = 0;
    uint32 End   = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::UI, Start, End);
    if (Start < End)
    {
        Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
    }
}
