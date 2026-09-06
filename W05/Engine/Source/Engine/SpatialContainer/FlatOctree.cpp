#include "FlatOctree.h"
using namespace Geometry;

namespace
{
	bool IntersectRayAABB(
		const FRay& InNormalizedRay,
		float InMinX, float InMinY, float InMinZ,
		float InMaxX, float InMaxY, float InMaxZ,
		float& OutEntryDistance,
		float& OutExitDistance)
	{
		float TMin = 0.0f;
		float TMax = (std::numeric_limits<float>::max)();

		const float Origin[3] = { InNormalizedRay.Origin.X, InNormalizedRay.Origin.Y, InNormalizedRay.Origin.Z };
		const float Direction[3] = { InNormalizedRay.Direction.X, InNormalizedRay.Direction.Y, InNormalizedRay.Direction.Z };
		const float MinValue[3] = { InMinX, InMinY, InMinZ };
		const float MaxValue[3] = { InMaxX, InMaxY, InMaxZ };

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (std::abs(Direction[Axis]) <= FMath::SmallNumber)
			{
				if (Origin[Axis] < MinValue[Axis] || Origin[Axis] > MaxValue[Axis])
				{
					return false;
				}

				continue;
			}

			const float InvDirection = 1.0f / Direction[Axis];
			float AxisEntry = (MinValue[Axis] - Origin[Axis]) * InvDirection;
			float AxisExit = (MaxValue[Axis] - Origin[Axis]) * InvDirection;

			if (AxisEntry > AxisExit)
			{
				std::swap(AxisEntry, AxisExit);
			}

			TMin = (std::max)(TMin, AxisEntry);
			TMax = (std::min)(TMax, AxisExit);

			if (TMin > TMax)
			{
				return false;
			}
		}

		if (TMax < 0.0f)
		{
			return false;
		}

		OutEntryDistance = TMin < 0.0f ? 0.0f : TMin;
		OutExitDistance = TMax;
		return true;
	}

	bool IntersectRayAABB(const FRay& InNormalizedRay, const FAABB& InBounds, float& OutEntryDistance, float& OutExitDistance)
	{
		return IntersectRayAABB(
			InNormalizedRay,
			InBounds.Min.X, InBounds.Min.Y, InBounds.Min.Z,
			InBounds.Max.X, InBounds.Max.Y, InBounds.Max.Z,
			OutEntryDistance,
			OutExitDistance);
	}
}

FFlatOctree::FNode::FNode()
{
	Children.fill(InvalidNodeIndex);
}

FFlatOctree::FNode::FNode(const FAABB& InBounds, int32 InDepth)
	: Bounds(InBounds)
	, Depth(InDepth)
{
	Children.fill(InvalidNodeIndex);
}

bool FFlatOctree::FNode::HasChildren() const
{
	return Children[0] != InvalidNodeIndex;
}

FFlatOctree::FFlatOctree(const FAABB& InWorldBounds, int32 InMaxDepth, int32 InMaxObjectsPerNode)
	: WorldBounds(InWorldBounds)
	, MaxDepth((std::max)(0, InMaxDepth))
	, MaxObjectsPerNode((std::max)(1, InMaxObjectsPerNode))
	, NextHandle(1)
	, SplitCount(0)
	, DeepestNodeDepth(0)
{
	assert(WorldBounds.IsValid());
	Clear();
}

FFlatOctree::~FFlatOctree() = default;

void FFlatOctree::Clear()
{
	NodePool.clear();
	NodePool.emplace_back(WorldBounds, 0);

	ObjectData.Handles.clear();
	ObjectData.UserData.clear();
	ObjectData.MinX.clear();
	ObjectData.MinY.clear();
	ObjectData.MinZ.clear();
	ObjectData.MaxX.clear();
	ObjectData.MaxY.clear();
	ObjectData.MaxZ.clear();
	ObjectData.NodeIndex.clear();
	ObjectData.LocalIndex.clear();
	ObjectData.FreeSlots.clear();

	HandleToSlot.clear();
	NextHandle = 1;
	SplitCount = 0;
	DeepestNodeDepth = 0;
}

FFlatOctree::FObjectHandle FFlatOctree::Insert(const FAABB& InBounds, void* InUserData)
{
	if (!InBounds.IsValid())
	{
		return InvalidHandle;
	}

	const FObjectHandle NewHandle = NextHandle;
	if (InsertInternal(InBounds, InUserData, NewHandle) == InvalidHandle)
	{
		return InvalidHandle;
	}

	++NextHandle;
	return NewHandle;
}

