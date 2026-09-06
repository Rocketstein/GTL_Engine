#include "Render/Execute/Passes/Scene/SubUVPass.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"

void FSubUVPass::PrepareInputs(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FSubUVPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FSubUVPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
{
    DrawCommandBuild::BuildMeshDrawCommand(Proxy, ERenderPass::SubUV, Context, *Context.DrawCommandList);
}

void FSubUVPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::SubUV);
}
