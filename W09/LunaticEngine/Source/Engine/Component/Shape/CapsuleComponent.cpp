#include "CapsuleComponent.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UCapsuleComponent, UShapeComponent)

void UCapsuleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	UShapeComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Capsule Radius",		EPropertyType::Float, &CapsuleRadius,	  0.0f, 2048.f, 0.1f });
	OutProps.push_back({ "Capsule Half-Height", EPropertyType::Float, &CapsuleHalfHeight, 0.0f, 2048.f, 0.1f });
}

void UCapsuleComponent::PostEditProperty(const char* PropertyName) {
	if (strcmp(PropertyName, "Capsule Radius") == 0)
		CapsuleRadius = CapsuleRadius < CapsuleHalfHeight ? CapsuleRadius : CapsuleHalfHeight;
	else if (strcmp(PropertyName, "Capsule Half-Height") == 0)
		CapsuleHalfHeight = CapsuleHalfHeight > CapsuleRadius ? CapsuleHalfHeight : CapsuleRadius;
	UShapeComponent::PostEditProperty(PropertyName);
}

void UCapsuleComponent::Serialize(FArchive& Ar) {
	UShapeComponent::Serialize(Ar);
	Ar << CapsuleRadius;
	Ar << CapsuleHalfHeight;
}

void UCapsuleComponent::DrawDebugShape(FScene& Scene) const {
	if (CapsuleRadius <= 0.f || CapsuleHalfHeight < 0.f) return;

	FVector Center    = GetWorldLocation();
	FVector Up        = GetUpVector().Normalized();
	FVector Fwd       = GetForwardVector().Normalized();
	FVector Right     = GetRightVector().Normalized();

	float BiasedHeight = CapsuleHalfHeight > CapsuleRadius ? (CapsuleHalfHeight - CapsuleRadius) : 0.f;
	FVector TopCenter = Center + Up * BiasedHeight;
	FVector BotCenter = Center - Up * BiasedHeight;
	FVector NegUp     = Up * -1.f;

	constexpr uint32 Segments     = 24;
	constexpr uint32 HalfSegments = Segments / 2;

	// Junction rings at top and bottom
	DrawDebugRing(TopCenter, CapsuleRadius, Fwd, Right, Segments,     false, Scene);
	DrawDebugRing(BotCenter, CapsuleRadius, Fwd, Right, Segments,     false, Scene);

	// Top hemisphere
	DrawDebugRing(TopCenter, CapsuleRadius, Fwd,  Up,    HalfSegments, true, Scene);
	DrawDebugRing(TopCenter, CapsuleRadius, Right, Up,    HalfSegments, true, Scene);

	// Bottom hemisphere
	DrawDebugRing(BotCenter, CapsuleRadius, Fwd,  NegUp, HalfSegments, true, Scene);
	DrawDebugRing(BotCenter, CapsuleRadius, Right, NegUp, HalfSegments, true, Scene);

	// Body lines
	Scene.AddDebugLine(TopCenter + Fwd   * CapsuleRadius, BotCenter + Fwd   * CapsuleRadius, ShapeColor);
	Scene.AddDebugLine(TopCenter - Fwd   * CapsuleRadius, BotCenter - Fwd   * CapsuleRadius, ShapeColor);
	Scene.AddDebugLine(TopCenter + Right * CapsuleRadius, BotCenter + Right * CapsuleRadius, ShapeColor);
	Scene.AddDebugLine(TopCenter - Right * CapsuleRadius, BotCenter - Right * CapsuleRadius, ShapeColor);
}

void UCapsuleComponent::UpdateWorldAABB() const {
	FVector WorldCenter = GetWorldLocation();
	FVector Up = GetUpVector().Normalized();

	float BiasedHeight = CapsuleHalfHeight > CapsuleRadius ? (CapsuleHalfHeight - CapsuleRadius) : 0.f;

	FVector Extent(
		fabsf(Up.X) * BiasedHeight + CapsuleRadius,
		fabsf(Up.Y) * BiasedHeight + CapsuleRadius,
		fabsf(Up.Z) * BiasedHeight + CapsuleRadius
	);

	WorldAABBMinLocation = WorldCenter - Extent;
	WorldAABBMaxLocation = WorldCenter + Extent;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}