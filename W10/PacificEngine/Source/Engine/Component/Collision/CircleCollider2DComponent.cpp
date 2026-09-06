#include "CircleCollider2DComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_CLASS(UCircleCollider2DComponent, UCollider2DComponent)

UCircleCollider2DComponent::UCircleCollider2DComponent()
{
    SyncLocalBoundsToCollision();
}

void UCircleCollider2DComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UCollider2DComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "CircleRadius", EPropertyType::Float, &CircleRadius, 0.0f, 10000.0f, 1.0f });
}

void UCircleCollider2DComponent::PostEditProperty(const char* PropertyName)
{
    UCollider2DComponent::PostEditProperty(PropertyName);
    SyncLocalBoundsToCollision();
}

void UCircleCollider2DComponent::Serialize(FArchive& Ar)
{
    UCollider2DComponent::Serialize(Ar);
    Ar << CircleRadius;
    SyncLocalBoundsToCollision();
}

FCollision2DShapeGeometry UCircleCollider2DComponent::GetCollision2DShapeGeometry() const
{
    FCollision2DShapeGeometry Geometry;
    Geometry.Type = GetCollision2DShapeType();
    Geometry.Center = GetShapeWorldLocation2D();
    Geometry.Radius = GetScaledCircleRadius();
    return Geometry;
}

void UCircleCollider2DComponent::SetRadius(float NewRadius)
{
    CircleRadius = NewRadius;
    SyncLocalBoundsToCollision();
}

float UCircleCollider2DComponent::GetScaledCircleRadius() const
{
    const FVector Scale = GetAbsWorldScale();
    return CircleRadius * std::max(Scale.X, Scale.Y);
}

FCollision2DBounds UCircleCollider2DComponent::GetCollision2DBounds() const
{
    const FCollision2DShapeGeometry Geometry = GetCollision2DShapeGeometry();
    const FVector2 Extent(Geometry.Radius, Geometry.Radius);

    FCollision2DBounds Bounds;
    Bounds.Min = Geometry.Center - Extent;
    Bounds.Max = Geometry.Center + Extent;
    return Bounds;
}

void UCircleCollider2DComponent::RenderDebugShape(FScene& Scene) const
{
    constexpr int32 Segments = 32;
    constexpr float Pi = 3.14159265358979323846f;

    const FCollision2DShapeGeometry Geometry = GetCollision2DShapeGeometry();
    FVector PreviousPoint = Expand2DPointToWorld(Geometry.Center + FVector2(Geometry.Radius, 0.0f));

    for (int32 SegmentIndex = 1; SegmentIndex <= Segments; ++SegmentIndex)
    {
        const float Angle = (static_cast<float>(SegmentIndex) / static_cast<float>(Segments)) * Pi * 2.0f;
        const FVector2 Point2D = Geometry.Center + FVector2(std::cos(Angle), std::sin(Angle)) * Geometry.Radius;
        const FVector CurrentPoint = Expand2DPointToWorld(Point2D);
        RenderDebugLine(Scene, PreviousPoint, CurrentPoint, GetDebugShapeColor(), 0.0f);
        PreviousPoint = CurrentPoint;
    }
}

void UCircleCollider2DComponent::SyncLocalBoundsToCollision()
{
    LocalExtents = FVector(CircleRadius, CircleRadius, 0.05f);
    MarkWorldBoundsDirty();
}
