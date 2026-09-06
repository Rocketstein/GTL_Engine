#pragma once
#include "IRenderPipeline.h"
#include "Render/Pipeline/RenderCollector.h"
#include "Render/Types/FrameContext.h"

class UEngine;

class FDefaultRenderPipeline : public IRenderPipeline
{
public:
	FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer);
	~FDefaultRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;

	FViewportRenderOptions& GetRuntimeRenderOptions() { return RuntimeRenderOptions; }
	const FViewportRenderOptions& GetRuntimeRenderOptions() const { return RuntimeRenderOptions; }
	void SetGammaCorrection(bool bEnable, float DisplayGamma, float BlendWeight, bool bUseSRGBCurve);

private:
	UEngine* Engine = nullptr;
	FRenderCollector Collector;
	FFrameContext Frame;
	FViewportRenderOptions RuntimeRenderOptions;
};
