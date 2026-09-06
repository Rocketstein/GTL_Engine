#pragma once
#include "World/PrimitiveComponent.h"

class UCubeComponent : public UPrimitiveComponent
{
private:

public:
	DECLARE_CLASS(UCubeComponent, UPrimitiveComponent)
	UCubeComponent();
	virtual void UpdateWorldAABB() override;
	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Cube;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};