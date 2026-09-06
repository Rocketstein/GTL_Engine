#include "Collider2DComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Core/Logging/LogMacros.h"

#include <cmath>

#include "GameFramework/AActor.h"

IMPLEMENT_ABSTRACT_CLASS(UCollider2DComponent, UShapeComponent)

UCollider2DComponent::UCollider2DComponent()
{
	ShapeColor = FColor(0, 220, 255, 255).ToVector4();

	OnComponentHit2D.AddDynamic(this,&UCollider2DComponent::OnComponentHit);
	OnComponentBeginOverlap2D.AddDynamic(this, &UCollider2DComponent::OnComponentBeginOverlap);
	OnComponentEndOverlap2D.AddDynamic(this, &UCollider2DComponent::OnComponentEndOverlap);
}

UCollider2DComponent::~UCollider2DComponent()
{
    OnComponentHit2D.RemoveDynamic(this);
	OnComponentBeginOverlap2D.RemoveDynamic(this);
	OnComponentEndOverlap2D.RemoveDynamic(this);
}

void UCollider2DComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "DebugPlaneOffsetZ", EPropertyType::Float, &DebugPlaneOffsetZ, -10000.0f, 10000.0f, 0.1f });
}

void UCollider2DComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << DebugPlaneOffsetZ;
}

FCollisionShapeGeometry UCollider2DComponent::GetCollisionShapeGeometry() const
{
    FCollisionShapeGeometry Geometry;
    Geometry.Type = ECollisionShapeType::Box;
    Geometry.Center = GetShapeWorldLocation();
    Geometry.BoxExtent = LocalExtents;
    return Geometry;
}

void UCollider2DComponent::OnComponentHit(UCollider2DComponent* OtherCollider)
{
}

void UCollider2DComponent::OnComponentBeginOverlap(UCollider2DComponent* OtherCollider)
{
}

void UCollider2DComponent::OnComponentEndOverlap(UCollider2DComponent* OtherCollider)
{
}

FVector2 UCollider2DComponent::GetShapeWorldLocation2D() const
{
    return ProjectWorldVectorTo2D(GetShapeWorldLocation());
}

float UCollider2DComponent::GetCollisionPlaneZ() const
{
    return GetShapeWorldLocation().Z + DebugPlaneOffsetZ;
}

FVector2 UCollider2DComponent::ProjectWorldVectorTo2D(const FVector& Vector) const
{
    return FVector2(Vector.X, Vector.Y);
}

FVector UCollider2DComponent::Expand2DPointToWorld(const FVector2& Point) const
{
    return FVector(Point.X, Point.Y, GetCollisionPlaneZ());
}

FVector2 UCollider2DComponent::GetSafeAxis2D(const FVector& WorldAxis, const FVector2& FallbackAxis) const
{
    FVector2 Axis = ProjectWorldVectorTo2D(WorldAxis);
    const float Length = Axis.Length();
    if (Length <= 0.0001f)
    {
        return FallbackAxis;
    }

    return Axis / Length;
}
