#pragma once

#include "Collider2DComponent.h"

class UBoxCollider2DComponent : public UCollider2DComponent
{
public:
    DECLARE_CLASS(UBoxCollider2DComponent, UCollider2DComponent)

    UBoxCollider2DComponent();

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void Serialize(FArchive& Ar) override;

    ECollision2DShapeType GetCollision2DShapeType() const override { return ECollision2DShapeType::Box; }
    FCollision2DShapeGeometry GetCollision2DShapeGeometry() const override;

    const FVector2& GetBoxExtent2D() const { return BoxExtent2D; }
    void SetBoxExtent2D(const FVector2& NewBoxExtent);
    FVector2 GetScaledBoxExtent2D() const;

	FCollision2DBounds GetCollision2DBounds() const override;

protected:
    void RenderDebugShape(FScene& Scene) const override;
    void SyncLocalBoundsToCollision();

protected:
    FVector2 BoxExtent2D = FVector2(1.0f, 1.0f);
};
