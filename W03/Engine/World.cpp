#include "World.h"
#include <SimpleJSON/json.hpp>

DEFINE_CLASS(UWorld, UObject)
REGISTER_FACTORY(UWorld)

UWorld::~UWorld(){
	EndPlay();
}

void UWorld::Init() {
	// Create a new scene if scene list in SceneManager is empty. It should be implicitly set active.
	if (SceneManager.IsEmpty()) {
		SceneManager.AddScene(FObjectManager::Get().CreateObject<UScene>());
	}
}

void UWorld::BeginPlay(){
	if (UScene* Scene = SceneManager.GetActiveScene()) Scene->BeginPlay();
}

void UWorld::Tick(float DeltaTime) {
	if (UScene* Scene = SceneManager.GetActiveScene()) Scene->Tick(DeltaTime);
}

void UWorld::EndPlay() {
	if (UScene* Scene = SceneManager.GetActiveScene()) Scene->EndPlay();
}

void UWorld::Serialize(json::JSON& j) const {
	SerializeHeader(j);
	json::JSON SceneArray = json::Array();
	for (UScene* Scene : SceneManager.GetScenes()) {
		Scene->Serialize(SceneArray);
	}
}

void UWorld::Deserialize(const json::JSON& j) {

}