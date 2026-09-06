#include "SphereComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Serialization/Archive.h"
#include <algorithm>

IMPLEMENT_CLASS(USphereComponent, UShapeComponent)

USphereComponent::USphereComponent()
{
    SyncLocalBoundsToCollision();
}

void USphereComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "SphereRadius", EPropertyType::Float, &SphereRadius, 0.0f, 10000.0f, 1.0f });
}

void USphereComponent::PostEditProperty(const char* PropertyName)
{
    UShapeComponent::PostEditProperty(PropertyName);
    SyncLocalBoundsToCollision();
}

void USphereComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << SphereRadius;
    SyncLocalBoundsToCollision();
}

FCollisionShapeGeometry USphereComponent::GetCollisionShapeGeometry() const
{
    FCollisionShapeGeometry Geometry;
    Geometry.Type = GetCollisionShapeType();
    Geometry.Radius = GetScaledSphereRadius();
    Geometry.Rotation = GetWorldRotation();
    Geometry.Center = GetShapeWorldLocation();
    return Geometry;
}

void USphereComponent::SetRadius(float NewRadius)
{
    SphereRadius = NewRadius;
    SyncLocalBoundsToCollision();
}

float USphereComponent::GetScaledSphereRadius() const
{
    const FVector Scale = GetAbsWorldScale();
    const float MaxScale = std::max(Scale.X, std::max(Scale.Y, Scale.Z));

	return SphereRadius * MaxScale;
}

void USphereComponent::RenderDebugShape(FScene& Scene) const
{
    RenderDebugSphere(Scene, GetShapeWorldLocation(), GetScaledSphereRadius(), 24, GetDebugShapeColor(), 0.0f);
}

void USphereComponent::SyncLocalBoundsToCollision()
{
    LocalExtents = FVector(SphereRadius, SphereRadius, SphereRadius);
    MarkWorldBoundsDirty();
}
