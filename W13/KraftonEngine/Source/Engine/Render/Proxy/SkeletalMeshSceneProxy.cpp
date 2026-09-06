#include "SkeletalMeshSceneProxy.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Render/Command/DrawCommand.h"
#include "Runtime/Engine.h"
#include "Profiling/Time/Timer.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>
#include <cstring>

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::SkeletalMesh;
}

FSkeletalMeshSceneProxy::~FSkeletalMeshSceneProxy()
{
	ReleaseSkinMatrixBuffer();
}   

USkeletalMeshComponent* FSkeletalMeshSceneProxy::GetSkeletalMeshComponent() const
{
	return static_cast<USkeletalMeshComponent*>(GetOwner());
}

void FSkeletalMeshSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
};

void FSkeletalMeshSceneProxy::UpdateMesh()
{
	MeshBuffer = GetOwner()->GetMeshBuffer();
	RebuildSectionDraws();

	CachedDynamicVertexCount = 0;
	UploadedSkinnedRevision = 0;
	UploadedSkinMatrixRevision = 0;
	bDynamicBufferNeedsCreate = true;
	ReleaseSkinMatrixBuffer();

	// 동적 스냅샷 무효화 — 다음 UpdateRenderSnapshot 에서 새 메시 기준으로 재계산.
	CachedSkinMatrices.clear();
	CachedSkinnedVertices.clear();
	RenderSnapshotRevision = 0;

	// 정적 렌더 버퍼(불변 GPU 리소스) 포인터 캐시 — 렌더 제출이 Asset 체인을 안 타게.
	CachedStaticVertexBuffer = nullptr;
	CachedStaticVertexStride = 0;
	CachedStaticIndexBuffer = nullptr;

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = SMC ? SMC->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (Asset)
	{
		CachedDynamicVertexCount = static_cast<uint32>(Asset->Vertices.size());
		if (Asset->RenderBuffer && Asset->RenderBuffer->IsValid())
		{
			CachedStaticVertexBuffer = Asset->RenderBuffer->GetVertexBuffer().GetBuffer();
			CachedStaticVertexStride = Asset->RenderBuffer->GetVertexBuffer().GetStride();
			CachedStaticIndexBuffer  = Asset->RenderBuffer->GetIndexBuffer().GetBuffer();
		}
	}
}

void FSkeletalMeshSceneProxy::UpdateRenderSnapshot()
{
	// 게임 스레드 — World::Tick 이후 본 포즈가 확정된 시점에 호출(FScene::UpdateDirtyProxies).
	// 컴포넌트의 스킨 행렬을 프록시 캐시로 복사해, 렌더 제출이 live 컴포넌트를 읽지 않게 한다.
	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	if (!SMC)
	{
		return;
	}

	// SkinnedRevision 이 바뀐 경우(또는 최초)만 재계산 — 정지 포즈는 매 프레임 재빌드 안 함.
	const uint64 Revision = SMC->GetSkinnedRevision();
	if (Revision == RenderSnapshotRevision && !CachedSkinMatrices.empty())
	{
		return;
	}

	// GPU 스키닝 경로(본 행렬)와 CPU 스키닝 경로(스킨된 버텍스)를 둘 다 스냅샷한다.
	// 활성 경로가 아닌 쪽은 비어있어(예: GPU 메시는 SkinnedVertices 가 비어있음) 복사 비용 0.
	SMC->BuildSkinMatrices(CachedSkinMatrices);
	CachedSkinnedVertices = SMC->GetSkinnedVertices();
	RenderSnapshotRevision = Revision;
}

bool FSkeletalMeshSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	// 렌더 제출 — 컴포넌트 미접근. 게임 스레드 스냅샷(CachedSkinnedVertices) + 캐시된 정적 IB 만 사용.
	if (!Device || !Context || !CachedStaticIndexBuffer) return false;

	const uint32 VertexCount = static_cast<uint32>(CachedSkinnedVertices.size());
	if (VertexCount == 0) return false;

	if (bDynamicBufferNeedsCreate || !DynamicVertexBuffer.GetBuffer())
	{
		DynamicVertexBuffer.Create(Device, CachedDynamicVertexCount ? CachedDynamicVertexCount : VertexCount, sizeof(FVertexPNCTT));
		bDynamicBufferNeedsCreate = false;
	}

	DynamicVertexBuffer.EnsureCapacity(Device, VertexCount);

	if (UploadedSkinnedRevision != RenderSnapshotRevision)
	{
		if (!DynamicVertexBuffer.Update(Context, CachedSkinnedVertices.data(), VertexCount))
		{
			return false;
		}
		UploadedSkinnedRevision = RenderSnapshotRevision;
	}

	OutBuffer = {};
	OutBuffer.VB = DynamicVertexBuffer.GetBuffer();
	OutBuffer.VBStride = DynamicVertexBuffer.GetStride();
	OutBuffer.IB = CachedStaticIndexBuffer;
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

