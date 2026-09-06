#include "ShapeComponent.h"
#include "Serialization/Archive.h"
#include "GameFramework/World.h"
#include "Collision/CollisionDispatcher.h"

DEFINE_CLASS(UShapeComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UShapeComponent)

void UShapeComponent::PostEditProperty(const char* PropertyName) {
	USceneComponent::PostEditProperty(PropertyName);
}

void UShapeComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Is Collidable",			EPropertyType::Bool,	&bCollisionEnabled });
	static const char* OverlapBehaviourNames[] = { "Ignore", "Hit", "Overlap" };
	OutProps.push_back({ "Overlap Behaviour",		EPropertyType::Enum, &bGenerateOverlapEvents, 0.f, 0.f, 0.1f, OverlapBehaviourNames, 3 });
	static const char* MobilityNames[] = { "Static", "Stationary", "Movable" };
	OutProps.push_back({ "Mobility",				EPropertyType::Enum, &Mobility, 0.f, 0.f, 0.1f, MobilityNames, 3 });
	OutProps.push_back({ "Draw Only If Selected",	EPropertyType::Bool,	&bDrawOnlyIfSelected });
	OutProps.push_back({ "Shape Color",				EPropertyType::Color4,	&ShapeColor });
}

void UShapeComponent::Serialize(FArchive& Ar) {
	UPrimitiveComponent::Serialize(Ar);
	Ar << bDrawOnlyIfSelected;
	Ar << ShapeColor;
}

void UShapeComponent::ContributeVisuals(FScene& Scene) const {
	if (!bDrawOnlyIfSelected) {
		DrawDebugShape(Scene);
	}
	else {
		if (Owner && Owner->IsActorSelected()) {
			DrawDebugShape(Scene);
		}
	}
}

void UShapeComponent::DrawDebugRing(FVector Center, float Radius, FVector AxisA, FVector AxisB, uint32 Segments, bool Half, FScene& Scene) const {
	const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);
	FVector Prev = Center + AxisA * Radius;

	int32 It = Half ? Segments / 2 : Segments;
	for (int32 Index = 1; Index <= It; ++Index)
	{
		const float Angle = Step * static_cast<float>(Index);
		const FVector Next = Center + (AxisA * cosf(Angle) + AxisB * sinf(Angle)) * Radius;
		Scene.AddDebugLine(Prev, Next, ShapeColor);
		Prev = Next;
	}
}
