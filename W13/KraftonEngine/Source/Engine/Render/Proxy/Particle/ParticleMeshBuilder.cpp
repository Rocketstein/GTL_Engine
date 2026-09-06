#include "Render/Proxy/Particle/ParticleMeshBuilder.h"

#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Render/Proxy/Particle/ParticleRenderUtils.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/FrameContext.h"
#include "Runtime/Engine.h"

#include <utility>

FParticleMeshBuilder::FParticleMeshBuilder(const FFrameContext& InFrame, const FMatrix& InLocalToWorld, FParticleMaterialCache& InMaterialCache,
	TArray<FVertexPNCTT>& InVertices, TArray<uint32>& InIndices, TArray<FMeshParticleInstanceVertex>& InInstances,
	TArray<FParticleRenderPacket>& InRenderPackets, TArray<FParticleMeshRenderBatch>& InMeshBatches)
	: Frame(InFrame)
	, LocalToWorld(InLocalToWorld)
	, MaterialCache(InMaterialCache)
	, Vertices(InVertices)
	, Indices(InIndices)
	, Instances(InInstances)
	, RenderPackets(InRenderPackets)
	, MeshBatches(InMeshBatches)
{
}

void FParticleMeshBuilder::Build(const FDynamicMeshEmitterData& EmitterData)
{
	const FDynamicSpriteEmitterReplayDataBase& Source = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(EmitterData.GetSource());
	if (Source.ActiveParticleCount <= 0 || ParticleRenderUtils::IsNonePath(Source.MeshPath))
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

	FParticleMeshRenderBatch MeshBatch;
	MeshBatch.Material = MaterialCache.ResolveParticleMaterial(Source);
	MeshBatch.MeshPath = Source.MeshPath;
	MeshBatch.BlendMode = Source.BlendMode;
	MeshBatch.Instances.reserve(Particles.size());

	for (const FParticleProxyParticle& Particle : Particles)
	{
		FMeshParticleInstanceVertex Instance;
		Instance.Location = Particle.Position;
		Instance.Size = Particle.Size;
		Instance.Color = Particle.Color;
		Instance.Rotation = Particle.Rotation;
		MeshBatch.Instances.push_back(Instance);
	}

	MeshBatches.push_back(std::move(MeshBatch));

	UStaticMesh* StaticMesh = ResolveParticleMesh(Source.MeshPath);
	FStaticMesh* MeshAsset = StaticMesh ? StaticMesh->GetStaticMeshAsset() : nullptr;
	if (!MeshAsset || MeshAsset->Vertices.empty() || MeshAsset->Indices.empty())
	{
		return;
	}

	FMeshBuffer* MeshBuffer = StaticMesh->GetLODMeshBuffer(0);
	if (MeshBuffer && MeshBuffer->IsValid() && MeshBuffer->GetIndexBuffer().GetBuffer())
	{
		const uint32 FirstInstance = static_cast<uint32>(Instances.size());
		Instances.reserve(Instances.size() + Particles.size());
		for (const FParticleProxyParticle& Particle : Particles)
		{
			FMeshParticleInstanceVertex Instance;
			Instance.Location = Particle.Position;
			Instance.Size = Particle.Size;
			Instance.Color = Particle.Color;
			Instance.Rotation = Particle.Rotation;
			Instances.push_back(Instance);
		}

		RenderPackets.push_back(MakeInstancedMeshPacket(
			Source,
			StaticMesh,
			FirstInstance,
			static_cast<uint32>(Particles.size()),
			MeshBuffer->GetIndexBuffer().GetIndexCount(),
			Particles));
		return;
	}

	const uint32 StartIndex = static_cast<uint32>(Indices.size());
	ParticleRenderUtils::BuildMeshVertices(Particles, MeshAsset->Vertices, MeshAsset->Indices, Vertices, Indices);

	const uint32 IndexCount = static_cast<uint32>(Indices.size()) - StartIndex;
	if (IndexCount > 0)
	{
		RenderPackets.push_back(MakeCpuExpandedPacket(Source, StartIndex, IndexCount, Particles, StaticMesh));
	}
}

UStaticMesh* FParticleMeshBuilder::ResolveParticleMesh(const FString& MeshPath) const
{
	if (ParticleRenderUtils::IsNonePath(MeshPath) || !GEngine)
	{
		return nullptr;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!Device)
	{
		return nullptr;
	}

	return FMeshManager::LoadStaticMesh(MeshPath, Device);
}

FParticleRenderPacket FParticleMeshBuilder::MakeInstancedMeshPacket(const FDynamicSpriteEmitterReplayDataBase& Source, UStaticMesh* Mesh,
	uint32 FirstInstance, uint32 InstanceCount, uint32 IndexCount, const TArray<FParticleProxyParticle>& Particles) const
{
	FParticleRenderPacket Packet;
	Packet.EmitterType = EDynamicEmitterType::Mesh;
	Packet.PacketType = EParticleRenderPacketType::InstancedMesh;
	Packet.Material = MaterialCache.ResolveParticleMaterial(Source);
	Packet.Mesh = Mesh;
	Packet.MeshPath = Source.MeshPath;
	Packet.BlendMode = Source.BlendMode;
	Packet.FirstIndex = 0;
	Packet.IndexCount = IndexCount;
	Packet.BaseVertex = 0;
	Packet.FirstInstance = FirstInstance;
	Packet.InstanceCount = InstanceCount;
	const EParticleSortMode ResolvedSortMode = ParticleRenderUtils::ResolveParticleSortMode(Source.SortMode, Source.BlendMode);
	Packet.SortDepth = ParticleRenderUtils::ComputePacketSortDepth(Particles, ResolvedSortMode);
	Packet.TranslucencySortPriority = Source.TranslucencySortPriority;
	Packet.bHasTranslucencySort = Source.BlendMode != EParticleBlendMode::Opaque;
	return Packet;
}

FParticleRenderPacket FParticleMeshBuilder::MakeCpuExpandedPacket(const FDynamicSpriteEmitterReplayDataBase& Source, uint32 FirstIndex,
	uint32 IndexCount, const TArray<FParticleProxyParticle>& Particles, UStaticMesh* Mesh) const
{
	FParticleRenderPacket Packet;
	Packet.EmitterType = EDynamicEmitterType::Mesh;
	Packet.PacketType = EParticleRenderPacketType::CpuExpandedMesh;
	Packet.Material = MaterialCache.ResolveParticleMaterial(Source);
	Packet.Mesh = Mesh;
	Packet.MeshPath = Source.MeshPath;
	Packet.BlendMode = Source.BlendMode;
	Packet.FirstIndex = FirstIndex;
	Packet.IndexCount = IndexCount;
	Packet.BaseVertex = 0;
	const EParticleSortMode ResolvedSortMode = ParticleRenderUtils::ResolveParticleSortMode(Source.SortMode, Source.BlendMode);
	Packet.SortDepth = ParticleRenderUtils::ComputePacketSortDepth(Particles, ResolvedSortMode);
	Packet.TranslucencySortPriority = Source.TranslucencySortPriority;
	Packet.bHasTranslucencySort = Source.BlendMode != EParticleBlendMode::Opaque;
	return Packet;
}
