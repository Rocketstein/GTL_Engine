#pragma once
#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent {
public:
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
	UCapsuleComponent() = default;
	UCapsuleComponent(float InHalfHeight, float InRadius) : CapsuleHalfHeight(InHalfHeight), CapsuleRadius(InRadius) {}

	void  GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void  Serialize(FArchive& Ar) override;

	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
	float GetCapsuleRadius() const { return CapsuleRadius; }
	void  SetCapsuleHalfHeight(float InHalfHeight) { CapsuleHalfHeight = CapsuleHalfHeight > CapsuleRadius ? CapsuleHalfHeight : CapsuleRadius; }
	void  SetCapsuleRadius(float InRadius) { CapsuleRadius = CapsuleRadius < CapsuleHalfHeight ? CapsuleRadius : CapsuleHalfHeight; }
	void  UpdateWorldAABB() const override;
	void  PostEditProperty(const char* PropertyName) override;

	void DrawDebugShape(FScene& Scene) const override;

private:
	float CapsuleHalfHeight = 2.f;
	float CapsuleRadius		= 1.f;
};