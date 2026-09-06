#include "SkeletalMesh.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Animation/Skeleton/Skeleton.h"
#include "Physics/Asset/PhysicsAsset.h"
#include "Physics/Asset/PhysicsAssetManager.h"

namespace
{
	constexpr uint32 SkeletalMeshPhysicsAssetPathVersion = 3;
}

void USkeletalMesh::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading() && !SkeletalMeshAsset)
	{
		SkeletalMeshAsset = new FSkeletalMesh();
	}

    if (Ar.IsSaving())
    {
        SyncSkeletonBindingToAsset();
    }

	Ar << SkeletalMeshAsset->PathFileName;
	Ar << SkeletalMeshAsset->SkeletonPath;
    Ar << SkeletalMeshAsset->SkeletonAssetGuid;
    Ar << SkeletalMeshAsset->SkeletonCompatibilitySignature;
	Ar << SkeletalMeshAsset->Vertices;
	Ar << SkeletalMeshAsset->Indices;
	Ar << SkeletalMeshAsset->Sections;
	Ar << SkeletalMeshAsset->MeshRanges;
	Ar << SkeletalMeshAsset->Bones;
	Ar << SkeletalMaterials;
	Ar << SkeletalMeshAsset->MorphTargets;

	if (Ar.IsSaving() || Ar.GetPackageVersion() >= SkeletalMeshPhysicsAssetPathVersion)
	{
		FString SerializedPhysicsAssetPath = Ar.IsSaving() ? PhysicsAssetPath.ToString() : FString();
		Ar << SerializedPhysicsAssetPath;

		if (Ar.IsLoading())
		{
			SetPhysicsAssetPath(SerializedPhysicsAssetPath);
		}
	}
	else if (Ar.IsLoading())
	{
		SetPhysicsAssetPath("None");
	}

	if (Ar.IsLoading())
	{
		SkeletalMeshAsset->NormalizeBonePoseData();
        SyncSkeletonBindingFromAsset();
		CacheSectionMaterialIndices();
		SkeletalMeshAsset->bBoundsValid = false;
	}
}

void USkeletalMesh::SetSkeletalMeshAsset(FSkeletalMesh* InMesh)
{
	SkeletalMeshAsset = InMesh;
	if (SkeletalMeshAsset)
	{
		SkeletalMeshAsset->NormalizeBonePoseData();
	}
    SyncSkeletonBindingFromAsset();
	CacheSectionMaterialIndices();
}

FSkeletalMesh* USkeletalMesh::GetSkeletalMeshAsset() const
{
	return SkeletalMeshAsset;
}

void USkeletalMesh::SetSkeletalMaterials(TArray<FSkeletalMaterial>&& InMaterials)
{
	SkeletalMaterials = InMaterials;
	CacheSectionMaterialIndices();
}

const TArray<FSkeletalMaterial>& USkeletalMesh::GetSkeletalMaterials() const
{
	return SkeletalMaterials;
}

void USkeletalMesh::InitResources(ID3D11Device* InDevice)
{
	if (!InDevice || !SkeletalMeshAsset) return;

	const uint32 CPUSize =
		static_cast<uint32>(SkeletalMeshAsset->Vertices.size() * sizeof(FVertexPNCTBW)) +
		static_cast<uint32>(SkeletalMeshAsset->Indices.size() * sizeof(uint32));
	MemoryStats::AddSkeletalMeshCPUMemory(CPUSize);

	TMeshData<FVertexPNCTBW> RenderMeshData;
	RenderMeshData.Vertices.reserve(SkeletalMeshAsset->Vertices.size());

	for (const FVertexPNCTBW& RawVert : SkeletalMeshAsset->Vertices)
	{
		FVertexPNCTBW RenderVert;
		RenderVert.Position = RawVert.Position;
		RenderVert.Normal = RawVert.Normal;
		RenderVert.Color = RawVert.Color;
		RenderVert.UV = RawVert.UV;
		RenderVert.Tangent = RawVert.Tangent;
		std::copy(std::begin(RawVert.BoneIndices), std::end(RawVert.BoneIndices), std::begin(RenderVert.BoneIndices));
		std::copy(std::begin(RawVert.BoneWeights), std::end(RawVert.BoneWeights), std::begin(RenderVert.BoneWeights));
		RenderMeshData.Vertices.push_back(RenderVert);
	}
	RenderMeshData.Indices = SkeletalMeshAsset->Indices;

	SkeletalMeshAsset->RenderBuffer = std::make_unique<FMeshBuffer>();
	SkeletalMeshAsset->RenderBuffer->Create(InDevice, RenderMeshData);
}

