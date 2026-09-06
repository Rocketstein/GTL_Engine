#pragma once

#include "Collider2DComponent.h"

class UCircleCollider2DComponent : public UCollider2DComponent
{
public:
    DECLARE_CLASS(UCircleCollider2DComponent, UCollider2DComponent)

    UCircleCollider2DComponent();

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void Serialize(FArchive& Ar) override;

    ECollision2DShapeType GetCollision2DShapeType() const override { return ECollision2DShapeType::Circle; }
    FCollision2DShapeGeometry GetCollision2DShapeGeometry() const override;

    float GetRadius() const { return CircleRadius; }
    void SetRadius(float NewRadius);
    float GetScaledCircleRadius() const;

	FCollision2DBounds GetCollision2DBounds() const override;


protected:
    void RenderDebugShape(FScene& Scene) const override;
    void SyncLocalBoundsToCollision();

protected:
    float CircleRadius = 1.0f;
};
