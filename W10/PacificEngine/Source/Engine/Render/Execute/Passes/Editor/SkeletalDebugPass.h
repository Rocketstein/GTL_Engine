#pragma once
#include "Render/Execute/Passes/Base/RenderPass.h"

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: render skeletal mesh bone node and lines over the editor scene.
    - Inputs: Bone core data (world tranmsform, parent index, etc.) and flat array of bones.
*/
class FSkeletalDebugPass : public FRenderPass {
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
