#include "Classes/AActor.h"
#include "Scene/Scene.h"
#include "SimpleJSON/json.hpp"

DEFINE_CLASS(AActor, UObject)
REGISTER_FACTORY(AActor)

AActor::~AActor() {
	for (auto* Comp : Components) {
		if (Comp) {
			//UObjectManager::Get().DestroyObject(Comp);
		}
	}

	Components.clear();
	RootComponent = nullptr;
}

USceneComponent* AActor::AddComponent(USceneComponent* ExistingComp) {
	if (!ExistingComp) return nullptr;

	if (!RootComponent) {
		RootComponent = ExistingComp;
		RootComponent->SetWorldLocation(PendingActorLocation);
	}
	else {
		ExistingComp->SetParent(RootComponent);
	}

	RegisterComponentRecursive(ExistingComp);
	return ExistingComp;
}

void AActor::RemoveComponent(USceneComponent* Component) {
	if (!Component) return;

	auto it = std::find(Components.begin(),
		Components.end(), Component);
	if (it != Components.end())
		Components.erase(it);

	if (RootComponent == Component)
		RootComponent = Components.empty()
		? nullptr
		: Components[0];

	FObjectManager::Get().DestroyObject(Component);
}

void AActor::SetRootComponent(USceneComponent* Comp) {
	if (!Comp) return;
	RootComponent = Comp;
	Components.clear(); // rebuild from scratch
	RegisterComponentRecursive(Comp);
}

FVector AActor::GetActorLocation() const {
	if (RootComponent) {
		return RootComponent->GetWorldLocation();
	}
	return FVector(0, 0, 0);
}

void AActor::SetActorLocation(const FVector& NewLocation) {
	PendingActorLocation = NewLocation;

	if (RootComponent) {
		RootComponent->SetWorldLocation(NewLocation);
	}
}

void AActor::SetActorRotation(const FVector& Rotation) {
	if (RootComponent) {
		RootComponent->SetRelativeRotation(Rotation);
	}
}

void AActor::SetActorScale(const FVector& Scale) {
	if (RootComponent) {
		RootComponent->SetRelativeScale(Scale);
	}
}

FVector AActor::GetActorLocation() {
	FVector Vec(0, 0, 0);
	if (RootComponent) {
		Vec = RootComponent->GetWorldLocation();
	}
	return Vec;
}

FVector AActor::GetActorRotation() {
	FVector Vec(0, 0, 0);
	if (RootComponent) {
		Vec = RootComponent->GetRelativeRotation();
	}
	return Vec;
}

FVector AActor::GetActorScale() {
	FVector Vec(0, 0, 0);
	if (RootComponent) {
		Vec = RootComponent->GetRelativeScale();
	}
	return Vec;
}

void AActor::RegisterComponentRecursive(USceneComponent* Comp) {
	if (!Comp) return;

	// Avoid duplicates
	auto it = std::find(Components.begin(), Components.end(), Comp);
	if (it == Components.end()) {
		Comp->SetOwningActor(this);
		Components.push_back(Comp);
	}

	for (USceneComponent* Child : Comp->GetChildren()) {
		RegisterComponentRecursive(Child);
	}
}

void AActor::Serialize(json::JSON& j) const {
	j["bVisible"] = bVisible;
	j["RootComponentUUID"] = RootComponent ? (int)RootComponent->UUID : 0;
}

void AActor::Deserialize(const json::JSON& j) {
	bVisible = const_cast<json::JSON&>(j)["bVisible"].ToBool();
	// RootComponentUUID resolved by LinkReferences
}