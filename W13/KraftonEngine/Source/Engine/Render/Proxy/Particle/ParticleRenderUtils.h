#pragma once

#include "Math/Matrix.h"
#include "Render/Proxy/Particle/ParticleRenderTypes.h"
#include "Render/Types/VertexTypes.h"

struct FFrameContext;
struct FNormalVertex;

namespace ParticleRenderUtils
{
	bool IsNonePath(const FString& Path);
	EParticleSortMode ResolveParticleSortMode(EParticleSortMode SortMode, EParticleBlendMode BlendMode);
	bool ShouldSortParticles(EParticleSortMode SortMode);
	void GatherParticles(const FDynamicEmitterReplayDataBase& Source, const FFrameContext& Frame,
		const FMatrix& LocalToWorld, TArray<FParticleProxyParticle>& OutParticles);
	void SortParticlesForView(TArray<FParticleProxyParticle>& Particles, EParticleSortMode SortMode);
	FVector2 BuildSubUV(const FDynamicSpriteEmitterReplayDataBase& Source, const FParticleProxyParticle& Particle, float U, float V,
		int32 ResolvedSubImagesX, int32 ResolvedSubImagesY);
	void BuildSpriteVertices(const FDynamicSpriteEmitterReplayDataBase& Source, const TArray<FParticleProxyParticle>& Particles,
		const FVector& CameraPosition, const FVector& CameraRight, const FVector& CameraUp, int32 ResolvedSubImagesX, int32 ResolvedSubImagesY,
		TArray<FVertexPNCTT>& OutVertices, TArray<uint32>& OutIndices);
	void BuildMeshVertices(const TArray<FParticleProxyParticle>& Particles, const TArray<FNormalVertex>& MeshVertices,
		const TArray<uint32>& MeshIndices, TArray<FVertexPNCTT>& OutVertices, TArray<uint32>& OutIndices);
	float ComputePacketSortDepth(const TArray<FParticleProxyParticle>& Particles, EParticleSortMode SortMode);
	bool SortRenderPacketForDraw(const FParticleRenderPacket& A, const FParticleRenderPacket& B);
}
