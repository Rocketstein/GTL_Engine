#include "Component/CanvasRootComponent.h"

#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cstring>

namespace
{
	const FVector4 SelectedUIOutlineColor(0.10f, 0.54f, 0.96f, -1.0f);
}

IMPLEMENT_CLASS(UCanvasRootComponent, USceneComponent)

void UCanvasRootComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << CanvasSize;
}

void UCanvasRootComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Canvas Size", EPropertyType::Vec3, &CanvasSize, 0.0f, 4096.0f, 1.0f });
}

void UCanvasRootComponent::PostEditProperty(const char* PropertyName)
{
	USceneComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Canvas Size") == 0 || strcmp(PropertyName, "CanvasSize") == 0)
	{
		SetCanvasSize(CanvasSize);
	}
}

void UCanvasRootComponent::SetCanvasSize(const FVector& InCanvasSize)
{
	CanvasSize.X = (std::max)(1.0f, InCanvasSize.X);
	CanvasSize.Y = (std::max)(1.0f, InCanvasSize.Y);
	CanvasSize.Z = InCanvasSize.Z;
}

FVector2 UCanvasRootComponent::GetCanvasOrigin() const
{
	const FVector Location = GetRelativeLocation();
	return FVector2(Location.X, Location.Y);
}

void UCanvasRootComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	if (Scene.GetSelectedComponent() != this)
	{
		return;
	}

	constexpr float OutlineThickness = 3.0f;
	const FVector2 Origin = GetCanvasOrigin();
	const float X = Origin.X;
	const float Y = Origin.Y;
	const float Width = (std::max)(1.0f, CanvasSize.X);
	const float Height = (std::max)(1.0f, CanvasSize.Y);
	const int32 OutlineZ = 1000;

	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y - OutlineThickness), FVector2(Width + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y + Height), FVector2(Width + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y), FVector2(OutlineThickness, Height), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X + Width, Y), FVector2(OutlineThickness, Height), SelectedUIOutlineColor, OutlineZ);
}

bool UCanvasRootComponent::HitTestUIScreenPoint(float X, float Y) const
{
	const FVector2 Origin = GetCanvasOrigin();
	return X >= Origin.X
		&& X <= Origin.X + CanvasSize.X
		&& Y >= Origin.Y
		&& Y <= Origin.Y + CanvasSize.Y;
}