bool FFlatOctree::Remove(FObjectHandle InHandle)
{
	const auto SlotIt = HandleToSlot.find(InHandle);
	if (SlotIt == HandleToSlot.end())
	{
		return false;
	}

	const int32 ObjectSlot = SlotIt->second;
	RemoveFromNode(ObjectData.NodeIndex[ObjectSlot], ObjectData.LocalIndex[ObjectSlot], true);
	return true;
}

bool FFlatOctree::Update(FObjectHandle InHandle, const FAABB& InNewBounds)
{
	const auto SlotIt = HandleToSlot.find(InHandle);
	if (SlotIt == HandleToSlot.end())
	{
		return false;
	}

	if (!InNewBounds.IsValid() || !WorldBounds.Intersects(InNewBounds))
	{
		Remove(InHandle);
		return false;
	}

	const int32 ObjectSlot = SlotIt->second;
	RemoveFromNode(ObjectData.NodeIndex[ObjectSlot], ObjectData.LocalIndex[ObjectSlot], false);
	SetObjectBounds(ObjectSlot, InNewBounds);
	InsertRecursive(0, ObjectSlot);
	return true;
}

void FFlatOctree::QueryFrustum(const FFrustum& InFrustum, TArray<void*>& OutResults) const
{
	if (NodePool.empty())
	{
		return;
	}

	const EFrustumAABBResult RootClassification = InFrustum.ClassifyAABB(NodePool[0].Bounds);
	if (RootClassification == EFrustumAABBResult::Outside)
	{
		return;
	}

	QueryFrustumRecursive(0, InFrustum, RootClassification, OutResults);
}

void FFlatOctree::QueryFrustumHandles(const FFrustum& InFrustum, TArray<FObjectHandle>& OutResults) const
{
	if (NodePool.empty())
	{
		return;
	}

	const EFrustumAABBResult RootClassification = InFrustum.ClassifyAABB(NodePool[0].Bounds);
	if (RootClassification == EFrustumAABBResult::Outside)
	{
		return;
	}

	QueryFrustumHandlesRecursive(0, InFrustum, RootClassification, OutResults);
}

void FFlatOctree::RaycastAll(const FRay& InRay, TArray<FObjectHandle>& OutHits) const
{
	if (NodePool.empty())
	{
		return;
	}

	const FRay NormalizedRay = InRay;
	//if (!NormalizedRay.HasValidDirection())
	//{
	//	return;
	//}

	TArray<FRayHit> Hits;
	Hits.reserve(HandleToSlot.size());

	const FNode& RootNode = NodePool[0];
	for (int32 ObjectSlot : RootNode.ObjectSlots)
	{
		float HitEntryDistance = 0.0f;
		float HitExitDistance = 0.0f;
		if (IntersectRayAABB(
			NormalizedRay,
			ObjectData.MinX[ObjectSlot], ObjectData.MinY[ObjectSlot], ObjectData.MinZ[ObjectSlot],
			ObjectData.MaxX[ObjectSlot], ObjectData.MaxY[ObjectSlot], ObjectData.MaxZ[ObjectSlot],
			HitEntryDistance,
			HitExitDistance))
		{
			Hits.push_back({ ObjectData.Handles[ObjectSlot], HitEntryDistance });
		}
	}

	float RootEntryDistance = 0.0f;
	float RootExitDistance = 0.0f;
	if (RootNode.HasChildren() && IntersectRayAABB(NormalizedRay, RootNode.Bounds, RootEntryDistance, RootExitDistance))
	{
		for (int32 ChildIndex : RootNode.Children)
		{
			if (ChildIndex != InvalidNodeIndex)
			{
				RaycastAllRecursive(ChildIndex, NormalizedRay, Hits);
			}
		}
	}

	std::sort(Hits.begin(), Hits.end(), [](const FRayHit& A, const FRayHit& B)
	{
		if (A.Distance == B.Distance)
		{
			return A.Handle < B.Handle;
		}

		return A.Distance < B.Distance;
	});

	for (const FRayHit& Hit : Hits)
	{
		OutHits.push_back(Hit.Handle);
	}
}

