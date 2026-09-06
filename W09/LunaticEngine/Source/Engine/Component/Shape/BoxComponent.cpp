#include "BoxComponent.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <cstring>

IMPLEMENT_CLASS(UBoxComponent, UShapeComponent)

void UBoxComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	UShapeComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Box Extent", EPropertyType::Vec3, &BoxExtent, 0.0f, 0.0f, 0.1f });
}

void UBoxComponent::PostEditProperty(const char* PropertyName) {
	UShapeComponent::PostEditProperty(PropertyName);
	if (strcmp(PropertyName, "Box Extent") == 0)
	{
		MarkWorldBoundsDirty();
	}
}

void UBoxComponent::SetBoxExtent(FVector InExtent) {
	BoxExtent = InExtent;
	MarkWorldBoundsDirty();
}

FVector UBoxComponent::GetScaledBoxExtent() const {
	const FVector WorldScale = GetWorldScale();
	return FVector(
		BoxExtent.X * WorldScale.X,
		BoxExtent.Y * WorldScale.Y,
		BoxExtent.Z * WorldScale.Z);
}

void UBoxComponent::DrawDebugShape(FScene& Scene) const {
	FVector Center = GetWorldLocation();
	const FVector Extent = GetScaledBoxExtent();
	FVector X      = GetForwardVector().Normalized() * Extent.X;
	FVector Y      = GetRightVector().Normalized()   * Extent.Y;
	FVector Z      = GetUpVector().Normalized()      * Extent.Z;

	FVector P[8] = {
		Center - X - Y - Z,
		Center + X - Y - Z,
		Center + X + Y - Z,
		Center - X + Y - Z,
		Center - X - Y + Z,
		Center + X - Y + Z,
		Center + X + Y + Z,
		Center - X + Y + Z,
	};

	Scene.AddDebugLine(P[0], P[1], ShapeColor);
	Scene.AddDebugLine(P[1], P[2], ShapeColor);
	Scene.AddDebugLine(P[2], P[3], ShapeColor);
	Scene.AddDebugLine(P[3], P[0], ShapeColor);
	Scene.AddDebugLine(P[4], P[5], ShapeColor);
	Scene.AddDebugLine(P[5], P[6], ShapeColor);
	Scene.AddDebugLine(P[6], P[7], ShapeColor);
	Scene.AddDebugLine(P[7], P[4], ShapeColor);
	Scene.AddDebugLine(P[0], P[4], ShapeColor);
	Scene.AddDebugLine(P[1], P[5], ShapeColor);
	Scene.AddDebugLine(P[2], P[6], ShapeColor);
	Scene.AddDebugLine(P[3], P[7], ShapeColor);
}

void UBoxComponent::UpdateWorldAABB() const {
	FVector LExt = BoxExtent;

	FMatrix worldMatrix = GetWorldMatrix();

	float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
	float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
	float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}
void UBoxComponent::Serialize(FArchive& Ar) {
	UShapeComponent::Serialize(Ar);
	Ar << BoxExtent;
}
