#include "BoxComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UBoxComponent, UShapeComponent)

UBoxComponent::UBoxComponent()
{
    SyncLocalBoundsToCollision();
}

void UBoxComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "BoxExtent", EPropertyType::Vec3, &BoxExtent });
}

void UBoxComponent::PostEditProperty(const char* PropertyName)
{
    UShapeComponent::PostEditProperty(PropertyName);
    SyncLocalBoundsToCollision();
}

void UBoxComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << BoxExtent;
    SyncLocalBoundsToCollision();
}

FCollisionShapeGeometry UBoxComponent::GetCollisionShapeGeometry() const
{
    FCollisionShapeGeometry Geometry;
    Geometry.Type = GetCollisionShapeType();
    Geometry.Center = GetShapeWorldLocation();
    Geometry.Rotation = GetWorldRotation();
    Geometry.BoxExtent = GetScaledBoxExtent();
    return Geometry;
}

void UBoxComponent::SetBoxExtent(const FVector& NewBoxExtent)
{
    BoxExtent = NewBoxExtent;
    SyncLocalBoundsToCollision();
}

FVector UBoxComponent::GetScaledBoxExtent() const
{
    const FVector Scale = GetAbsWorldScale();

	return FVector(
        BoxExtent.X * Scale.X,
        BoxExtent.Y * Scale.Y,
        BoxExtent.Z * Scale.Z);
}

void UBoxComponent::RenderDebugShape(FScene& Scene) const
{
    const FVector Extent = GetScaledBoxExtent();
    const FVector Center = GetShapeWorldLocation();
    const FVector Forward = GetForwardVector();
    const FVector Right = GetRightVector();
    const FVector Up = GetUpVector();

    FVector P[8] = {
        Center - Forward * Extent.X - Right * Extent.Y - Up * Extent.Z,
        Center + Forward * Extent.X - Right * Extent.Y - Up * Extent.Z,
        Center + Forward * Extent.X + Right * Extent.Y - Up * Extent.Z,
        Center - Forward * Extent.X + Right * Extent.Y - Up * Extent.Z,
        Center - Forward * Extent.X - Right * Extent.Y + Up * Extent.Z,
        Center + Forward * Extent.X - Right * Extent.Y + Up * Extent.Z,
        Center + Forward * Extent.X + Right * Extent.Y + Up * Extent.Z,
        Center - Forward * Extent.X + Right * Extent.Y + Up * Extent.Z
    };

    RenderDebugBox(Scene, P[0], P[1], P[2], P[3], P[4], P[5], P[6], P[7], GetDebugShapeColor(), 0.0f);
}

void UBoxComponent::SyncLocalBoundsToCollision()
{
    LocalExtents = BoxExtent;
    MarkWorldBoundsDirty();
}
