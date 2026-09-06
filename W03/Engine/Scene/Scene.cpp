#include "Scene.h"
#include "SimpleJSON/json.hpp"

DEFINE_CLASS(UScene, UObject)
REGISTER_FACTORY(UScene)

UScene::~UScene() {
    EndPlay();
}

void UScene::BeginPlay() {
    for (AActor* Actor : Actors) {
        if (Actor && !Actor->bPendingKill) {
            Actor->BeginPlay();
        }
    }
}

void UScene::Tick(float DeltaTime) {
    for (AActor* Actor : Actors) {
        if (Actor && !Actor->bPendingKill) {
            Actor->Tick(DeltaTime);
        }
    }
}

void UScene::EndPlay() {
    Actors.clear();
    if (ActiveCamera) ActiveCamera = nullptr;
}

void UScene::Serialize(json::JSON& j) const {
    // Scene metadata (ClassName/UUID/InternalIndex) written by SerializeHeader.
    // Actors are iterated by SaveSceneAsJSON — nothing extra here.
}

void UScene::Deserialize(const json::JSON& j) {

}