#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Geometry/Primitives/Ray.h"

/*
        Flat / pooled octree design notes:
        - Nodes live in a contiguous node pool and reference children by pool index.
        - Objects live in a SoA store (handle/user data + 6 float arrays for bounds).
        - Each node stores only object slot indices. Bounds stay in the object SoA.
        - This is still a regular octree, not a loose octree.
        - An object descends only when it is strictly contained by exactly one child region.
          If it touches a split plane or spans multiple children, it stays in the current node.
        - World bounds policy matches the existing octree:
          fully outside -> reject, partially overlapping -> keep at root if it cannot descend.
        - Merge is intentionally omitted to keep the pool stable and update/remove paths simple.
*/

struct FFlatOctreeObject
{
    uint32          Handle = 0;
    Geometry::FAABB Bounds;
    void           *UserData = nullptr;
};

struct FFlatOctreeStats
{
    int32 NodeCount = 0;
    int32 ObjectCount = 0;
    int32 ConfiguredMaxDepth = 0;
    int32 DeepestNodeDepth = 0;
    int32 SplitCount = 0;
};

struct FFlatOctreeNodeDebugInfo
{
    Geometry::FAABB Bounds;
    int32           Depth = 0;
    int32           LocalObjectCount = 0;
    bool            bHasChildren = false;
};

class FFlatOctree
{
  public:
    using FObjectHandle = uint32;

    static constexpr FObjectHandle InvalidHandle = 0;
    static constexpr int32         InvalidNodeIndex = -1;
    static constexpr int32         InvalidObjectSlot = -1;
    static constexpr int32         ChildCount = 8;

  public:
    FFlatOctree(const Geometry::FAABB &InWorldBounds, int32 InMaxDepth = 8,
                int32 InMaxObjectsPerNode = 8);
    ~FFlatOctree();

    FFlatOctree(const FFlatOctree &) = delete;
    FFlatOctree &operator=(const FFlatOctree &) = delete;
    FFlatOctree(FFlatOctree &&) = delete;
    FFlatOctree &operator=(FFlatOctree &&) = delete;

    void Clear();

    FObjectHandle Insert(const Geometry::FAABB &InBounds, void *InUserData);
    bool          Remove(FObjectHandle InHandle);
    bool          Update(FObjectHandle InHandle, const Geometry::FAABB &InNewBounds);

    void QueryFrustum(const FFrustum &InFrustum, TArray<void *> &OutResults) const;
    void QueryFrustumHandles(const FFrustum &InFrustum, TArray<FObjectHandle> &OutResults) const;

    void Ray
        All(const Geometry::FRay &InRay, TArray<FObjectHandle> &OutHits) const;
    bool RaycastNearest(const Geometry::FRay &InRay, FObjectHandle &OutHitHandle,
                        float &OutHitDistance) const;

    int32 GetObjectCount() const;
    int32 GetNodeCount() const;
    int32 GetMaxDepth() const;
    int32 GetMaxObjectsPerNode() const;
    int32 GetSplitCount() const;
    int32 GetDeepestNodeDepth() const;

    const Geometry::FAABB &GetWorldBounds() const;

    bool GetObjectNodeDepth(FObjectHandle InHandle, int32 &OutDepth) const;
    bool GetObject(FObjectHandle InHandle, FFlatOctreeObject &OutObject) const;

    FFlatOctreeStats GetStats() const;
    void             CollectNodeDebugInfo(TArray<FFlatOctreeNodeDebugInfo> &OutNodes) const;

  private:
    struct FNode
    {
        Geometry::FAABB               Bounds;
        int32                         Depth = 0;
        std::array<int32, ChildCount> Children{};
        TArray<int32>                 ObjectSlots;

        FNode();
        FNode(const Geometry::FAABB &InBounds, int32 InDepth);

        bool HasChildren() const;
    };

    struct FObjectSoA
    {
        TArray<FObjectHandle> Handles;
        TArray<void *>        UserData;
        TArray<float>         MinX;
        TArray<float>         MinY;
        TArray<float>         MinZ;
        TArray<float>         MaxX;
        TArray<float>         MaxY;
        TArray<float>         MaxZ;
        TArray<int32>         NodeIndex;
        TArray<int32>         LocalIndex;
        TArray<int32>         FreeSlots;
    };

    struct FRayHit
    {
        FObjectHandle Handle = InvalidHandle;
        float         Distance = 0.0f;
    };

  private:
    int32 AllocateNode(const Geometry::FAABB &InBounds, int32 InDepth);
    int32 AllocateObjectSlot(FObjectHandle InHandle, const Geometry::FAABB &InBounds,
                             void *InUserData);
    void  ReleaseObjectSlot(int32 InObjectSlot);

    FObjectHandle InsertInternal(const Geometry::FAABB &InBounds, void *InUserData,
                                 FObjectHandle InHandle);
    FObjectHandle InsertRecursive(int32 InNodeIndex, int32 InObjectSlot);

    void TrySplit(int32 InNodeIndex);
    void CreateChildren(int32 InNodeIndex);
    void RedistributeObjects(int32 InNodeIndex);
    void RemoveFromNode(int32 InNodeIndex, int32 InLocalIndex, bool bReleaseObjectSlot);

    void AppendNodeObjectsUnchecked(int32 InNodeIndex, TArray<void *> &OutResults) const;
    void AppendNodeHandlesUnchecked(int32 InNodeIndex, TArray<FObjectHandle> &OutResults) const;
    void QueryFrustumRecursive(int32 InNodeIndex, const FFrustum &InFrustum,
                               EFrustumAABBResult InNodeResult, TArray<void *> &OutResults) const;
    void QueryFrustumHandlesRecursive(int32 InNodeIndex, const FFrustum &InFrustum,
                                      EFrustumAABBResult     InNodeResult,
                                      TArray<FObjectHandle> &OutResults) const;

    void RaycastAllRecursive(int32 InNodeIndex, const Geometry::FRay &InNormalizedRay,
                             TArray<FRayHit> &OutHits) const;
    void RaycastNearestRecursive(int32 InNodeIndex, const Geometry::FRay &InNormalizedRay,
                                 FObjectHandle &InOutHitHandle, float &InOutHitDistance) const;

    void CollectNodeDebugInfoRecursive(int32                             InNodeIndex,
                                       TArray<FFlatOctreeNodeDebugInfo> &OutNodes) const;

    bool            IsValidObjectSlot(int32 InObjectSlot) const;
    Geometry::FAABB GetObjectBounds(int32 InObjectSlot) const;
    void            SetObjectBounds(int32 InObjectSlot, const Geometry::FAABB &InBounds);

    int32 GetContainingChildIndex(int32 InNodeIndex, const Geometry::FAABB &InBounds) const;
    bool  CanSplit(int32 InNodeIndex) const;
    bool  CanCreateChildren(const Geometry::FAABB &InBounds) const;
    Geometry::FAABB BuildChildBounds(const Geometry::FAABB &InParentBounds,
                                     int32                  InChildIndex) const;

  private:
    Geometry::FAABB WorldBounds;
    int32           MaxDepth = 0;
    int32           MaxObjectsPerNode = 0;
    FObjectHandle   NextHandle = 1;

    int32 SplitCount = 0;
    int32 DeepestNodeDepth = 0;

    TArray<FNode>                            NodePool;
    FObjectSoA                               ObjectData;
    std::unordered_map<FObjectHandle, int32> HandleToSlot;
};
