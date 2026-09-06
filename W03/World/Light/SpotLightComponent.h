#pragma once
#include "LightComponent.h"
#include "World/Primitives/Primitives.h"
#include "Core/Common.h"
#include "Render/Scene/BatchedLine.h"

class USpotlightComponent : public ULightComponent {
private:
	// Cone stuffs
	float ConeHeight = common::constants::Actors::ASpotlight::DefaultConeHeight;
	float Radius = common::constants::Actors::ASpotlight::DefaultRadius;
	float Yaw = 0.0f;
	float Pitch = 0.0f;
	int NumCircleVertex = common::constants::Actors::ASpotlight::DefaultCircleVertex;

	FVector UnitNormal;
	FVector UnitOrthogonal;
	FVector CircleCenter;

	TArray<FVector> CircleVertices;

public:
	DECLARE_CLASS(USpotlightComponent, ULightComponent)

	USpotlightComponent();
	~USpotlightComponent() = default;

	void BeginPlay() override {}
	void TickComponent(float DeltaTime) override;
	void EndPlay() override {}

	void UpdateWorldMatrix() override;

	void UpdateLight() override;
	void SetConeHeight(float NewConeHeight) { ConeHeight = NewConeHeight; bIsLightDirty = true; }
	void SetConeRadius(float NewRadius) { Radius = NewRadius; bIsLightDirty = true; }
	void SetNumVertex(size_t Num) { NumCircleVertex = Num; bIsLightDirty = true; }
	void SetYaw(float NewYaw) { Yaw = NewYaw; bIsLightDirty = true; }
	void SetPitch(float NewPitch) { Pitch = NewPitch; bIsLightDirty = true; }
	void SetRadius(float NewRadius) { if (NewRadius >= 0) Radius = NewRadius; bIsLightDirty = true; }

	float GetYaw() const { return Yaw; }
	float GetPitch() const { return Pitch; }
	float GetConeHeight() const { return ConeHeight; }
	float GetRadius() const { return Radius; }
	int GetNumVertices() const { return NumCircleVertex; }

	// Rendering
	void Render() override { /* To be Implemented */ }
	void RenderLines(FBatchedLine* BatchLine) override;

	void Serialize(json::JSON& j) const override;
	void Deserialize(const json::JSON& j) override;
};
