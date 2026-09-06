#pragma once
#include "Object/Object.h"
#include "World/PrimitiveComponent.h"
#include "Camera.h"
#include "Classes/AActor.h"

class UWorld;

// This class replaces the original role of ULevel in Unreal implmentation
class UScene : public UObject {
public:
	DECLARE_CLASS(UScene, UObject)
	UScene() = default;
	~UScene();

	void BeginPlay();
	void Tick(float DeltaTIme);
	void EndPlay();

	void AddActor(AActor* Actor) { Actors.push_back(Actor); }
	const TArray<AActor*>& GetActors() const { return Actors; }

	template <typename T>
	AActor* SpawnPrimitiveActor(const FVector& Location) {
		AActor* Actor = FObjectManager::Get().CreateObject<AActor>();
		Actor->SetOwnerUUID(UUID);
		Actor->SetActorLocation(Location);
		Actor->AddComponent<T>();
		Actors.push_back(Actor);
		return Actor;
	}

	template <typename T>
	AActor* SpawnUtilActor(const FVector& Location) {
		T* Actor = FObjectManager::Get().CreateObject<T>();
		Actor->SetOwnerUUID(UUID);
		Actor->SetActorLocation(Location);
		Actors.push_back(Actor);
		return Actor;
	}

	void Serialize(json::JSON& j) const override;
	void Deserialize(const json::JSON& j) override;

private:
	TArray<AActor*> Actors;
	UCamera* ActiveCamera = nullptr;	// Unused for now
};