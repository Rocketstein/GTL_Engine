#pragma once
#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/Buffer.h"

class FDOFPass final : public FRenderPassBase
{
public:
	FDOFPass();
	bool BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;

private:
	FConstantBuffer DOFCB;
};
