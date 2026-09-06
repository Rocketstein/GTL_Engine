#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Math/Matrix.h"
#include "Render/Types/VertexTypes.h"

class USkeletalMeshComponent;
struct FDrawCommandBuffer;

class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent);
	~FSkeletalMeshSceneProxy() override;

	void UpdateMaterial() override;
	void UpdateMesh() override;
	// 게임 스레드: 컴포넌트에서 스킨 행렬을 스냅샷해 CachedSkinMatrices 에 저장(SkinnedRevision 변경 시만).
	void UpdateRenderSnapshot() override;

	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const override;
	bool PrepareGpuSkinningDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const;
	ID3D11ShaderResourceView* GetSkinMatrixSRV(ID3D11Device* Device, ID3D11DeviceContext* Context) const;
	
private:
	void RebuildSectionDraws();
	USkeletalMeshComponent* GetSkeletalMeshComponent() const;
	void ReleaseSkinMatrixBuffer() const;
	bool UpdateSkinMatrixBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context) const;

private:
	mutable FDynamicVertexBuffer DynamicVertexBuffer;
	mutable uint64 UploadedSkinnedRevision = 0;
	uint32 CachedDynamicVertexCount = 0;
	mutable bool bDynamicBufferNeedsCreate = true;

	mutable ID3D11Buffer* SkinMatrixBuffer = nullptr;
	mutable ID3D11ShaderResourceView* SkinMatrixSRV = nullptr;
	mutable uint32 SkinMatrixCapacity = 0;
	mutable uint64 UploadedSkinMatrixRevision = 0;

	// 게임 스레드(UpdateRenderSnapshot)가 채우는 per-frame 동적 스냅샷. 렌더 제출은 컴포넌트가
	// 아닌 이 캐시만 읽는다(렌더 스레드 분리 전제). RenderSnapshotRevision = 스냅샷 시점
	// SkinnedRevision — 렌더가 GPU 업로드 여부를 판단하는 기준.
	TArray<FMatrix>      CachedSkinMatrices;     // GPU 스키닝 경로(본 행렬)
	TArray<FVertexPNCTT> CachedSkinnedVertices;  // CPU 스키닝 경로(스킨된 버텍스)
	uint64 RenderSnapshotRevision = 0;

	// 정적 렌더 버퍼(불변 GPU 리소스) 포인터 — UpdateMesh(게임 스레드, 메시 변경 시)에 캐시.
	// 렌더 제출이 SMC->GetSkeletalMesh()->Asset->RenderBuffer 체인을 타지 않게 한다.
	ID3D11Buffer* CachedStaticVertexBuffer = nullptr;
	uint32        CachedStaticVertexStride = 0;
	ID3D11Buffer* CachedStaticIndexBuffer  = nullptr;
};
