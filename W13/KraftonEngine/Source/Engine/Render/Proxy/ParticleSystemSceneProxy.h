#pragma once

#include "PrimitiveSceneProxy.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"
#include "Particle/ParticleDynamicData.h"
#include "Render/Proxy/Particle/ParticleDrawBufferCache.h"
#include "Render/Proxy/Particle/ParticleMaterialCache.h"
#include "Render/Proxy/Particle/ParticleRenderTypes.h"

class UParticleSystemComponent;
class UMaterial;
class UStaticMesh;
class FMeshBuffer;
class FScene;
struct FFrameContext;
struct FDrawCommandBuffer;

/*
UParticleSystemComponent가 시뮬레이션한 파티클 결과로 렌더링 준비
- PSC는 시뮬레이션만 담당하고, View-dependent 렌더링 판단은 proxy가 담당.
- 아래는 CPU에서 처리해 GPU로 넘기기 직전까지의 데이터 흐름.

1. Game/Simulation 단계
   - UParticleSystemComponent와 FParticleEmitterInstance
   Particle에 대해 Spawn/Update/Kill.
   -> 이때 Update 정보는 FBaseParticle 저장

2. ReplayData 단계
   - Tick이 끝난 뒤 PSC에서 render proxy가 볼 수 있도록 처리
   -> FBaseParticle 메모리를 DataContainer로 복사하고, 이걸로 emitter instance마다
   -> FDynamicEmitterDataBase / FDynamicEmitterReplayDataBase 생성.
   -> emitter type/material/mesh/subUV/blend 에 대한 렌더링 원본 데이터.

3. SceneProxy 단계
   - FParticleSystemSceneProxy는 ReplayData를 받아 렌더링 데이터 생성
   -> Billboard(Sprite), Instancing(Sprite), SubUV(Sprite) 관련 처리

4. DrawCommand 단계
   - FMeshSectionDraw / FDrawCommandBuffer를 통해 draw call 생성.
   - proxy 내부에서는 FParticleRenderPacket이라는 중간 단위를 만들고,
   마지막에 기존 렌더링을 위한 SectionDraws로 동기화.

================================================================================
*/

/*
	FParticleSystemSceneProxy
	-------------------------
	- Component는 게임 월드의 객체이고, Tick/모듈/시뮬레이션을 담당.
	- Renderer는 component를 직접 알 필요가 없고, 렌더링에 필요한 스냅샷만 필요.
	- SceneProxy는 그 사이에서 component의 현재 상태를 변환해 렌더러에게 전달.
*/
class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);
	virtual ~FParticleSystemSceneProxy() override;

	// FPrimitiveSceneProxy interface
	virtual void UpdateTransform() override;
	virtual void UpdateVisibility() override;
	virtual void UpdateMaterial() override;
	virtual void UpdateMesh() override;

	// PSC가 Tick 이후에 만든 emitter replay data의 소유권을 proxy로 넘긴다.
	// 이 함수의 입력은 Sprite/Mesh/Beam/Ribbon 등 emitter type을 유지하므로,
	// proxy 내부에서 타입별 렌더 경로로 분기할 수 있다.
	void UpdateDynamicData(TArray<FDynamicEmitterDataBase*>&& InEmitterData);
	void ClearDynamicData();

	// View-dependent particle
	virtual void UpdatePerViewport(const FFrameContext& Frame) override;
	void AppendDebugLines(FScene& Scene) const;
	void AppendBoundsDebugLines(FScene& Scene) const;

	// proxy가 만든 CPU-expanded vertex/index와 mesh instance 데이터를
	// GPU dynamic buffer로 업로드한다.
	virtual bool PrepareDrawBuffer(ID3D11Device* Device,
		ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const override;

	uint32 GetParticleDrawCallCount() const { return bVisible ? LastDrawCallCount : 0; }
	double GetLastRenderBuildTimeMs() const { return bVisible ? LastRenderBuildTimeMs : 0.0; }

private:
	UParticleSystemComponent* GetParticleComponent() const;

	// 현재 View 기준으로 모든 emitter를 다시 빌드한다.
	void RebuildRenderDataForView(const FFrameContext& Frame);

	// ReplayData의 EmitterType을 보고 Sprite/Mesh/Beam/Ribbon 빌더로 분배한다.
	void BuildEmitterForView(FDynamicEmitterDataBase* EmitterData, const FFrameContext& Frame);

	void SortRenderPacketsForView();
	void RebuildSectionDrawsFromRenderPackets();

private:
	TArray<FDynamicEmitterDataBase*> DynamicEmitters;

	FParticleDrawBufferCache DrawBufferCache;

	// ReplayData에서 view-dependent geometry를 만든 뒤 draw unit 단위로 나눈 결과.
	// DrawCommandBuilder는 아직 FMeshSectionDraw를 읽기 때문에, RenderPackets를 SectionDraws로 동기화.
	TArray<FParticleRenderPacket> RenderPackets;

	// instancing 전환/디버깅용 bridge 데이터.
	// 현재 주 draw path는 RenderPackets지만, batch 단위 instance 정보를 확인하거나 향후 section/material 분리에 활용.
	TArray<FParticleMeshRenderBatch> CachedMeshBatches;

	FParticleMaterialCache MaterialCache;

	bool bDynamicDataDirty = true;
	uint32 LastDrawCallCount = 0;
	double LastRenderBuildTimeMs = 0.0;
};
