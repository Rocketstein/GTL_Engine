#include "Mesh/Importer/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Importer/Fbx/FbxSceneQuery.h"
#include "Mesh/Importer/Fbx/FbxTransformUtils.h"

namespace
{
	static FbxNode* FindLowestCommonAncestor(FbxNode* A, FbxNode* B)
	{
		if (!A || !B)
		{
			return nullptr;
		}

		TSet<FbxNode*> Ancestors;
		for (FbxNode* Node = A; Node && Node->GetParent(); Node = Node->GetParent())
		{
			Ancestors.insert(Node);
		}

		for (FbxNode* Node = B; Node && Node->GetParent(); Node = Node->GetParent())
		{
			if (Ancestors.find(Node) != Ancestors.end())
			{
				return Node;
			}
		}

		return nullptr;
	}

	static FbxNode* FindSkinClusterLowestCommonAncestor(const TArray<FbxNode*>& LinkNodes)
	{
		FbxNode* CommonAncestor = nullptr;
		for (FbxNode* LinkNode : LinkNodes)
		{
			if (!LinkNode)
			{
				continue;
			}

			CommonAncestor = CommonAncestor
				? FindLowestCommonAncestor(CommonAncestor, LinkNode)
				: LinkNode;

			if (!CommonAncestor)
			{
				break;
			}
		}

		return CommonAncestor;
	}

	static TArray<FbxNode*> CollectSkinClusterLinkNodes(FbxSkin* Skin)
	{
		TArray<FbxNode*> LinkNodes;
		if (!Skin)
		{
			return LinkNodes;
		}

		for (int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ++ClusterIndex)
		{
			FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
			FbxNode*    LinkNode = Cluster ? Cluster->GetLink() : nullptr;
			if (LinkNode)
			{
				LinkNodes.push_back(LinkNode);
			}
		}

		return LinkNodes;
	}

	static void MarkNodesFromLinksToCommonAncestor(
		const TArray<FbxNode*>& LinkNodes,
		FbxNode*                CommonAncestor,
		TSet<FbxNode*>&         OutRequiredNodes
		)
	{
		if (!CommonAncestor)
		{
			return;
		}

		for (FbxNode* LinkNode : LinkNodes)
		{
			for (FbxNode* BoneNode = LinkNode; BoneNode && BoneNode->GetParent(); BoneNode = BoneNode->GetParent())
			{
				OutRequiredNodes.insert(BoneNode);
				if (BoneNode == CommonAncestor)
				{
					break;
				}
			}
		}
	}

	static bool HasDirectMeshChild(FbxNode* Node)
	{
		if (!Node)
		{
			return false;
		}

		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			FbxNode* Child = Node->GetChild(ChildIndex);
			if (Child && Child->GetMesh())
			{
				return true;
			}
		}

