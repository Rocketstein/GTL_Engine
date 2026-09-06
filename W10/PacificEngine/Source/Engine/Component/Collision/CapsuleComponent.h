#pragma once

#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UCapsuleComponent, UShapeComponent)

    UCapsuleComponent();

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

    ECollisionShapeType GetCollisionShapeType() const override { return ECollisionShapeType::Capsule; }
    FCollisionShapeGeometry GetCollisionShapeGeometry() const override;

	void SetHalfHeight(float NewHalfHeight);
    void SetRadius(float NewRadius);
	float GetHalfHeight() const { return CapsuleHalfHeight; }
    float GetRadius() const { return CapsuleRadius; }
    float GetScaledCapsuleRadius() const;
    float GetScaledCapsuleHalfHeight() const;
    FVector GetCapsuleAxis() const;

protected:
    void RenderDebugShape(FScene& Scene) const override;
    void SyncLocalBoundsToCollision();

protected:
    float CapsuleHalfHeight = 3.0f;
    float CapsuleRadius = 1.0f;
};
