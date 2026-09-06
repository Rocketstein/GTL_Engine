#include "SphereComponent.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(USphereComponent, UShapeComponent)

void USphereComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	UShapeComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Sphere Radius", EPropertyType::Float, &SphereRadius, 0.0f, 2048.f, 0.1f });
}

void USphereComponent::Serialize(FArchive& Ar) {
	UShapeComponent::Serialize(Ar);
	Ar << SphereRadius;
}

void USphereComponent::DrawDebugShape(FScene& Scene) const {
	if (SphereRadius <= 0.f) return;
	constexpr uint32 Segments = 24;
	
	FVector Center = GetWorldLocation();

	DrawDebugRing(Center, SphereRadius, FVector(1, 0, 0), FVector(0, 1, 0), Segments, false, Scene);
	DrawDebugRing(Center, SphereRadius, FVector(1, 0, 0), FVector(0, 0, 1), Segments, false, Scene);
	DrawDebugRing(Center, SphereRadius, FVector(0, 1, 0), FVector(0, 0, 1), Segments, false, Scene);
}

void USphereComponent::UpdateWorldAABB() const {
	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(SphereRadius, SphereRadius, SphereRadius);
	WorldAABBMaxLocation = WorldCenter + FVector(SphereRadius, SphereRadius, SphereRadius);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}