bool FFlatOctree::RaycastNearest(const FRay& InRay, FObjectHandle& OutHitHandle, float& OutHitDistance) const
{
	OutHitHandle = InvalidHandle;
	OutHitDistance = (std::numeric_limits<float>::max)();

	if (NodePool.empty())
	{
		return false;
	}

	const FRay NormalizedRay = InRay;
	//if (!NormalizedRay.HasValidDirection())
	//{
	//	return false;
	//}

	const FNode& RootNode = NodePool[0];
	for (int32 ObjectSlot : RootNode.ObjectSlots)
	{
		float HitEntryDistance = 0.0f;
		float HitExitDistance = 0.0f;
		if (IntersectRayAABB(
			NormalizedRay,
			ObjectData.MinX[ObjectSlot], ObjectData.MinY[ObjectSlot], ObjectData.MinZ[ObjectSlot],
			ObjectData.MaxX[ObjectSlot], ObjectData.MaxY[ObjectSlot], ObjectData.MaxZ[ObjectSlot],
			HitEntryDistance,
			HitExitDistance) && HitEntryDistance < OutHitDistance)
		{
			OutHitHandle = ObjectData.Handles[ObjectSlot];
			OutHitDistance = HitEntryDistance;
		}
	}

	float RootEntryDistance = 0.0f;
	float RootExitDistance = 0.0f;
	if (!RootNode.HasChildren() || !IntersectRayAABB(NormalizedRay, RootNode.Bounds, RootEntryDistance, RootExitDistance) || RootEntryDistance > OutHitDistance)
	{
		return OutHitHandle != InvalidHandle;
	}

	std::array<std::pair<int32, float>, ChildCount> ChildTraversal;
	int32 ChildTraversalCount = 0;

	for (int32 ChildIndex : RootNode.Children)
	{
		if (ChildIndex == InvalidNodeIndex)
		{
			continue;
		}

		float ChildEntryDistance = 0.0f;
		float ChildExitDistance = 0.0f;
		if (IntersectRayAABB(NormalizedRay, NodePool[ChildIndex].Bounds, ChildEntryDistance, ChildExitDistance) && ChildEntryDistance <= OutHitDistance)
		{
			ChildTraversal[ChildTraversalCount++] = { ChildIndex, ChildEntryDistance };
		}
	}

	std::sort(ChildTraversal.begin(), ChildTraversal.begin() + ChildTraversalCount, [](const std::pair<int32, float>& A, const std::pair<int32, float>& B)
	{
		return A.second < B.second;
	});

	for (int32 Index = 0; Index < ChildTraversalCount; ++Index)
	{
		if (ChildTraversal[Index].second > OutHitDistance)
		{
			break;
		}

		RaycastNearestRecursive(ChildTraversal[Index].first, NormalizedRay, OutHitHandle, OutHitDistance);
	}

	return OutHitHandle != InvalidHandle;
}

int32 FFlatOctree::GetObjectCount() const
{
	return static_cast<int32>(HandleToSlot.size());
}

int32 FFlatOctree::GetNodeCount() const
{
	return static_cast<int32>(NodePool.size());
}

int32 FFlatOctree::GetMaxDepth() const
{
	return MaxDepth;
}

int32 FFlatOctree::GetMaxObjectsPerNode() const
{
	return MaxObjectsPerNode;
}

int32 FFlatOctree::GetSplitCount() const
{
	return SplitCount;
}

int32 FFlatOctree::GetDeepestNodeDepth() const
{
	return DeepestNodeDepth;
}

const FAABB& FFlatOctree::GetWorldBounds() const
{
	return WorldBounds;
}

bool FFlatOctree::GetObjectNodeDepth(FObjectHandle InHandle, int32& OutDepth) const
{
	const auto SlotIt = HandleToSlot.find(InHandle);
	if (SlotIt == HandleToSlot.end())
	{
		return false;
	}

	const int32 NodeIndex = ObjectData.NodeIndex[SlotIt->second];
	if (NodeIndex == InvalidNodeIndex)
	{
		return false;
	}

	OutDepth = NodePool[NodeIndex].Depth;
	return true;
}

