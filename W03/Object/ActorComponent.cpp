#include "ActorComponent.h"
#include "SimpleJSON/json.hpp"

DEFINE_CLASS(UActorComponent, UObject)

void UActorComponent::BeginPlay()
{
	if (bAutoActivate)
	{
		Activate();
	}
}

void UActorComponent::Activate()
{
	bCanEverTick = true;
}

void UActorComponent::Deactivate()
{
	bCanEverTick = false;
}

void UActorComponent::ExcuteTick(float DeltaTime)
{
	if (bCanEverTick == false || bIsActive == false)
	{
		return;
	}

	TickComponent(DeltaTime);
}

void UActorComponent::SetActive(bool bNewActive)
{
	if (bNewActive == bIsActive)
	{
		return;
	}

	bIsActive = bNewActive;

	if (bIsActive)
	{
		Activate();
	}
	else
	{
		Deactivate();
	}
}

void UActorComponent::Serialize(json::JSON& j) const {
	json::JSON CompJSON = json::Object();
	CompJSON["UUID"] = UUID;
	CompJSON["ClassName"] = GetTypeInfo()->name;
	j["Scene"]["Components"]["ActorComponents"].append(CompJSON);
}

void UActorComponent::Deserialize(const json::JSON& j) {

}