#pragma once

#include "Particle/ParticleDynamicData.h"
#include "Render/Proxy/Particle/ParticleRenderTypes.h"
#include "Render/Types/VertexTypes.h"

class FDynamicIndexBuffer;
class FDynamicVertexBuffer;
struct FDrawCommandBuffer;
struct FMeshSectionDraw;
struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;

class FParticleDrawBufferCache
{
public:
	FParticleDrawBufferCache();
	~FParticleDrawBufferCache();

	void ClearViewData();
	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer,
		TArray<FMeshSectionDraw>& SectionDraws) const;
	ID3D11Buffer* GetInstanceBuffer() const;

	TArray<FVertexPNCTT>& GetParticleVertices() { return ParticleVertices; }
	TArray<uint32>& GetParticleIndices() { return ParticleIndices; }
	TArray<FMeshParticleInstanceVertex>& GetMeshInstances() { return MeshInstances; }

	const TArray<FVertexPNCTT>& GetParticleVertices() const { return ParticleVertices; }
	const TArray<uint32>& GetParticleIndices() const { return ParticleIndices; }
	const TArray<FMeshParticleInstanceVertex>& GetMeshInstances() const { return MeshInstances; }

private:
	TArray<FVertexPNCTT> ParticleVertices;
	TArray<uint32> ParticleIndices;
	TArray<FMeshParticleInstanceVertex> MeshInstances;

	FDynamicVertexBuffer* DynamicParticleVB = nullptr;
	FDynamicIndexBuffer* DynamicParticleIB = nullptr;
	FDynamicVertexBuffer* DynamicMeshInstanceVB = nullptr;
};
