#pragma once
#include "ShapeComponent.h"

class UBoxComponent : public UShapeComponent {
public:
	DECLARE_CLASS(UBoxComponent, UShapeComponent)
	UBoxComponent() = default;
	UBoxComponent(FVector InExtent) : BoxExtent(InExtent) {}
	FVector GetBoxExtent() const { return BoxExtent; }
	FVector GetScaledBoxExtent() const;
	void	SetBoxExtent(FVector InExtent);
	void	UpdateWorldAABB() const override;
	void	DrawDebugShape(FScene& Scene) const override;
	void	GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void	PostEditProperty(const char* PropertyName) override;
	void	Serialize(FArchive& Ar) override;

private:
	FVector BoxExtent = FVector(1.f, 1.f, 1.f);
};
