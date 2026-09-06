#pragma once

#include "Core/CoreTypes.h"
#include "Collision/BVH/ShapeCollisionBVH.h"

class UWorld;
class UShapeComponent;
struct FCollisionContact;

class FCollisionManager
{
public:
    void Update(UWorld& World);
private:
    void CollectShapes(UWorld& World, TArray<UShapeComponent*>& OutShapes);
    bool CanCollide(UShapeComponent* ShapeA, UShapeComponent* ShapeB);
    void ResolveBlock(UShapeComponent* ShapeA, UShapeComponent* ShapeB, const FCollisionContact& Contact);

    FShapeCollisionBVH ShapeBVH;
};
