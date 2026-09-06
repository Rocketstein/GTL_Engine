#pragma once
#include "Object/Object.h"
#include "Scene/Scene.h"
#include "Classes/AActor.h"
#include "Engine/Scene/Camera.h"
#include "Engine/Core/InputSystem.h"
#include "Scene/SceneManager.h"

class UWorld : public UObject {
public:
    DECLARE_CLASS(UWorld, UObject)
    UWorld() = default;
    ~UWorld() override;

    void Init();
    void BeginPlay();
    void Tick(float  DeltaTime);
    void EndPlay();

    FSceneManager& GetSceneManager() { return SceneManager; }
    UScene* GetActiveScene() { return SceneManager.GetActiveScene(); }

    template <typename T>
    AActor* SpawnPrimitiveActor(const FVector& Location) {
        AActor* Actor = SceneManager.SpawnPrimitiveActor<T>(Location);
        Actor->BeginPlay();
        return Actor;
    }

    template <typename T>
    AActor* SpawnUtilActor(const FVector& Location) {
        AActor* Actor = SceneManager.SpawnUtilActor<T>(Location);
        Actor->BeginPlay();
        return Actor;
    }

    void Serialize(json::JSON& j) const override;
    void Deserialize(const json::JSON& j) override;

private:
    FSceneManager SceneManager;     // Has no independent lifetime = value member
    UScene* CurrentScene = nullptr;
};