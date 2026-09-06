#pragma once
#include "ShapeComponent.h"
class UBoxComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UBoxComponent, UShapeComponent)

    UBoxComponent();

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

	ECollisionShapeType GetCollisionShapeType() const override{ return ECollisionShapeType::Box; }
    FCollisionShapeGeometry GetCollisionShapeGeometry() const override;

	const FVector& GetBoxExtent() const { return BoxExtent; }
    void SetBoxExtent(const FVector& NewBoxExtent);

	FVector GetScaledBoxExtent() const;

protected:
    void RenderDebugShape(FScene& Scene) const override;
    void SyncLocalBoundsToCollision();

protected:
    FVector BoxExtent = FVector(1.0f, 1.0f, 1.0f);
};