bool FFlatOctree::GetObject(FObjectHandle InHandle, FFlatOctreeObject& OutObject) const
{
	const auto SlotIt = HandleToSlot.find(InHandle);
	if (SlotIt == HandleToSlot.end())
	{
		return false;
	}

	const int32 ObjectSlot = SlotIt->second;
	OutObject.Handle = ObjectData.Handles[ObjectSlot];
	OutObject.Bounds = GetObjectBounds(ObjectSlot);
	OutObject.UserData = ObjectData.UserData[ObjectSlot];
	return true;
}

FFlatOctreeStats FFlatOctree::GetStats() const
{
	return { GetNodeCount(), GetObjectCount(), MaxDepth, DeepestNodeDepth, SplitCount };
}

void FFlatOctree::CollectNodeDebugInfo(TArray<FFlatOctreeNodeDebugInfo>& OutNodes) const
{
	if (!NodePool.empty())
	{
		CollectNodeDebugInfoRecursive(0, OutNodes);
	}
}

int32 FFlatOctree::AllocateNode(const FAABB& InBounds, int32 InDepth)
{
	const int32 NodeIndex = static_cast<int32>(NodePool.size());
	NodePool.emplace_back(InBounds, InDepth);
	DeepestNodeDepth = (std::max)(DeepestNodeDepth, InDepth);
	return NodeIndex;
}

int32 FFlatOctree::AllocateObjectSlot(FObjectHandle InHandle, const FAABB& InBounds, void* InUserData)
{
	int32 ObjectSlot = InvalidObjectSlot;
	if (!ObjectData.FreeSlots.empty())
	{
		ObjectSlot = ObjectData.FreeSlots.back();
		ObjectData.FreeSlots.pop_back();
	}
	else
	{
		ObjectSlot = static_cast<int32>(ObjectData.Handles.size());
		ObjectData.Handles.push_back(InvalidHandle);
		ObjectData.UserData.push_back(nullptr);
		ObjectData.MinX.push_back(0.0f);
		ObjectData.MinY.push_back(0.0f);
		ObjectData.MinZ.push_back(0.0f);
		ObjectData.MaxX.push_back(0.0f);
		ObjectData.MaxY.push_back(0.0f);
		ObjectData.MaxZ.push_back(0.0f);
		ObjectData.NodeIndex.push_back(InvalidNodeIndex);
		ObjectData.LocalIndex.push_back(InvalidObjectSlot);
	}

	ObjectData.Handles[ObjectSlot] = InHandle;
	ObjectData.UserData[ObjectSlot] = InUserData;
	SetObjectBounds(ObjectSlot, InBounds);
	ObjectData.NodeIndex[ObjectSlot] = InvalidNodeIndex;
	ObjectData.LocalIndex[ObjectSlot] = InvalidObjectSlot;
	return ObjectSlot;
}

void FFlatOctree::ReleaseObjectSlot(int32 InObjectSlot)
{
	assert(IsValidObjectSlot(InObjectSlot));

	HandleToSlot.erase(ObjectData.Handles[InObjectSlot]);
	ObjectData.Handles[InObjectSlot] = InvalidHandle;
	ObjectData.UserData[InObjectSlot] = nullptr;
	ObjectData.NodeIndex[InObjectSlot] = InvalidNodeIndex;
	ObjectData.LocalIndex[InObjectSlot] = InvalidObjectSlot;
	ObjectData.FreeSlots.push_back(InObjectSlot);
}

FFlatOctree::FObjectHandle FFlatOctree::InsertInternal(const FAABB& InBounds, void* InUserData, FObjectHandle InHandle)
{
	if (!InBounds.IsValid() || !WorldBounds.Intersects(InBounds))
	{
		return InvalidHandle;
	}

	const int32 ObjectSlot = AllocateObjectSlot(InHandle, InBounds, InUserData);
	HandleToSlot[InHandle] = ObjectSlot;
	return InsertRecursive(0, ObjectSlot);
}

FFlatOctree::FObjectHandle FFlatOctree::InsertRecursive(int32 InNodeIndex, int32 InObjectSlot)
{
	assert(InNodeIndex != InvalidNodeIndex);
	assert(IsValidObjectSlot(InObjectSlot));

	const FAABB Bounds = GetObjectBounds(InObjectSlot);
	const FNode& Node = NodePool[InNodeIndex];
	if (Node.HasChildren())
	{
		const int32 ChildLocalIndex = GetContainingChildIndex(InNodeIndex, Bounds);
		if (ChildLocalIndex != InvalidNodeIndex)
		{
			return InsertRecursive(Node.Children[ChildLocalIndex], InObjectSlot);
		}
	}

	FNode& MutableNode = NodePool[InNodeIndex];
	const int32 LocalIndex = static_cast<int32>(MutableNode.ObjectSlots.size());
	MutableNode.ObjectSlots.push_back(InObjectSlot);
	ObjectData.NodeIndex[InObjectSlot] = InNodeIndex;
	ObjectData.LocalIndex[InObjectSlot] = LocalIndex;

	TrySplit(InNodeIndex);
	return ObjectData.Handles[InObjectSlot];
}

