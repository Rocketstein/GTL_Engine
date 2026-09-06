#pragma once

#include "RenderPassBase.h"

class FGammaCorrectionPass : public FRenderPassBase
{
public:
	FGammaCorrectionPass();

	bool BeginPass(const FPassContext& Ctx) override;
};
