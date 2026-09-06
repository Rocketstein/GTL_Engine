#include "ASpotlight.h"
#include "SimpleJSON/json.hpp"

DEFINE_CLASS(ASpotlight, AActor)
REGISTER_FACTORY(ASpotlight)

ASpotlight::ASpotlight() {
	// Creates an icon component as a clickable primitive root
	UBillBoardComponent* Icon = FObjectManager::Get().CreateObject<UBillBoardComponent>();
	Icon->LoadTexture(L"Assets/Icons/light_icon.png");
	AddComponent(Icon);

	// Spotlight component
	AddComponent<USpotlightComponent>();
	UpdateShine();
}

USpotlightComponent* ASpotlight::GetSpotlightComp() const {
	// Light actor is closed to modular modifications
	auto* Obj = Components[1];
	return USpotlightComponent::Cast(Obj);
}

void ASpotlight::UpdateShine() {
	// Light actor is closed to modular modifications
	auto* Obj = Components[1];
	USpotlightComponent* Spotlight = USpotlightComponent::Cast(Obj);
	Spotlight->UpdateLight();
}

void ASpotlight::RenderLines(FBatchedLine* BatchLine) {
	// Light actor is closed to modular modifications
	auto* Obj = Components[1];
	USpotlightComponent* Spotlight = USpotlightComponent::Cast(Obj);
	Spotlight->RenderLines(BatchLine);
}

void ASpotlight::Tick(float DeltaTime) {
	for (UActorComponent* Comp : Components) {
		Comp->ExcuteTick(DeltaTime);
	}
}

void ASpotlight::Serialize(json::JSON& j) const {
	AActor::Serialize(j);
}

void ASpotlight::Deserialize(const json::JSON& j) {
	AActor::Deserialize(j);
}