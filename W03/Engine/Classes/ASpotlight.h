#pragma once
#include "AActor.h"
#include "World/Primitives/Primitives.h"
#include "Core/Common.h"
#include "Render/Scene/BatchedLine.h"
#include "World/Light/SpotLightComponent.h"

using namespace common::constants;

class ASpotlight : public AActor {
private:
	// Everything is fixed to the actor. Do not allow modular modification from the outside
	using AActor::AddComponent;
	using AActor::RemoveComponent;
public:
	DECLARE_CLASS(ASpotlight, AActor)

	ASpotlight();
	~ASpotlight() = default;

	void BeginPlay() override {}
	void Tick(float DeltaTime) override;
	void EndPlay() override {}

	USpotlightComponent* GetSpotlightComp() const;

	// Rendering
	void UpdateShine();
	void RenderLines(FBatchedLine* BatchLine);

	void Serialize(json::JSON& j) const override;
	void Deserialize(const json::JSON& j) override;
};