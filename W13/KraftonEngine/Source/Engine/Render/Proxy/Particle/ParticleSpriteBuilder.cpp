#include "Render/Proxy/Particle/ParticleSpriteBuilder.h"

#include "Render/Proxy/Particle/ParticleRenderUtils.h"
#include "Render/Types/FrameContext.h"

#include <algorithm>

FParticleSpriteBuilder::FParticleSpriteBuilder(const FFrameContext& InFrame, const FMatrix& InLocalToWorld, FParticleMaterialCache& InMaterialCache,
	TArray<FVertexPNCTT>& InVertices, TArray<uint32>& InIndices, TArray<FParticleRenderPacket>& InRenderPackets)
	: Frame(InFrame)
	, LocalToWorld(InLocalToWorld)
	, MaterialCache(InMaterialCache)
	, Vertices(InVertices)
	, Indices(InIndices)
	, RenderPackets(InRenderPackets)
{
}

void FParticleSpriteBuilder::Build(const FDynamicSpriteEmitterDataBase& EmitterData)
{
	const FDynamicSpriteEmitterReplayDataBase& Source = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(EmitterData.GetSource());
	if (Source.ActiveParticleCount <= 0)
	{
		return;
	}

	TArray<FParticleProxyParticle> Particles;
	ParticleRenderUtils::GatherParticles(Source, Frame, LocalToWorld, Particles);
	if (Particles.empty())
	{
		return;
	}

	const EParticleSortMode ResolvedSortMode = ParticleRenderUtils::ResolveParticleSortMode(Source.SortMode, Source.BlendMode);
	if (ParticleRenderUtils::ShouldSortParticles(ResolvedSortMode))
	{
		ParticleRenderUtils::SortParticlesForView(Particles, ResolvedSortMode);
	}

	int32 ResolvedSubImagesX = Source.SubImagesX;
	int32 ResolvedSubImagesY = Source.SubImagesY;
	if (const FTextureAtlasResource* SubUVResource = MaterialCache.ResolveSubUVResource(Source))
	{
		ResolvedSubImagesX = static_cast<int32>((std::max)(1u, SubUVResource->Columns));
		ResolvedSubImagesY = static_cast<int32>((std::max)(1u, SubUVResource->Rows));
	}

	const uint32 StartIndex = static_cast<uint32>(Indices.size());
	ParticleRenderUtils::BuildSpriteVertices(Source, Particles, Frame.CameraPosition, Frame.CameraRight, Frame.CameraUp, ResolvedSubImagesX, ResolvedSubImagesY,
		Vertices, Indices);

	const uint32 IndexCount = static_cast<uint32>(Indices.size()) - StartIndex;
	if (IndexCount > 0)
	{
		RenderPackets.push_back(MakeCpuExpandedPacket(Source, StartIndex, IndexCount, Particles));
	}
}

FParticleRenderPacket FParticleSpriteBuilder::MakeCpuExpandedPacket(const FDynamicSpriteEmitterReplayDataBase& Source, uint32 FirstIndex,
	uint32 IndexCount, const TArray<FParticleProxyParticle>& Particles) const
{
	FParticleRenderPacket Packet;
	Packet.EmitterType = EDynamicEmitterType::Sprite;
	Packet.PacketType = EParticleRenderPacketType::CpuExpandedSprite;
	Packet.Material = MaterialCache.ResolveParticleMaterial(Source);
	Packet.MeshPath = Source.MeshPath;
	Packet.BlendMode = Source.BlendMode;
	Packet.FirstIndex = FirstIndex;
	Packet.IndexCount = IndexCount;
	const EParticleSortMode ResolvedSortMode = ParticleRenderUtils::ResolveParticleSortMode(Source.SortMode, Source.BlendMode);
	Packet.SortDepth = ParticleRenderUtils::ComputePacketSortDepth(Particles, ResolvedSortMode);
	Packet.TranslucencySortPriority = Source.TranslucencySortPriority;
	Packet.bHasTranslucencySort = Source.BlendMode != EParticleBlendMode::Opaque;
	return Packet;
}