void FFlatOctree::TrySplit(int32 InNodeIndex)
{
	if (!CanSplit(InNodeIndex))
	{
		return;
	}

	CreateChildren(InNodeIndex);
	RedistributeObjects(InNodeIndex);
}

void FFlatOctree::CreateChildren(int32 InNodeIndex)
{
	assert(InNodeIndex != InvalidNodeIndex);
	assert(!NodePool[InNodeIndex].HasChildren());

	const FAABB ParentBounds = NodePool[InNodeIndex].Bounds;
	const int32 ChildDepth = NodePool[InNodeIndex].Depth + 1;

	for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		const int32 NewNodeIndex = AllocateNode(BuildChildBounds(ParentBounds, ChildIndex), ChildDepth);
		NodePool[InNodeIndex].Children[ChildIndex] = NewNodeIndex;
	}

	++SplitCount;
}

void FFlatOctree::RedistributeObjects(int32 InNodeIndex)
{
	assert(InNodeIndex != InvalidNodeIndex);
	assert(NodePool[InNodeIndex].HasChildren());

	int32 LocalIndex = 0;
	while (LocalIndex < static_cast<int32>(NodePool[InNodeIndex].ObjectSlots.size()))
	{
		const int32 ObjectSlot = NodePool[InNodeIndex].ObjectSlots[LocalIndex];
		const int32 ChildLocalIndex = GetContainingChildIndex(InNodeIndex, GetObjectBounds(ObjectSlot));
		if (ChildLocalIndex == InvalidNodeIndex)
		{
			++LocalIndex;
			continue;
		}

		const int32 ChildNodeIndex = NodePool[InNodeIndex].Children[ChildLocalIndex];
		RemoveFromNode(InNodeIndex, LocalIndex, false);
		InsertRecursive(ChildNodeIndex, ObjectSlot);
	}
}

void FFlatOctree::RemoveFromNode(int32 InNodeIndex, int32 InLocalIndex, bool bReleaseObjectSlot)
{
	assert(InNodeIndex != InvalidNodeIndex);
	FNode& Node = NodePool[InNodeIndex];
	assert(InLocalIndex >= 0 && InLocalIndex < static_cast<int32>(Node.ObjectSlots.size()));

	const int32 RemovedObjectSlot = Node.ObjectSlots[InLocalIndex];
	const int32 LastIndex = static_cast<int32>(Node.ObjectSlots.size()) - 1;

	if (InLocalIndex != LastIndex)
	{
		const int32 MovedObjectSlot = Node.ObjectSlots[LastIndex];
		Node.ObjectSlots[InLocalIndex] = MovedObjectSlot;
		ObjectData.NodeIndex[MovedObjectSlot] = InNodeIndex;
		ObjectData.LocalIndex[MovedObjectSlot] = InLocalIndex;
	}

	Node.ObjectSlots.pop_back();
	ObjectData.NodeIndex[RemovedObjectSlot] = InvalidNodeIndex;
	ObjectData.LocalIndex[RemovedObjectSlot] = InvalidObjectSlot;

	if (bReleaseObjectSlot)
	{
		ReleaseObjectSlot(RemovedObjectSlot);
	}
}

void FFlatOctree::AppendNodeObjectsUnchecked(int32 InNodeIndex, TArray<void*>& OutResults) const
{
	const FNode& Node = NodePool[InNodeIndex];
	for (int32 ObjectSlot : Node.ObjectSlots)
	{
		OutResults.push_back(ObjectData.UserData[ObjectSlot]);
	}
}

void FFlatOctree::AppendNodeHandlesUnchecked(int32 InNodeIndex, TArray<FObjectHandle>& OutResults) const
{
	const FNode& Node = NodePool[InNodeIndex];
	for (int32 ObjectSlot : Node.ObjectSlots)
	{
		OutResults.push_back(ObjectData.Handles[ObjectSlot]);
	}
}

