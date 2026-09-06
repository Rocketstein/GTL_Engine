#pragma once

#include "Core/CoreTypes.h"
#include "Collision/Collision2DShapeGeometry.h"

class UCollider2DComponent;

class FSpatialHash
{
public:
    void Clear();
    void Build(const TArray<UCollider2DComponent*>& Colliders);
    void QueryPairs(TArray<FCollision2DPair>& OutPairs) const;
    void QueryRadius(const FVector2& Position, float Radius, TArray<UCollider2DComponent*>& OutColliders) const;

    void SetCellSize(float NewCellSize);

private:
    FCollision2DCell GetCell(const FVector2& Position) const;
    void InsertCollider(UCollider2DComponent* Collider);

private:
    float CellSize = 2.0f;
    TMap<FCollision2DCellKey, TArray<UCollider2DComponent*>> Cells;
};
