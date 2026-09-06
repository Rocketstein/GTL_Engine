#pragma once

#include "Math/Matrix.h"
#include "Render/Proxy/Particle/ParticleMaterialCache.h"
#include "Render/Proxy/Particle/ParticleRenderTypes.h"
#include "Render/Types/VertexTypes.h"

struct FFrameContext;

class FParticleTrailBuilder
{
public:
	FParticleTrailBuilder(const FFrameContext& InFrame, const FMatrix& InLocalToWorld, FParticleMaterialCache& InMaterialCache,
		TArray<FVertexPNCTT>& InVertices, TArray<uint32>& InIndices, TArray<FParticleRenderPacket>& InRenderPackets);

	void BuildBeam(const FDynamicBeamEmitterData& EmitterData);
	void BuildRibbon(const FDynamicRibbonEmitterData& EmitterData);

private:
	void BuildBeamVertices(const TArray<FParticleProxyParticle>& Particles, const FDynamicBeamEmitterReplayData& Source) const;
	void BuildRibbonTrailVertices(const TArray<FParticleProxyParticle>& Points, int32 StartIndex, int32 PointCount,
		const FDynamicRibbonEmitterReplayData& Source) const;
	FParticleRenderPacket MakeCpuExpandedPacket(EDynamicEmitterType EmitterType, EParticleRenderPacketType PacketType,
		const FDynamicSpriteEmitterReplayDataBase& Source, uint32 FirstIndex, uint32 IndexCount,
		const TArray<FParticleProxyParticle>& Particles) const;

	const FFrameContext& Frame;
	const FMatrix& LocalToWorld;
	FParticleMaterialCache& MaterialCache;
	TArray<FVertexPNCTT>& Vertices;
	TArray<uint32>& Indices;
	TArray<FParticleRenderPacket>& RenderPackets;
};