void FFlatOctree::QueryFrustumRecursive(int32 InNodeIndex, const FFrustum& InFrustum, EFrustumAABBResult InNodeResult, TArray<void*>& OutResults) const
{
	if (InNodeResult == EFrustumAABBResult::Outside)
	{
		return;
	}

	const FNode& Node = NodePool[InNodeIndex];
	if (InNodeResult == EFrustumAABBResult::Inside)
	{
		// Child bounds are subsets of the current node bounds, so a fully-contained
		// node can append its entire subtree without further plane tests.
		AppendNodeObjectsUnchecked(InNodeIndex, OutResults);

		if (!Node.HasChildren())
		{
			return;
		}

		for (int32 ChildIndex : Node.Children)
		{
			if (ChildIndex != InvalidNodeIndex)
			{
				QueryFrustumRecursive(ChildIndex, InFrustum, EFrustumAABBResult::Inside, OutResults);
			}
		}

		return;
	}

	for (int32 ObjectSlot : Node.ObjectSlots)
	{
		if (InFrustum.ClassifyAABB(GetObjectBounds(ObjectSlot)) != EFrustumAABBResult::Outside)
		{
			OutResults.push_back(ObjectData.UserData[ObjectSlot]);
		}
	}

	if (!Node.HasChildren())
	{
		return;
	}

	for (int32 ChildIndex : Node.Children)
	{
		if (ChildIndex != InvalidNodeIndex)
		{
			const EFrustumAABBResult ChildClassification = InFrustum.ClassifyAABB(NodePool[ChildIndex].Bounds);
			if (ChildClassification != EFrustumAABBResult::Outside)
			{
				QueryFrustumRecursive(ChildIndex, InFrustum, ChildClassification, OutResults);
			}
		}
	}
}

void FFlatOctree::QueryFrustumHandlesRecursive(int32 InNodeIndex, const FFrustum& InFrustum, EFrustumAABBResult InNodeResult, TArray<FObjectHandle>& OutResults) const
{
	if (InNodeResult == EFrustumAABBResult::Outside)
	{
		return;
	}

	const FNode& Node = NodePool[InNodeIndex];
	if (InNodeResult == EFrustumAABBResult::Inside)
	{
		// Child bounds are subsets of the current node bounds, so a fully-contained
		// node can append its entire subtree without further plane tests.
		AppendNodeHandlesUnchecked(InNodeIndex, OutResults);

		if (!Node.HasChildren())
		{
			return;
		}

		for (int32 ChildIndex : Node.Children)
		{
			if (ChildIndex != InvalidNodeIndex)
			{
				QueryFrustumHandlesRecursive(ChildIndex, InFrustum, EFrustumAABBResult::Inside, OutResults);
			}
		}

		return;
	}

	for (int32 ObjectSlot : Node.ObjectSlots)
	{
		if (InFrustum.ClassifyAABB(GetObjectBounds(ObjectSlot)) != EFrustumAABBResult::Outside)
		{
			OutResults.push_back(ObjectData.Handles[ObjectSlot]);
		}
	}

	if (!Node.HasChildren())
	{
		return;
	}

	for (int32 ChildIndex : Node.Children)
	{
		if (ChildIndex != InvalidNodeIndex)
		{
			const EFrustumAABBResult ChildClassification = InFrustum.ClassifyAABB(NodePool[ChildIndex].Bounds);
			if (ChildClassification != EFrustumAABBResult::Outside)
			{
				QueryFrustumHandlesRecursive(ChildIndex, InFrustum, ChildClassification, OutResults);
			}
		}
	}
}