		return false;
	}

	static bool HasSkeletonChild(FbxNode* Node)
	{
		if (!Node)
		{
			return false;
		}

		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			if (FFbxSceneQuery::IsSkeletonNode(Node->GetChild(ChildIndex)))
			{
				return true;
			}
		}

		return false;
	}

	static void MarkSyntheticRootParent(FbxNode* CommonAncestor, TSet<FbxNode*>& OutRequiredNodes)
	{
		FbxNode* Parent = CommonAncestor ? CommonAncestor->GetParent() : nullptr;
		if (!Parent || !Parent->GetParent())
		{
			return;
		}

		// Blender can export an explicit root bone as a non-skeleton transform node above Hips.
		// Keep that single node so Root->Hips survives, but do not pull mesh wrapper objects in.
		if (!FFbxSceneQuery::IsSkeletonNode(Parent) &&
			FFbxSceneQuery::IsSkeletonNode(CommonAncestor) &&
			!HasDirectMeshChild(Parent))
		{
			OutRequiredNodes.insert(Parent);
		}
	}

	static void MarkSkeletonLeafHierarchyNodes(FFbxImportContext& Context, TSet<FbxNode*>& OutRequiredNodes)
	{
		TArray<FbxNode*> LeafNodes;
		for (FbxNode* Node : Context.AllNodes)
		{
			if (FFbxSceneQuery::IsSkeletonNode(Node) && !HasSkeletonChild(Node))
			{
				LeafNodes.push_back(Node);
			}
		}

		if (LeafNodes.size() <= 1)
		{
			return;
		}

		FbxNode* CommonAncestor = FindSkinClusterLowestCommonAncestor(LeafNodes);
		MarkNodesFromLinksToCommonAncestor(LeafNodes, CommonAncestor, OutRequiredNodes);
		MarkSyntheticRootParent(CommonAncestor, OutRequiredNodes);
	}

	static void MarkSkinClusterHierarchyNodes(FFbxImportContext& Context, TSet<FbxNode*>& OutRequiredNodes)
	{
		for (FbxNode* Node : Context.AllNodes)
		{
			FbxMesh* Mesh = Node ? Node->GetMesh() : nullptr;
			if (!Mesh)
			{
				continue;
			}

			const int32 DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
			for (int32 DeformerIndex = 0; DeformerIndex < DeformerCount; ++DeformerIndex)
			{
				FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(DeformerIndex, FbxDeformer::eSkin));
				if (!Skin)
				{
					continue;
				}

				const TArray<FbxNode*> LinkNodes = CollectSkinClusterLinkNodes(Skin);
				FbxNode* CommonAncestor = FindSkinClusterLowestCommonAncestor(LinkNodes);
				MarkNodesFromLinksToCommonAncestor(LinkNodes, CommonAncestor, OutRequiredNodes);
				MarkSyntheticRootParent(CommonAncestor, OutRequiredNodes);
			}
		}
	}

	static bool ShouldImportAsBone(FbxNode* Node, const TSet<FbxNode*>& RequiredHierarchyNodes)
	{
		if (!Node || Node->GetMesh())
		{
			return false;
		}

		return FFbxSceneQuery::IsSkeletonNode(Node) || RequiredHierarchyNodes.find(Node) != RequiredHierarchyNodes.end();
	}

	static void BuildReferenceSkeleton(FFbxImportContext& Context)
	{
		Context.ReferenceSkeleton.Bones.clear();
		Context.ReferenceSkeleton.Bones.reserve(Context.Bones.size());

		for (const FBone& Bone : Context.Bones)
		{
			FReferenceBone RefBone;
			RefBone.Name = Bone.Name;
			RefBone.ParentIndex = Bone.ParentIndex;
			RefBone.LocalBindPose = Bone.GetReferenceLocalPose();
			RefBone.GlobalBindPose = Bone.GetReferenceGlobalPose();
			RefBone.InverseBindPose = Bone.GetInverseBindPose();
			Context.ReferenceSkeleton.Bones.push_back(RefBone);
		}
	}
}

bool FFbxSkeletonImporter::ImportSkeleton(FbxScene* Scene, FFbxImportContext& Context, FString* OutMessage)
{
	(void)Scene;
	Context.Bones.clear();
	Context.BoneNodeToIndex.clear();
	Context.ReferenceSkeleton.Bones.clear();

	TSet<FbxNode*> RequiredHierarchyNodes;
	MarkSkinClusterHierarchyNodes(Context, RequiredHierarchyNodes);
	if (RequiredHierarchyNodes.empty())
	{
		MarkSkeletonLeafHierarchyNodes(Context, RequiredHierarchyNodes);
	}

	for (FbxNode* Node : Context.AllNodes)
	{
		if (!ShouldImportAsBone(Node, RequiredHierarchyNodes))
		{
			continue;
		}

		FBone Bone;
		Bone.Name = Node->GetName();

		Bone.ParentIndex = FindNearestParentBoneIndex(Node, Context.BoneNodeToIndex);

		const FbxAMatrix GlobalFbxMatrix = Node->EvaluateGlobalTransform();
		const bool bAbsorbWrapperTransform = Bone.ParentIndex < 0 && FFbxSceneQuery::HasNonSkeletonWrapperParent(Node);
		const FbxAMatrix LocalFbxMatrix = bAbsorbWrapperTransform
			? GlobalFbxMatrix
			: Node->EvaluateLocalTransform();
		Bone.LocalMatrix = FFbxTransformUtils::ToEngineMatrix(LocalFbxMatrix);
		Bone.GlobalMatrix = FFbxTransformUtils::ToEngineMatrix(GlobalFbxMatrix);
		Bone.InverseBindPoseMatrix = FFbxTransformUtils::ToEngineInverseMatrix(GlobalFbxMatrix);
		Bone.SyncSeparatedPoseDataFromLegacy();

		const int32 NewBoneIndex = static_cast<int32>(Context.Bones.size());
		Context.Bones.push_back(Bone);
		Context.BoneNodeToIndex[Node] = NewBoneIndex;
	}

	if (Context.Bones.empty())
	{
		if (OutMessage) *OutMessage = "FBX skeletal import failed: no skeleton nodes found.";
		return false;
	}

	BuildReferenceSkeleton(Context);
	return true;
}

int32 FFbxSkeletonImporter::FindNearestParentBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& NodeToIndex)
{
	FbxNode* Parent = Node ? Node->GetParent() : nullptr;
	while (Parent)
	{
		auto It = NodeToIndex.find(Parent);
		if (It != NodeToIndex.end())
		{
			return It->second;
		}

		Parent = Parent->GetParent();
	}

	return -1;
}
