#include "Render/Proxy/Particle/ParticleDrawBufferCache.h"

#include "Render/Command/DrawCommand.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/RenderConstants.h"

#include <algorithm>

FParticleDrawBufferCache::FParticleDrawBufferCache()
{
	DynamicParticleVB = new FDynamicVertexBuffer();
	DynamicParticleIB = new FDynamicIndexBuffer();
	DynamicMeshInstanceVB = new FDynamicVertexBuffer();
}

FParticleDrawBufferCache::~FParticleDrawBufferCache()
{
	delete DynamicParticleVB;
	DynamicParticleVB = nullptr;

	delete DynamicParticleIB;
	DynamicParticleIB = nullptr;

	delete DynamicMeshInstanceVB;
	DynamicMeshInstanceVB = nullptr;
}

void FParticleDrawBufferCache::ClearViewData()
{
	ParticleVertices.clear();
	ParticleIndices.clear();
	MeshInstances.clear();
}

ID3D11Buffer* FParticleDrawBufferCache::GetInstanceBuffer() const
{
	return DynamicMeshInstanceVB ? DynamicMeshInstanceVB->GetBuffer() : nullptr;
}

bool FParticleDrawBufferCache::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer,
	TArray<FMeshSectionDraw>& SectionDraws) const
{
	if ((ParticleVertices.empty() || ParticleIndices.empty()) && MeshInstances.empty())
	{
		return false;
	}

	if ((!ParticleVertices.empty() && (!DynamicParticleVB || !DynamicParticleIB))
		|| (!MeshInstances.empty() && !DynamicMeshInstanceVB))
	{
		return false;
	}

	OutBuffer = {};

	if (!ParticleVertices.empty() && !ParticleIndices.empty())
	{
		const uint32 RequiredVertexCount = static_cast<uint32>(ParticleVertices.size());
		const uint32 RequiredIndexCount = static_cast<uint32>(ParticleIndices.size());

		if (DynamicParticleVB->GetMaxCount() == 0)
		{
			const uint32 InitialVertexCount = (std::max)(256u, RequiredVertexCount);
			DynamicParticleVB->Create(Device, InitialVertexCount, sizeof(FVertexPNCTT));
		}
		else
		{
			DynamicParticleVB->EnsureCapacity(Device, RequiredVertexCount);
		}

		if (DynamicParticleIB->GetMaxCount() == 0)
		{
			const uint32 InitialIndexCount = (std::max)(256u, RequiredIndexCount);
			DynamicParticleIB->Create(Device, InitialIndexCount);
		}
		else
		{
			DynamicParticleIB->EnsureCapacity(Device, RequiredIndexCount);
		}

		if (!DynamicParticleVB->Update(Context, ParticleVertices.data(), RequiredVertexCount)
			|| !DynamicParticleIB->Update(Context, ParticleIndices.data(), RequiredIndexCount))
		{
			return false;
		}

		OutBuffer.VB = DynamicParticleVB->GetBuffer();
		OutBuffer.VBStride = DynamicParticleVB->GetStride();
		OutBuffer.IB = DynamicParticleIB->GetBuffer();
		OutBuffer.IndexCount = RequiredIndexCount;
		OutBuffer.VertexCount = RequiredVertexCount;
		OutBuffer.FirstIndex = 0;
		OutBuffer.BaseVertex = 0;
	}

	if (!MeshInstances.empty())
	{
		const uint32 RequiredInstanceCount = static_cast<uint32>(MeshInstances.size());
		if (DynamicMeshInstanceVB->GetMaxCount() == 0)
		{
			const uint32 InitialInstanceCount = (std::max)(256u, RequiredInstanceCount);
			DynamicMeshInstanceVB->Create(Device, InitialInstanceCount, sizeof(FMeshParticleInstanceVertex));
		}
		else
		{
			DynamicMeshInstanceVB->EnsureCapacity(Device, RequiredInstanceCount);
		}

		if (!DynamicMeshInstanceVB->Update(Context, MeshInstances.data(), RequiredInstanceCount))
		{
			return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
		}

		ID3D11Buffer* InstanceBuffer = DynamicMeshInstanceVB->GetBuffer();
		for (FMeshSectionDraw& Draw : SectionDraws)
		{
			if (Draw.bInstanced)
			{
				Draw.InstanceBuffer = InstanceBuffer;
				if (!OutBuffer.VB)
				{
					OutBuffer.VB = Draw.VertexBuffer;
					OutBuffer.VBStride = Draw.VertexStride;
					OutBuffer.IB = Draw.IndexBuffer;
					OutBuffer.IndexCount = Draw.IndexCount;
					OutBuffer.FirstIndex = Draw.FirstIndex;
					OutBuffer.BaseVertex = 0;
				}
			}
		}
	}

	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}
