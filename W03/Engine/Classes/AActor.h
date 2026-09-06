#pragma once
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "World/SceneComponent.h"

class UScene;

class AActor : public UObject{
public:
	DECLARE_CLASS(AActor, UObject)

	// Lifecycle
	AActor() = default;
	~AActor() override;

	virtual void BeginPlay() {}
	virtual void Tick(float DeltaTime) 
	{
		for (USceneComponent* comp : GetComponents()) {
			comp->Update(DeltaTime);
		}
	}
	virtual void EndPlay() {}

	// Component management
	template<typename T>
	T* AddComponent() {
		T* component = FObjectManager::Get().CreateObject<T>();

		component->SetOwningActor(this);

		// First component added becomes the root
		if (!RootComponent)
		{
			RootComponent = component;
			RootComponent->SetWorldLocation(PendingActorLocation);
		}
		else
		{
			// Attach to root by default
			component->SetParent(RootComponent);
		}

		Components.push_back(component);
		return component;
	}

	USceneComponent* AddComponent(USceneComponent* ExistingComp);
	void             RemoveComponent(USceneComponent* Component);
	void             SetRootComponent(USceneComponent* Comp);

	USceneComponent*                 GetRootComponent() const { return RootComponent; }
	const TArray<USceneComponent*>&  GetComponents()    const { return Components; }

	// Transform
	FVector GetActorLocation() const;
	virtual void SetActorLocation(const FVector& Location);
	virtual void SetActorRotation(const FVector& Rotation);
	virtual void SetActorScale(const FVector& Scale);
	FVector GetActorLocation();
	FVector GetActorRotation();
	FVector GetActorScale();
	virtual void Transformed() {}

	FVector GetActorForward() const
	{
		if (RootComponent)
			return RootComponent->GetForwardVector();
		return FVector(0, 0, 1);
	}

	// Owner
	void   SetOwnerUUID(uint32 SceneUUID) { OwningSceneUUID = SceneUUID; }
	uint32 GetOwnerUUID()           const { return OwningSceneUUID; }

	// Visibility
	bool IsVisible()              const { return bVisible; }
	void SetVisible(bool Visible)       { bVisible = Visible; }

	void Serialize(json::JSON& j) const override;
	void Deserialize(const json::JSON& j) override;

protected:
	USceneComponent* RootComponent = nullptr;
	uint32 OwningSceneUUID = -1;

	FVector PendingActorLocation = FVector(0, 0, 0);
	bool bVisible = true;

	TArray<USceneComponent*> Components;

private:
	void RegisterComponentRecursive(USceneComponent* Comp);
};
