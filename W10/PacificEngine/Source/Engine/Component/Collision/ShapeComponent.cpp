#include "ShapeComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Scene.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_ABSTRACT_CLASS(UShapeComponent, UPrimitiveComponent)

void UShapeComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Shape Color", EPropertyType::Color4, &ShapeColor });
    OutProps.push_back({ "DrawOnlyIfSelected", EPropertyType::Bool, &bDrawOnlyIfSelected });
}

void UShapeComponent::PostEditProperty(const char* PropertyName)
{
}

void UShapeComponent::Serialize(FArchive& Ar)
{
    UPrimitiveComponent::Serialize(Ar);
    Ar << ShapeColor;
    Ar << bDrawOnlyIfSelected;
}

void UShapeComponent::SetShapeColor(FColor NewColor)
{
    ShapeColor = NewColor.ToVector4();
}

void UShapeComponent::SetDebugOverlapping(bool bNewDebugOverlapping)
{
    bDebugOverlapping = bNewDebugOverlapping;
}

void UShapeComponent::SetDrawOnlyIfSelected(bool bNewDrawOnlyIfSelected)
{
    bDrawOnlyIfSelected = bNewDrawOnlyIfSelected;
}

void UShapeComponent::ContributeSelectedVisuals(FScene& Scene) const
{
    if (bDrawOnlyIfSelected && ShouldRenderDebugShape())
    {
        RenderDebugShape(Scene);
    }
}

FVector UShapeComponent::GetShapeWorldLocation() const
{
    return GetWorldLocation();
}

FVector UShapeComponent::GetAbsWorldScale() const
{
    FVector Scale = GetWorldScale();
    return FVector(
        std::abs(Scale.X),
        std::abs(Scale.Y),
        std::abs(Scale.Z));
}

void UShapeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bDrawOnlyIfSelected || TickType == ELevelTick::LEVELTICK_All || !ShouldRenderDebugShape())
    {
        return;
    }

    AActor* OwnerActor = GetOwner();
    UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
    if (World)
    {
        RenderDebugShape(World->GetScene());
    }
}

bool UShapeComponent::ShouldRenderDebugShape() const
{
    const AActor* OwnerActor = GetOwner();
    UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
    if (!OwnerActor || !World)
    {
        return false;
    }

    if (World->GetWorldType() != EWorldType::Editor)
    {
        return false;
    }

    return OwnerActor->IsVisible() && IsVisible() && ShouldRenderInCurrentWorld();
}

FColor UShapeComponent::GetDebugShapeColor() const
{
    if (bDebugOverlapping)
    {
        return FColor::Red();
    }

    auto ToByte = [](float Value) -> uint32
    {
        return static_cast<uint32>(std::clamp(Value, 0.0f, 1.0f) * 255.0f);
    };

    return FColor(ToByte(ShapeColor.X), ToByte(ShapeColor.Y), ToByte(ShapeColor.Z), ToByte(ShapeColor.W));
}
