#pragma once

#include "FlatOctree.h"

class FFlatOctreeBase
{
public:
	using FObjectHandle = uint32;

	static constexpr FObjectHandle InvalidHandle = 0;
	static constexpr int32 InvalidNodeIndex = -1;
	static constexpr int32 InvalidObjectSlot = -1;
	static constexpr int32 ChildCount = 8;
	static constexpr int32 MaxBatchLaneCount = 8;

	struct FFrustumBatchClassification
	{
		uint32 VisibleMask = 0;
		// Preserved for future query modes that want to distinguish fully-contained objects.
		uint32 InsideMask = 0;
	};

public:
	FFlatOctreeBase(const FAABB& InWorldBounds, int32 InMaxDepth = 8, int32 InMaxObjectsPerNode = 8);
	virtual ~FFlatOctreeBase();

	FFlatOctreeBase(const FFlatOctreeBase&) = delete;
	FFlatOctreeBase& operator=(const FFlatOctreeBase&) = delete;
	FFlatOctreeBase(FFlatOctreeBase&&) = delete;
	FFlatOctreeBase& operator=(FFlatOctreeBase&&) = delete;

	void Clear();

	FObjectHandle Insert(const FAABB& InBounds, void* InUserData);
	bool Remove(FObjectHandle InHandle);
	bool Update(FObjectHandle InHandle, const FAABB& InNewBounds);

	void QueryFrustum(const FFrustum& InFrustum, TArray<void*>& OutResults) const;
	void QueryFrustumHandles(const FFrustum& InFrustum, TArray<FObjectHandle>& OutResults) const;

	void RaycastAll(const FRay& InRay, TArray<FObjectHandle>& OutHits) const;
	bool RaycastNearest(const FRay& InRay, FObjectHandle& OutHitHandle, float& OutHitDistance) const;

	int32 GetObjectCount() const;
	int32 GetNodeCount() const;
	int32 GetMaxDepth() const;
	int32 GetMaxObjectsPerNode() const;
	int32 GetSplitCount() const;
	int32 GetDeepestNodeDepth() const;

	const FAABB& GetWorldBounds() const;

	bool GetObjectNodeDepth(FObjectHandle InHandle, int32& OutDepth) const;
	bool GetObject(FObjectHandle InHandle, FFlatOctreeObject& OutObject) const;

	FFlatOctreeStats GetStats() const;
	void CollectNodeDebugInfo(TArray<FFlatOctreeNodeDebugInfo>& OutNodes) const;

protected:
	void GatherObjectBounds(
		const int32* InObjectSlots,
		int32 InCount,
		float* OutMinX, float* OutMinY, float* OutMinZ,
		float* OutMaxX, float* OutMaxY, float* OutMaxZ) const;

	virtual int32 GetBatchLaneCount() const = 0;
	virtual FFrustumBatchClassification ClassifyFrustumBatch(const FFrustum& InFrustum, const int32* InObjectSlots, int32 InCount) const = 0;
	virtual uint32 RaycastBatchMask(const FRay& InNormalizedRay, const int32* InObjectSlots, int32 InCount, float* OutEntryDistances) const = 0;

private:
	struct FNode
	{
		FAABB Bounds;
		int32 Depth = 0;
		std::array<int32, ChildCount> Children{};
		TArray<int32> ObjectSlots;

		FNode();
		FNode(const FAABB& InBounds, int32 InDepth);

		bool HasChildren() const;
	};

	struct FObjectSoA
	{
		TArray<FObjectHandle> Handles;
		TArray<void*> UserData;
		TArray<float> MinX;
		TArray<float> MinY;
		TArray<float> MinZ;
		TArray<float> MaxX;
		TArray<float> MaxY;
		TArray<float> MaxZ;
		TArray<int32> NodeIndex;
		TArray<int32> LocalIndex;
		TArray<int32> FreeSlots;
	};

	struct FRayHit
	{
		FObjectHandle Handle = InvalidHandle;
		float Distance = 0.0f;
	};

private:
	int32 AllocateNode(const FAABB& InBounds, int32 InDepth);
	int32 AllocateObjectSlot(FObjectHandle InHandle, const FAABB& InBounds, void* InUserData);
	void ReleaseObjectSlot(int32 InObjectSlot);

	FObjectHandle InsertInternal(const FAABB& InBounds, void* InUserData, FObjectHandle InHandle);
	FObjectHandle InsertRecursive(int32 InNodeIndex, int32 InObjectSlot);

	void TrySplit(int32 InNodeIndex);
	void CreateChildren(int32 InNodeIndex);
	void RedistributeObjects(int32 InNodeIndex);
	void RemoveFromNode(int32 InNodeIndex, int32 InLocalIndex, bool bReleaseObjectSlot);

	void AppendNodeObjectsUnchecked(int32 InNodeIndex, TArray<void*>& OutResults) const;
	void AppendNodeHandlesUnchecked(int32 InNodeIndex, TArray<FObjectHandle>& OutResults) const;
	void QueryFrustumRecursive(int32 InNodeIndex, const FFrustum& InFrustum, EFrustumAABBResult InNodeResult, TArray<void*>& OutResults) const;
	void QueryFrustumHandlesRecursive(int32 InNodeIndex, const FFrustum& InFrustum, EFrustumAABBResult InNodeResult, TArray<FObjectHandle>& OutResults) const;

	void RaycastAllRecursive(int32 InNodeIndex, const FRay& InNormalizedRay, TArray<FRayHit>& OutHits) const;
	void RaycastNearestRecursive(int32 InNodeIndex, const FRay& InNormalizedRay, FObjectHandle& InOutHitHandle, float& InOutHitDistance) const;

	void CollectNodeDebugInfoRecursive(int32 InNodeIndex, TArray<FFlatOctreeNodeDebugInfo>& OutNodes) const;

	bool IsValidObjectSlot(int32 InObjectSlot) const;
	FAABB GetObjectBounds(int32 InObjectSlot) const;
	void SetObjectBounds(int32 InObjectSlot, const FAABB& InBounds);

	int32 GetContainingChildIndex(int32 InNodeIndex, const FAABB& InBounds) const;
	bool CanSplit(int32 InNodeIndex) const;
	bool CanCreateChildren(const FAABB& InBounds) const;
	FAABB BuildChildBounds(const FAABB& InParentBounds, int32 InChildIndex) const;

private:
	FAABB WorldBounds;
	int32 MaxDepth = 0;
	int32 MaxObjectsPerNode = 0;
	FObjectHandle NextHandle = 1;

	int32 SplitCount = 0;
	int32 DeepestNodeDepth = 0;

	TArray<FNode> NodePool;
	FObjectSoA ObjectData;
	std::unordered_map<FObjectHandle, int32> HandleToSlot;
};
