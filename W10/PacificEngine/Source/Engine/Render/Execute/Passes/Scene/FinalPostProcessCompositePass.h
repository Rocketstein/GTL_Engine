#pragma once

#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

class FFinalPostProcessCompositePass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;

protected:
    bool IsEnabled(const FRenderPipelineContext& Context) const override;

private:
    bool BindCompositeConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command);
};