void FFlatOctree::RaycastAllRecursive(int32 InNodeIndex, const FRay& InNormalizedRay, TArray<FRayHit>& OutHits) const
{
	const FNode& Node = NodePool[InNodeIndex];

	float NodeEntryDistance = 0.0f;
	float NodeExitDistance = 0.0f;
	if (!IntersectRayAABB(InNormalizedRay, Node.Bounds, NodeEntryDistance, NodeExitDistance))
	{
		return;
	}

	for (int32 ObjectSlot : Node.ObjectSlots)
	{
		float HitEntryDistance = 0.0f;
		float HitExitDistance = 0.0f;
		if (IntersectRayAABB(
			InNormalizedRay,
			ObjectData.MinX[ObjectSlot], ObjectData.MinY[ObjectSlot], ObjectData.MinZ[ObjectSlot],
			ObjectData.MaxX[ObjectSlot], ObjectData.MaxY[ObjectSlot], ObjectData.MaxZ[ObjectSlot],
			HitEntryDistance,
			HitExitDistance))
		{
			OutHits.push_back({ ObjectData.Handles[ObjectSlot], HitEntryDistance });
		}
	}

	if (!Node.HasChildren())
	{
		return;
	}

	for (int32 ChildIndex : Node.Children)
	{
		if (ChildIndex != InvalidNodeIndex)
		{
			RaycastAllRecursive(ChildIndex, InNormalizedRay, OutHits);
		}
	}
}

void FFlatOctree::RaycastNearestRecursive(int32 InNodeIndex, const FRay& InNormalizedRay, FObjectHandle& InOutHitHandle, float& InOutHitDistance) const
{
	const FNode& Node = NodePool[InNodeIndex];

	float NodeEntryDistance = 0.0f;
	float NodeExitDistance = 0.0f;
	if (!IntersectRayAABB(InNormalizedRay, Node.Bounds, NodeEntryDistance, NodeExitDistance) || NodeEntryDistance > InOutHitDistance)
	{
		return;
	}

	for (int32 ObjectSlot : Node.ObjectSlots)
	{
		float HitEntryDistance = 0.0f;
		float HitExitDistance = 0.0f;
		if (IntersectRayAABB(
			InNormalizedRay,
			ObjectData.MinX[ObjectSlot], ObjectData.MinY[ObjectSlot], ObjectData.MinZ[ObjectSlot],
			ObjectData.MaxX[ObjectSlot], ObjectData.MaxY[ObjectSlot], ObjectData.MaxZ[ObjectSlot],
			HitEntryDistance,
			HitExitDistance) && HitEntryDistance < InOutHitDistance)
		{
			InOutHitHandle = ObjectData.Handles[ObjectSlot];
			InOutHitDistance = HitEntryDistance;
		}
	}

	if (!Node.HasChildren())
	{
		return;
	}

	std::array<std::pair<int32, float>, ChildCount> ChildTraversal;
	int32 ChildTraversalCount = 0;

	for (int32 ChildIndex : Node.Children)
	{
		if (ChildIndex == InvalidNodeIndex)
		{
			continue;
		}

		float ChildEntryDistance = 0.0f;
		float ChildExitDistance = 0.0f;
		if (IntersectRayAABB(InNormalizedRay, NodePool[ChildIndex].Bounds, ChildEntryDistance, ChildExitDistance) && ChildEntryDistance <= InOutHitDistance)
		{
			ChildTraversal[ChildTraversalCount++] = { ChildIndex, ChildEntryDistance };
		}
	}

	std::sort(ChildTraversal.begin(), ChildTraversal.begin() + ChildTraversalCount, [](const std::pair<int32, float>& A, const std::pair<int32, float>& B)
	{
		return A.second < B.second;
	});

	for (int32 Index = 0; Index < ChildTraversalCount; ++Index)
	{
		if (ChildTraversal[Index].second > InOutHitDistance)
		{
			break;
		}

		RaycastNearestRecursive(ChildTraversal[Index].first, InNormalizedRay, InOutHitHandle, InOutHitDistance);
	}
}

void FFlatOctree::CollectNodeDebugInfoRecursive(int32 InNodeIndex, TArray<FFlatOctreeNodeDebugInfo>& OutNodes) const
{
	const FNode& Node = NodePool[InNodeIndex];
	OutNodes.push_back({ Node.Bounds, Node.Depth, static_cast<int32>(Node.ObjectSlots.size()), Node.HasChildren() });

	if (!Node.HasChildren())
	{
		return;
	}

	for (int32 ChildIndex : Node.Children)
	{
		if (ChildIndex != InvalidNodeIndex)
		{
			CollectNodeDebugInfoRecursive(ChildIndex, OutNodes);
		}
	}
}

bool FFlatOctree::IsValidObjectSlot(int32 InObjectSlot) const
{
	return InObjectSlot >= 0
		&& InObjectSlot < static_cast<int32>(ObjectData.Handles.size())
		&& ObjectData.Handles[InObjectSlot] != InvalidHandle;
}

