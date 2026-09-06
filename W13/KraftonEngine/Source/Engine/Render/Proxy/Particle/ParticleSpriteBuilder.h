#pragma once

#include "Math/Matrix.h"
#include "Render/Proxy/Particle/ParticleMaterialCache.h"
#include "Render/Proxy/Particle/ParticleRenderTypes.h"
#include "Render/Types/VertexTypes.h"

struct FFrameContext;

class FParticleSpriteBuilder
{
public:
	FParticleSpriteBuilder(const FFrameContext& InFrame, const FMatrix& InLocalToWorld, FParticleMaterialCache& InMaterialCache,
		TArray<FVertexPNCTT>& InVertices, TArray<uint32>& InIndices, TArray<FParticleRenderPacket>& InRenderPackets);

	void Build(const FDynamicSpriteEmitterDataBase& EmitterData);

private:
	FParticleRenderPacket MakeCpuExpandedPacket(const FDynamicSpriteEmitterReplayDataBase& Source, uint32 FirstIndex,
		uint32 IndexCount, const TArray<FParticleProxyParticle>& Particles) const;

	const FFrameContext& Frame;
	const FMatrix& LocalToWorld;
	FParticleMaterialCache& MaterialCache;
	TArray<FVertexPNCTT>& Vertices;
	TArray<uint32>& Indices;
	TArray<FParticleRenderPacket>& RenderPackets;
};
