#pragma once
#include "Math/Vector.h"

class AActor;
class UPrimitiveComponent;

enum class EOverlapBehaviour {
	Ignore,
	Hit,
	Overlap,
};

struct FHitResult {
	bool	bBlocking;
	FVector Location;
	FVector ImpactLocation;
	FVector ImpactNormal;
	float   PenetrationDepth;

	UPrimitiveComponent* Component = nullptr;
	AActor* GetActor();
};

struct FOverlapInfo {
	FHitResult HitResult;
};