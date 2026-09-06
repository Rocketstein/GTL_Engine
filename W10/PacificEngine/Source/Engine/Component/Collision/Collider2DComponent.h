#pragma once

#include "ShapeComponent.h"
#include "Collision/Collision2DShapeGeometry.h"
#include "Core/Delegate.h"

class UCollider2DComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UCollider2DComponent, UShapeComponent)

    UCollider2DComponent();
    ~UCollider2DComponent();

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void Serialize(FArchive& Ar) override;

    ECollisionShapeType GetCollisionShapeType() const override { return ECollisionShapeType::Box; }
    FCollisionShapeGeometry GetCollisionShapeGeometry() const override;

    virtual ECollision2DShapeType GetCollision2DShapeType() const = 0;
    virtual FCollision2DShapeGeometry GetCollision2DShapeGeometry() const = 0;

	DECLARE_DELEGATE(OnComponentHit2D, UCollider2DComponent*);              // Collision
	DECLARE_DELEGATE(OnComponentBeginOverlap2D, UCollider2DComponent*);     // Trigger Begin
	DECLARE_DELEGATE(OnComponentEndOverlap2D, UCollider2DComponent*);       // Trigger End

	void OnComponentHit(UCollider2DComponent* OtherCollider);
	void OnComponentBeginOverlap(UCollider2DComponent* OtherCollider);
	void OnComponentEndOverlap(UCollider2DComponent* OtherCollider);

    FVector2 GetShapeWorldLocation2D() const;
    float GetCollisionPlaneZ() const;
    virtual FCollision2DBounds GetCollision2DBounds() const = 0;

protected:
    FVector2 ProjectWorldVectorTo2D(const FVector& Vector) const;
    FVector Expand2DPointToWorld(const FVector2& Point) const;
    FVector2 GetSafeAxis2D(const FVector& WorldAxis, const FVector2& FallbackAxis) const;

protected:
    float DebugPlaneOffsetZ = 0.0f;
};
