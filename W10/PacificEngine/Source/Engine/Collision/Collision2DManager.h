#pragma once

#include "Core/CoreTypes.h"
#include "Collision/SpatialHash.h"

class UCollider2DComponent;
class UWorld;
struct FCollision2DContact;
struct FCollision2DShapeGeometry;

class FCollision2DManager
{
public:
    void Update(UWorld& World);

    void OverlapCircle(const FVector2& Position, float Radius, TArray<UCollider2DComponent*>& OutColliders) const
    {
        SpatialHash.QueryRadius(Position, Radius, OutColliders);
    }

private:
    void CollectColliders(UWorld& World, TArray<UCollider2DComponent*>& OutColliders);
	void BuildCollisionPairs(const TArray<UCollider2DComponent*>& Colliders, TArray<FCollision2DPair>& OutPairs);
	void ProcessCollisionPairs(const TArray<FCollision2DPair>& CollisionPairs, TMap<uint64, FCollision2DPair>& OutCurrentOverlapPairs);
	void ProcessCollisionPair(UCollider2DComponent* ColliderA, UCollider2DComponent* ColliderB, TMap<uint64, FCollision2DPair>& OutCurrentOverlapPairs);
    bool ComputePenetration(const FCollision2DShapeGeometry& A, const FCollision2DShapeGeometry& B, FCollision2DContact& OutContact) const;
	void HandleBlockingCollision(UCollider2DComponent* ColliderA, UCollider2DComponent* ColliderB, const FCollision2DContact& Contact);
	void HandleOverlapCollision(UCollider2DComponent* ColliderA, UCollider2DComponent* ColliderB, TMap<uint64, FCollision2DPair>& OutCurrentOverlapPairs);
    void ResolveBlock(UCollider2DComponent* ColliderA, UCollider2DComponent* ColliderB, const FCollision2DContact& Contact);
    void PruneInvalidOverlapPairs(TMap<uint64, FCollision2DPair>& InOutOverlapPairs);
	void DispatchEndOverlapEvents(const TMap<uint64, FCollision2DPair>& CurrentOverlapPairs);

	private:
    FSpatialHash SpatialHash;
	TMap<uint64, FCollision2DPair> PreviousOverlapPairs;
};
