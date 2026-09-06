#include "BoxCollider2DComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UBoxCollider2DComponent, UCollider2DComponent)

UBoxCollider2DComponent::UBoxCollider2DComponent()
{
    SyncLocalBoundsToCollision();
}

void UBoxCollider2DComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UCollider2DComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "BoxExtent2D", EPropertyType::Vec2, &BoxExtent2D });
}

void UBoxCollider2DComponent::PostEditProperty(const char* PropertyName)
{
    UCollider2DComponent::PostEditProperty(PropertyName);
    SyncLocalBoundsToCollision();
}

void UBoxCollider2DComponent::Serialize(FArchive& Ar)
{
    UCollider2DComponent::Serialize(Ar);
    Ar << BoxExtent2D;
    SyncLocalBoundsToCollision();
}

FCollision2DShapeGeometry UBoxCollider2DComponent::GetCollision2DShapeGeometry() const
{
    FCollision2DShapeGeometry Geometry;
    Geometry.Type = GetCollision2DShapeType();
    Geometry.Center = GetShapeWorldLocation2D();
    Geometry.AxisX = GetSafeAxis2D(GetForwardVector(), FVector2(1.0f, 0.0f));
    Geometry.AxisY = GetSafeAxis2D(GetRightVector(), FVector2(0.0f, 1.0f));
    Geometry.BoxExtent = GetScaledBoxExtent2D();
    return Geometry;
}

void UBoxCollider2DComponent::SetBoxExtent2D(const FVector2& NewBoxExtent)
{
    BoxExtent2D = NewBoxExtent;
    SyncLocalBoundsToCollision();
}

FVector2 UBoxCollider2DComponent::GetScaledBoxExtent2D() const
{
    const FVector Scale = GetAbsWorldScale();
    return FVector2(BoxExtent2D.X * Scale.X, BoxExtent2D.Y * Scale.Y);
}

FCollision2DBounds UBoxCollider2DComponent::GetCollision2DBounds() const
{
    const FCollision2DShapeGeometry Geometry = GetCollision2DShapeGeometry();

    const float ExtentX =
        std::abs(Geometry.AxisX.X) * Geometry.BoxExtent.X +
        std::abs(Geometry.AxisY.X) * Geometry.BoxExtent.Y;

    const float ExtentY =
        std::abs(Geometry.AxisX.Y) * Geometry.BoxExtent.X +
        std::abs(Geometry.AxisY.Y) * Geometry.BoxExtent.Y;

    const FVector2 Extent(ExtentX, ExtentY);

    FCollision2DBounds Bounds;
    Bounds.Min = Geometry.Center - Extent;
    Bounds.Max = Geometry.Center + Extent;
    return Bounds;
}

void UBoxCollider2DComponent::RenderDebugShape(FScene& Scene) const
{
    const FCollision2DShapeGeometry Geometry = GetCollision2DShapeGeometry();
    const FVector2 X = Geometry.AxisX * Geometry.BoxExtent.X;
    const FVector2 Y = Geometry.AxisY * Geometry.BoxExtent.Y;

    const FVector P0 = Expand2DPointToWorld(Geometry.Center - X - Y);
    const FVector P1 = Expand2DPointToWorld(Geometry.Center + X - Y);
    const FVector P2 = Expand2DPointToWorld(Geometry.Center + X + Y);
    const FVector P3 = Expand2DPointToWorld(Geometry.Center - X + Y);

    RenderDebugLine(Scene, P0, P1, GetDebugShapeColor(), 0.0f);
    RenderDebugLine(Scene, P1, P2, GetDebugShapeColor(), 0.0f);
    RenderDebugLine(Scene, P2, P3, GetDebugShapeColor(), 0.0f);
    RenderDebugLine(Scene, P3, P0, GetDebugShapeColor(), 0.0f);
}

void UBoxCollider2DComponent::SyncLocalBoundsToCollision()
{
    LocalExtents = FVector(BoxExtent2D.X, BoxExtent2D.Y, 0.05f);
    MarkWorldBoundsDirty();
}