FAABB FFlatOctree::GetObjectBounds(int32 InObjectSlot) const
{
	assert(IsValidObjectSlot(InObjectSlot));
	return FAABB(
		FVector(ObjectData.MinX[InObjectSlot], ObjectData.MinY[InObjectSlot], ObjectData.MinZ[InObjectSlot]),
		FVector(ObjectData.MaxX[InObjectSlot], ObjectData.MaxY[InObjectSlot], ObjectData.MaxZ[InObjectSlot]));
}

void FFlatOctree::SetObjectBounds(int32 InObjectSlot, const FAABB& InBounds)
{
	assert(InObjectSlot >= 0 && InObjectSlot < static_cast<int32>(ObjectData.Handles.size()));
	ObjectData.MinX[InObjectSlot] = InBounds.Min.X;
	ObjectData.MinY[InObjectSlot] = InBounds.Min.Y;
	ObjectData.MinZ[InObjectSlot] = InBounds.Min.Z;
	ObjectData.MaxX[InObjectSlot] = InBounds.Max.X;
	ObjectData.MaxY[InObjectSlot] = InBounds.Max.Y;
	ObjectData.MaxZ[InObjectSlot] = InBounds.Max.Z;
}

int32 FFlatOctree::GetContainingChildIndex(int32 InNodeIndex, const FAABB& InBounds) const
{
	const FAABB& NodeBounds = NodePool[InNodeIndex].Bounds;
	if (!NodeBounds.Contains(InBounds))
	{
		return InvalidNodeIndex;
	}

	const FVector Center = NodeBounds.GetCenter();
	int32 ChildIndex = 0;

	// Strict comparisons intentionally keep split-plane boundary objects in the parent.
	if (InBounds.Max.X < Center.X)
	{
	}
	else if (InBounds.Min.X > Center.X)
	{
		ChildIndex |= 1;
	}
	else
	{
		return InvalidNodeIndex;
	}

	if (InBounds.Max.Y < Center.Y)
	{
	}
	else if (InBounds.Min.Y > Center.Y)
	{
		ChildIndex |= 2;
	}
	else
	{
		return InvalidNodeIndex;
	}

	if (InBounds.Max.Z < Center.Z)
	{
	}
	else if (InBounds.Min.Z > Center.Z)
	{
		ChildIndex |= 4;
	}
	else
	{
		return InvalidNodeIndex;
	}

	return ChildIndex;
}

bool FFlatOctree::CanSplit(int32 InNodeIndex) const
{
	const FNode& Node = NodePool[InNodeIndex];
	if (Node.HasChildren()
		|| Node.Depth >= MaxDepth
		|| static_cast<int32>(Node.ObjectSlots.size()) <= MaxObjectsPerNode
		|| !CanCreateChildren(Node.Bounds))
	{
		return false;
	}

	for (int32 ObjectSlot : Node.ObjectSlots)
	{
		if (GetContainingChildIndex(InNodeIndex, GetObjectBounds(ObjectSlot)) != InvalidNodeIndex)
		{
			return true;
		}
	}

	return false;
}

bool FFlatOctree::CanCreateChildren(const FAABB& InBounds) const
{
	const FVector Extent = InBounds.GetExtent();
	return Extent.X > FMath::KindaSmallNumber
		&& Extent.Y > FMath::KindaSmallNumber
		&& Extent.Z > FMath::KindaSmallNumber;
}

FAABB FFlatOctree::BuildChildBounds(const FAABB& InParentBounds, int32 InChildIndex) const
{
	const FVector Center = InParentBounds.GetCenter();

	const bool bUpperX = (InChildIndex & 1) != 0;
	const bool bUpperY = (InChildIndex & 2) != 0;
	const bool bUpperZ = (InChildIndex & 4) != 0;

	const FVector ChildMin(
		bUpperX ? Center.X : InParentBounds.Min.X,
		bUpperY ? Center.Y : InParentBounds.Min.Y,
		bUpperZ ? Center.Z : InParentBounds.Min.Z);

	const FVector ChildMax(
		bUpperX ? InParentBounds.Max.X : Center.X,
		bUpperY ? InParentBounds.Max.Y : Center.Y,
		bUpperZ ? InParentBounds.Max.Z : Center.Z);

	return FAABB(ChildMin, ChildMax);
}