void USkeletalMesh::SetSkeleton(USkeleton* InSkeleton)
{
	Skeleton = InSkeleton;

	if (Skeleton)
	{
        SetSkeletonBinding(Skeleton->GetSkeletonBinding());
	}
	else
	{
        FSkeletonBinding EmptyBinding;
        SetSkeletonBinding(EmptyBinding);
	}
}

USkeleton* USkeletalMesh::GetSkeleton() const
{
	return Skeleton;
}

void USkeletalMesh::SetSkeletonBinding(const FSkeletonBinding& InBinding)
{
    SkeletonBinding = InBinding;
    if (SkeletonBinding.SkeletonPath.empty())
    {
        SkeletonBinding.SkeletonPath = "None";
    }
    SyncSkeletonBindingToAsset();
}

void USkeletalMesh::SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset)
{
    PhysicsAsset = InPhysicsAsset;
    PhysicsAssetPath = InPhysicsAsset ? InPhysicsAsset->GetSourcePath() : FString("None");
}

void USkeletalMesh::SetPhysicsAssetPath(const FString& InPath)
{
    PhysicsAssetPath = InPath.empty() ? FString("None") : InPath;
    PhysicsAsset = nullptr;
}

UPhysicsAsset* USkeletalMesh::GetPhysicsAsset() const
{
    // Lazy 해석 폴백: 포인터가 비어 있고 경로가 유효하면 그 자리에서 로드해 채운다.
    // (FMeshManager::LoadSkeletalMesh 의 해석 경로를 타지 않은 메시도 null 이 안 되도록 보장.)
    if (!PhysicsAsset)
    {
        const FString Path = PhysicsAssetPath.ToString();
        if (!Path.empty() && Path != "None")
        {
            PhysicsAsset = FPhysicsAssetManager::Get().Load(Path);
        }
    }
    return PhysicsAsset;
}

void USkeletalMesh::SyncSkeletonBindingToAsset()
{
    if (!SkeletalMeshAsset)
    {
        return;
    }

    SkeletalMeshAsset->SkeletonPath = SkeletonBinding.SkeletonPath.empty() ? FString("None") : SkeletonBinding.SkeletonPath;
    SkeletalMeshAsset->SkeletonAssetGuid = SkeletonBinding.SkeletonAssetGuid;
    SkeletalMeshAsset->SkeletonCompatibilitySignature = SkeletonBinding.CompatibilitySignature;
}

void USkeletalMesh::SyncSkeletonBindingFromAsset()
{
    if (!SkeletalMeshAsset)
    {
        return;
    }

    SkeletonBinding.SkeletonPath = SkeletalMeshAsset->SkeletonPath.empty() ? FString("None") : SkeletalMeshAsset->SkeletonPath;
    SkeletonBinding.SkeletonAssetGuid = SkeletalMeshAsset->SkeletonAssetGuid;
    SkeletonBinding.CompatibilitySignature = SkeletalMeshAsset->SkeletonCompatibilitySignature;
}

void USkeletalMesh::CacheSectionMaterialIndices()
{
	if (!SkeletalMeshAsset)
	{
		return;
	}

	for (FSkeletalMeshSection& Section : SkeletalMeshAsset->Sections)
	{
		Section.MaterialIndex = -1;
		for (int32 i = 0; i < static_cast<int32>(SkeletalMaterials.size()); ++i)
		{
			if (SkeletalMaterials[i].MaterialSlotName == Section.MaterialSlotName)
			{
				Section.MaterialIndex = i;
				break;
			}
		}
	}
}
