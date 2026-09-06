#include "ShapeCollisionBVH.h"

#include "Math/Intersection.h"
#include "Component/Collision/ShapeComponent.h"

void FShapeCollisionBVH::Build(const TArray<UShapeComponent*>& Shapes)
{
    Leaves.clear();
    BVH.Reset();

    Leaves.reserve(Shapes.size());

    for (uint32 ShapeIndex = 0; ShapeIndex < static_cast<uint32>(Shapes.size()); ++ShapeIndex)
    {
        UShapeComponent* Shape = Shapes[ShapeIndex];
        if (!Shape)
        {
            continue;
        }

        const FBoundingBox Bounds = Shape->GetWorldBoundingBox();
        if (!Bounds.IsValid())
        {
            continue;
        }

        FLeaf& Leaf = Leaves.emplace_back();
        Leaf.Bounds = Bounds;
        Leaf.ShapeComp = Shape;
        Leaf.StableIndex = ShapeIndex;
    }

    if (!Leaves.empty())
    {
        BVH.Build(
            Leaves,
            [](const FLeaf& Leaf)
            {
                return Leaf.Bounds;
            },
            [](const FLeaf& Leaf)
            {
                return Leaf.StableIndex;
            });
    }
}
void FShapeCollisionBVH::QueryOverlappingPairs(TArray<FShapeCollisionPair>& OutPairs) const
{
    OutPairs.clear();

    if (Leaves.empty() || BVH.GetNodes().empty())
    {
        return;
    }

    QueryNodePairOverlaps(0, 0, OutPairs);
}

void FShapeCollisionBVH::QueryNodePairOverlaps(int32 NodeIndexA, int32 NodeIndexB, TArray<FShapeCollisionPair>& OutPairs) const
{
    const TArray<FNode>& Nodes = BVH.GetNodes();
    if (NodeIndexA < 0 || NodeIndexA >= static_cast<int32>(Nodes.size()) ||
        NodeIndexB < 0 || NodeIndexB >= static_cast<int32>(Nodes.size()))
    {
        return;
    }

    const FNode& NodeA = Nodes[NodeIndexA];
    const FNode& NodeB = Nodes[NodeIndexB];
    if (!FMath::IntersectAABB(NodeA.Bounds, NodeB.Bounds))
    {
        return;
    }

    if (NodeA.IsLeaf() && NodeB.IsLeaf())
    {
        QueryLeafPairOverlaps(NodeA, NodeB, OutPairs);
        return;
    }

    if (NodeIndexA == NodeIndexB)
    {
        for (int32 ChildIndexA = 0; ChildIndexA < NodeA.ChildCount; ++ChildIndexA)
        {
            for (int32 ChildIndexB = ChildIndexA; ChildIndexB < NodeA.ChildCount; ++ChildIndexB)
            {
                QueryNodePairOverlaps(NodeA.Children[ChildIndexA], NodeA.Children[ChildIndexB], OutPairs);
            }
        }
        return;
    }

    if (NodeA.IsLeaf())
    {
        for (int32 ChildIndexB = 0; ChildIndexB < NodeB.ChildCount; ++ChildIndexB)
        {
            QueryNodePairOverlaps(NodeIndexA, NodeB.Children[ChildIndexB], OutPairs);
        }
        return;
    }

    if (NodeB.IsLeaf())
    {
        for (int32 ChildIndexA = 0; ChildIndexA < NodeA.ChildCount; ++ChildIndexA)
        {
            QueryNodePairOverlaps(NodeA.Children[ChildIndexA], NodeIndexB, OutPairs);
        }
        return;
    }

    for (int32 ChildIndexA = 0; ChildIndexA < NodeA.ChildCount; ++ChildIndexA)
    {
        for (int32 ChildIndexB = 0; ChildIndexB < NodeB.ChildCount; ++ChildIndexB)
        {
            QueryNodePairOverlaps(NodeA.Children[ChildIndexA], NodeB.Children[ChildIndexB], OutPairs);
        }
    }
}

void FShapeCollisionBVH::QueryLeafPairOverlaps(const FNode& NodeA, const FNode& NodeB, TArray<FShapeCollisionPair>& OutPairs) const
{
    for (int32 LeafOffsetA = 0; LeafOffsetA < NodeA.LeafCount; ++LeafOffsetA)
    {
        const FLeaf& LeafA = Leaves[NodeA.FirstLeaf + LeafOffsetA];

        for (int32 LeafOffsetB = 0; LeafOffsetB < NodeB.LeafCount; ++LeafOffsetB)
        {
            const FLeaf& LeafB = Leaves[NodeB.FirstLeaf + LeafOffsetB];

            if (LeafA.StableIndex >= LeafB.StableIndex)
            {
                continue;
            }

            if (!FMath::IntersectAABB(LeafA.Bounds, LeafB.Bounds))
            {
                continue;
            }

            OutPairs.push_back({ LeafA.ShapeComp, LeafB.ShapeComp });
        }
    }
}
