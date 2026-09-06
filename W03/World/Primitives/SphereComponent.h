#pragma once
#include "World/PrimitiveComponent.h"

class USphereComponent : public UPrimitiveComponent
{
private:

public:
	DECLARE_CLASS(USphereComponent, UPrimitiveComponent)
	USphereComponent();
	virtual void UpdateWorldAABB() override;
	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Sphere;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};