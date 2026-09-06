#pragma once

#include "Math/Matrix.h"
#include "Render/Proxy/Particle/ParticleMaterialCache.h"
#include "Render/Proxy/Particle/ParticleRenderTypes.h"
#include "Render/Types/VertexTypes.h"

class UStaticMesh;
struct FFrameContext;

class FParticleMeshBuilder
{
public:
	FParticleMeshBuilder(const FFrameContext& InFrame, const FMatrix& InLocalToWorld, FParticleMaterialCache& InMaterialCache,
		TArray<FVertexPNCTT>& InVertices, TArray<uint32>& InIndices, TArray<FMeshParticleInstanceVertex>& InInstances,
		TArray<FParticleRenderPacket>& InRenderPackets, TArray<FParticleMeshRenderBatch>& InMeshBatches);

	void Build(const FDynamicMeshEmitterData& EmitterData);

private:
	UStaticMesh* ResolveParticleMesh(const FString& MeshPath) const;
	FParticleRenderPacket MakeInstancedMeshPacket(const FDynamicSpriteEmitterReplayDataBase& Source, UStaticMesh* Mesh,
		uint32 FirstInstance, uint32 InstanceCount, uint32 IndexCount, const TArray<FParticleProxyParticle>& Particles) const;
	FParticleRenderPacket MakeCpuExpandedPacket(const FDynamicSpriteEmitterReplayDataBase& Source, uint32 FirstIndex,
		uint32 IndexCount, const TArray<FParticleProxyParticle>& Particles, UStaticMesh* Mesh) const;

	const FFrameContext& Frame;
	const FMatrix& LocalToWorld;
	FParticleMaterialCache& MaterialCache;
	TArray<FVertexPNCTT>& Vertices;
	TArray<uint32>& Indices;
	TArray<FMeshParticleInstanceVertex>& Instances;
	TArray<FParticleRenderPacket>& RenderPackets;
	TArray<FParticleMeshRenderBatch>& MeshBatches;
};
