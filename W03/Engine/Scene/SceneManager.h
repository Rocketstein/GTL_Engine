#pragma once
#include "Scene.h"
#include "Classes/Actors.h"

class FSceneManager {
public:
	void Init();
	void LoadScene(const FString& SceneName);
	void UnloadScene(UScene* Scene);
	void SetActiveScene(UScene* Scene);
	UScene* GetActiveScene();
	TArray<UScene*> GetScenes() const { return LoadedScenes; }
	void ReplaceScene(UScene* Scene);
	void AddScene(UScene* Scene);
	void AddNewScene();

	// Use with caution: this function may leave the world with 0 active scene
	uint32 RemoveActiveScene();
	bool IsEmpty() { return LoadedScenes.empty(); }

	template <typename T>
	AActor* SpawnPrimitiveActor(const FVector& Location) {
		if (!ActiveScene) { return nullptr; }
		return ActiveScene->SpawnPrimitiveActor<T>(Location);
	}

	template <typename T>
	AActor* SpawnUtilActor(const FVector& Location) {
		if (!ActiveScene) { return nullptr; }
		return ActiveScene->SpawnUtilActor<T>(Location);
	}

private:
	UScene* ActiveScene = nullptr;
	TArray<UScene*> LoadedScenes;
};