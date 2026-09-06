#pragma once
#include "World/PrimitiveComponent.h"

class UPlaneComponent : public UPrimitiveComponent
{
private:

public:
	DECLARE_CLASS(UPlaneComponent, UPrimitiveComponent)
	UPlaneComponent();

	virtual void UpdateWorldAABB() override;
	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Plane;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};