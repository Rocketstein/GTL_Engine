#pragma once
#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent {
public:
	DECLARE_CLASS(USphereComponent, UShapeComponent)

	USphereComponent() = default;
	USphereComponent(float InRadius) : SphereRadius(InRadius) {} 
	void  GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void  Serialize(FArchive& Ar) override;

	float GetSphereRadius() const { return SphereRadius; }
	void  SetSphereRadius(float InRadius) { SphereRadius = InRadius; }
	void  UpdateWorldAABB() const override;

	void DrawDebugShape(FScene& Scene) const override;

private:
	float SphereRadius = 1.f;
};