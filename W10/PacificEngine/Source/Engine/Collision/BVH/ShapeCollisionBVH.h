#pragma once

#include "Collision/BVH/BVH.h"

class UShapeComponent;

struct FShapeCollisionPair
{
    UShapeComponent* ShapeCompA = nullptr;
    UShapeComponent* ShapeCompB = nullptr;
};

class FShapeCollisionBVH
{
public:
    static constexpr int32 ChildFanout = 8;
    static constexpr int32 MaxLeafSize = 16;

    struct FLeaf
    {
        FBoundingBox Bounds;
        UShapeComponent* ShapeComp = nullptr;
        uint32 StableIndex = 0;
    };

    using FBVH = TBVH<FLeaf, ChildFanout, MaxLeafSize>;
    using FNode = FBVH::FNode;


private:
    TBVH<FLeaf, ChildFanout, MaxLeafSize> BVH;
    TArray<FLeaf> Leaves;

public:
    void Build(const TArray<UShapeComponent*>& Shapes);
    void QueryOverlappingPairs(TArray<FShapeCollisionPair>& OutPairs) const;

    const TArray<FLeaf>& GetLeaves() const { return Leaves; }
    const TArray<FNode>& GetNodes() const { return BVH.GetNodes(); }

private:
    void QueryNodePairOverlaps(int32 NodeIndexA, int32 NodeIndexB, TArray<FShapeCollisionPair>& OutPairs) const;
    void QueryLeafPairOverlaps(const FNode& NodeA, const FNode& NodeB, TArray<FShapeCollisionPair>& OutPairs) const;
};
