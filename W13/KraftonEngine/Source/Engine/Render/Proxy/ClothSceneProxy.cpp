#include "Render/Proxy/ClothSceneProxy.h"

#include "Component/Primitive/ClothComponent.h"
#include "Materials/MaterialManager.h"
#include "Render/Command/DrawCommand.h"

FClothSceneProxy::FClothSceneProxy(UClothComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::Cloth;
}

UClothComponent* FClothSceneProxy::GetClothComponent() const
{
	return static_cast<UClothComponent*>(GetOwner());
}

void FClothSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

void FClothSceneProxy::UpdateMesh()
{
	MeshBuffer = nullptr;
	RebuildSectionDraws();

	UClothComponent* Component = GetClothComponent();
	CachedVertexCount = Component ? static_cast<uint32>(Component->GetRenderVertices().size()) : 0;
	CachedIndexCount = Component ? static_cast<uint32>(Component->GetRenderIndices().size()) : 0;
	bDynamicBuffersNeedCreate = true;
	UploadedRevision = 0;
}

bool FClothSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	UClothComponent* Component = GetClothComponent();
	if (!Device || !Context || !Component)
	{
		return false;
	}

	const TArray<FVertexPNCTT>& Vertices = Component->GetRenderVertices();
	const TArray<uint32>& Indices = Component->GetRenderIndices();
	const uint32 VertexCount = static_cast<uint32>(Vertices.size());
	const uint32 IndexCount = static_cast<uint32>(Indices.size());
	if (VertexCount == 0 || IndexCount == 0)
	{
		return false;
	}

	if (bDynamicBuffersNeedCreate || !DynamicVertexBuffer.GetBuffer() || !DynamicIndexBuffer.GetBuffer())
	{
		DynamicVertexBuffer.Create(Device, CachedVertexCount ? CachedVertexCount : VertexCount, sizeof(FVertexPNCTT));
		DynamicIndexBuffer.Create(Device, CachedIndexCount ? CachedIndexCount : IndexCount);
		bDynamicBuffersNeedCreate = false;
	}

	DynamicVertexBuffer.EnsureCapacity(Device, VertexCount);
	DynamicIndexBuffer.EnsureCapacity(Device, IndexCount);

	const uint64 CurrentRevision = Component->GetRenderRevision();
	if (UploadedRevision != CurrentRevision)
	{
		if (!DynamicVertexBuffer.Update(Context, Vertices.data(), VertexCount))
		{
			return false;
		}
		if (!DynamicIndexBuffer.Update(Context, Indices.data(), IndexCount))
		{
			return false;
		}
		UploadedRevision = CurrentRevision;
	}

	OutBuffer = {};
	OutBuffer.VB = DynamicVertexBuffer.GetBuffer();
	OutBuffer.VBStride = DynamicVertexBuffer.GetStride();
	OutBuffer.IB = DynamicIndexBuffer.GetBuffer();
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

void FClothSceneProxy::RebuildSectionDraws()
{
	SectionDraws.clear();

	UClothComponent* Component = GetClothComponent();
	if (!Component || Component->GetRenderIndices().empty())
	{
		return;
	}

	FMeshSectionDraw Draw;
	Draw.Material = Component->GetResolvedMaterial();
	if (!Draw.Material)
	{
		Draw.Material = FMaterialManager::Get().GetOrCreateMaterial("None");
	}
	Draw.FirstIndex = 0;
	Draw.IndexCount = static_cast<uint32>(Component->GetRenderIndices().size());
	SectionDraws.push_back(Draw);
}
