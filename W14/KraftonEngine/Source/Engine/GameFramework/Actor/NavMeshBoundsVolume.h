#pragma once

#include "GameFramework/AActor.h"
#include "Object/Ptr/WeakObjectPtr.h"

class UBoxComponent;

#include "Source/Engine/GameFramework/Actor/NavMeshBoundsVolume.generated.h"

UCLASS()
class ANavMeshBoundsVolume : public AActor
{
public:
	GENERATED_BODY()
	ANavMeshBoundsVolume() = default;
	~ANavMeshBoundsVolume() override = default;

	void InitDefaultComponents(const FVector& Extent = FVector(1000.0f, 1000.0f, 300.0f));
	void PostDuplicate() override;
	void PostEditProperty(const char* PropertyName) override;

	UFUNCTION(Pure, Category="Navigation")
	UBoxComponent* GetBoundsComponent() const;
	UFUNCTION(Pure, Category="Navigation")
	bool ContainsPoint(const FVector& Point) const;

private:
	TWeakObjectPtr<UBoxComponent> BoundsComponent = nullptr;
};