bool FSkeletalMeshSceneProxy::PrepareGpuSkinningDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	// 렌더 제출 — 컴포넌트 미접근. 캐시된 정적 VB/IB + 스냅샷 기반 스킨 행렬 버퍼만 사용.
	if (!CachedStaticVertexBuffer || !CachedStaticIndexBuffer) return false;

	if (!UpdateSkinMatrixBuffer(Device, Context)) return false;

	OutBuffer = {};
	OutBuffer.VB = CachedStaticVertexBuffer;
	OutBuffer.VBStride = CachedStaticVertexStride;
	OutBuffer.IB = CachedStaticIndexBuffer;
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

ID3D11ShaderResourceView* FSkeletalMeshSceneProxy::GetSkinMatrixSRV(ID3D11Device* Device, ID3D11DeviceContext* Context) const
{
	UpdateSkinMatrixBuffer(Device, Context);
	return SkinMatrixSRV;
}

void FSkeletalMeshSceneProxy::ReleaseSkinMatrixBuffer() const
{
	if (SkinMatrixSRV)
	{
		SkinMatrixSRV->Release();
		SkinMatrixSRV = nullptr;
	}

	if (SkinMatrixBuffer)
	{
		SkinMatrixBuffer->Release();
		SkinMatrixBuffer = nullptr;
	}

	SkinMatrixCapacity = 0;
}

bool FSkeletalMeshSceneProxy::UpdateSkinMatrixBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context) const
{
	// 렌더 제출 단계 — 컴포넌트를 일절 읽지 않는다. 게임 스레드가 UpdateRenderSnapshot 에서 채운
	// CachedSkinMatrices 스냅샷만 GPU 로 업로드한다(렌더 스레드 분리 전제).
	if (!Device || !Context || CachedSkinMatrices.empty()) return false;

	const uint32 MatrixCount = static_cast<uint32>(CachedSkinMatrices.size());
	const uint64 CurrentRevision = RenderSnapshotRevision;

	if (!SkinMatrixBuffer || !SkinMatrixSRV || SkinMatrixCapacity < MatrixCount)
	{
		ReleaseSkinMatrixBuffer();

		D3D11_BUFFER_DESC BufferDesc = {};
		BufferDesc.ByteWidth = sizeof(FMatrix) * MatrixCount;
		BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		BufferDesc.StructureByteStride = sizeof(FMatrix);

		if (FAILED(Device->CreateBuffer(&BufferDesc, nullptr, &SkinMatrixBuffer)))
		{
			ReleaseSkinMatrixBuffer();
			return false;
		}

		SkinMatrixBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen("SkinMatrixBuffer")), "SkinMatrixBuffer");

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.FirstElement = 0;
		SRVDesc.Buffer.NumElements = MatrixCount;

		if (FAILED(Device->CreateShaderResourceView(SkinMatrixBuffer, &SRVDesc, &SkinMatrixSRV)))
		{
			ReleaseSkinMatrixBuffer();
			return false;
		}

		SkinMatrixSRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen("SkinMatrixSRV")), "SkinMatrixSRV");
		SkinMatrixCapacity = MatrixCount;
		UploadedSkinMatrixRevision = 0;
	}

	if (UploadedSkinMatrixRevision == CurrentRevision)
	{
		return true;
	}

	{
		SCOPE_STAT_CAT("GPUSkinning_MatrixUpload", "Skinning");

		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (FAILED(Context->Map(SkinMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		{
			return false;
		}

		std::memcpy(Mapped.pData, CachedSkinMatrices.data(), sizeof(FMatrix) * MatrixCount);
		Context->Unmap(SkinMatrixBuffer, 0);
	}

	UploadedSkinMatrixRevision = CurrentRevision;
	return true;
}

void FSkeletalMeshSceneProxy::RebuildSectionDraws()
{
	SectionDraws.clear();

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();

		return;
	}

	SectionDraws.clear();

	const auto& Slots = Mesh->GetSkeletalMaterials();
	const auto& Overrides = SMC->GetOverrideMaterials();

	for (const FSkeletalMeshSection& Section : Mesh->GetSkeletalMeshAsset()->Sections)
	{
		FMeshSectionDraw Draw;
		Draw.Material = nullptr;
		Draw.FirstIndex = Section.FirstIndex;
		Draw.IndexCount = Section.IndexCount;


		int32 i = Section.MaterialIndex;
		if (i >= 0 && i < static_cast<int32>(Slots.size()))
		{
			if (i < static_cast<int32>(Overrides.size()) && Overrides[i])
				Draw.Material = Overrides[i];
			else if (Slots[i].MaterialInterface)
				Draw.Material = Slots[i].MaterialInterface;
		}

		if (!Draw.Material)
		{
			Draw.Material = FMaterialManager::Get().GetOrCreateMaterial("None");
		}

		SectionDraws.push_back(Draw);
	}
}
