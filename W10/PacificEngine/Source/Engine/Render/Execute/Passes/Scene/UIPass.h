#pragma once

#include "Render/Execute/Passes/Base/RenderPass.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: render screen-space UI quads over the scene and post process output.
    - Inputs: FScene UI proxy registry, frame-local UI batch, optional UI texture SRVs.
    - Outputs: viewport color with alpha-blended UI.
    - Registers: VS/PS b2 UIParams and PS t0 optional UI texture.
*/
class FUIPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
