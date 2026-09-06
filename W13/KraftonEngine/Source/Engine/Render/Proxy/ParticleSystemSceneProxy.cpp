#include "Render/Proxy/ParticleSystemSceneProxy.h"
#include "GameFramework/AActor.h"

#include "Component/Primitive/ParticleSystemComponent.h"
#include "Mesh/Static/StaticMesh.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModuleVectorField.h"
#include "Particle/ParticleSystem.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Proxy/Particle/ParticleMeshBuilder.h"
#include "Render/Proxy/Particle/ParticleRenderUtils.h"
#include "Render/Proxy/Particle/ParticleSpriteBuilder.h"
#include "Render/Proxy/Particle/ParticleTrailBuilder.h"
#include "Render/Resource/Buffer.h"
#include "Render/Scene/FScene.h"
#include "Render/Types/FrameContext.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
	float FindEmitterTime(const TArray<FDynamicEmitterDataBase*>& DynamicEmitters, int32 EmitterIndex)
	{
		for (const FDynamicEmitterDataBase* DynamicEmitter : DynamicEmitters)
		{
			if (DynamicEmitter && DynamicEmitter->EmitterIndex == EmitterIndex)
			{
				return DynamicEmitter->GetSource().EmitterTime;
			}
		}
		return 0.0f;
	}

	void ApplyParticleBlendModeOverride(FMeshSectionDraw& Draw, EParticleBlendMode BlendMode)
	{
		Draw.bHasRenderStateOverride = true;

		switch (BlendMode)
		{
		case EParticleBlendMode::Opaque:
			Draw.OverrideRenderPass = ERenderPass::Opaque;
			Draw.OverrideBlendState = EBlendState::Opaque;
			Draw.OverrideDepthStencilState = EDepthStencilState::DepthGreaterEqual;
			break;
		case EParticleBlendMode::Additive:
			Draw.OverrideRenderPass = ERenderPass::AdditiveDecal;
			Draw.OverrideBlendState = EBlendState::Additive;
			Draw.OverrideDepthStencilState = EDepthStencilState::DepthReadOnly;
			break;
		case EParticleBlendMode::AlphaBlend:
		default:
			Draw.OverrideRenderPass = ERenderPass::AlphaBlend;
			Draw.OverrideBlendState = EBlendState::AlphaBlend;
			Draw.OverrideDepthStencilState = EDepthStencilState::DepthReadOnly;
			break;
		}
	}
}

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate | EPrimitiveProxyFlags::NeverCull | EPrimitiveProxyFlags::ParticleSystem;
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	ClearDynamicData();
}

void FParticleSystemSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();
	
	// Particle vertices are generated in world space by the CPU builders.
	// We override the Model matrix to Identity to prevent transforming them twice.
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(FMatrix::Identity);
	
	bDynamicDataDirty = true;
}

void FParticleSystemSceneProxy::UpdateVisibility()
{
	FPrimitiveSceneProxy::UpdateVisibility();
}

void FParticleSystemSceneProxy::UpdateMaterial()
{
	// Emitter마다 material이 달라질 수 있으므로 SectionDraws는 view rebuild 때 구성한다.
}

void FParticleSystemSceneProxy::UpdateMesh()
{
	// Particle은 static mesh buffer를 owner에서 가져오지 않는다.
}

void FParticleSystemSceneProxy::UpdateDynamicData(TArray<FDynamicEmitterDataBase*>&& InEmitterData)
{
	ClearDynamicData();
	DynamicEmitters = std::move(InEmitterData);
	bDynamicDataDirty = true;
}

void FParticleSystemSceneProxy::ClearDynamicData()
{
	for (FDynamicEmitterDataBase* DynamicData : DynamicEmitters)
	{
		delete DynamicData;
	}
	DynamicEmitters.clear();

	DrawBufferCache.ClearViewData();
	RenderPackets.clear();
	CachedMeshBatches.clear();
	SectionDraws.clear();
	bDynamicDataDirty = true;
	LastDrawCallCount = 0;
	LastRenderBuildTimeMs = 0.0;
}

void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	//아래 코드 망할 가능성 있음.
	const UParticleSystemComponent* Component = GetParticleComponent();
	AActor* OwnerActor = Component ? Component->GetOwner() : nullptr;

	if (!Frame.RenderOptions.ShowFlags.bParticle
		|| !Component
		|| !Component->IsVisible()
		|| (OwnerActor && !OwnerActor->IsVisible()))
	{
		bVisible = false;
		LastDrawCallCount = 0;
		LastRenderBuildTimeMs = 0.0;
		return;
	}

	bVisible = true;

	RebuildRenderDataForView(Frame);
	bDynamicDataDirty = false;
}

void FParticleSystemSceneProxy::AppendDebugLines(FScene& Scene) const
{
	UParticleSystemComponent* Component = GetParticleComponent();
	UParticleSystem* Template = Component ? Component->GetTemplate() : nullptr;
	if (!Component || !Template)
	{
		return;
	}

	const FMatrix ComponentToWorld = Component->GetWorldMatrix();
	const int32 LODIndex = Component->GetCurrentLODIndex();
	const TArray<UParticleEmitter*>& Emitters = Template->GetEmitters();
	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters.size()); ++EmitterIndex)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (!Emitter || !Emitter->IsEnabled())
		{
			continue;
		}

		UParticleLODLevel* LODLevel = Emitter->GetLODLevelForIndex(LODIndex);
		if (!LODLevel)
		{
			continue;
		}

		const float EmitterTime = FindEmitterTime(DynamicEmitters, EmitterIndex);
		for (UParticleModule* Module : LODLevel->GetModules())
		{
			if (UParticleModuleVectorFieldLocal* VectorField = Cast<UParticleModuleVectorFieldLocal>(Module))
			{
				VectorField->AppendFieldDebugLines(Scene, ComponentToWorld, LODLevel, EmitterTime);
			}
		}
	}
}

void FParticleSystemSceneProxy::AppendBoundsDebugLines(FScene& Scene) const
{
	const UParticleSystemComponent* Component = GetParticleComponent();
	if (!Component)
	{
		return;
	}

	const FMatrix LocalToWorld = Component->GetWorldMatrix();
	FBoundingBox Bounds;
	bool bHasBounds = false;

	for (const FDynamicEmitterDataBase* EmitterData : DynamicEmitters)
	{
		if (!EmitterData)
		{
			continue;
		}

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		const uint8* ParticleData = Source.DataContainer.ParticleData;
		const uint16* ParticleIndices = Source.DataContainer.ParticleIndices;
		if (!ParticleData || Source.ActiveParticleCount <= 0 || Source.ParticleStride <= 0)
		{
			continue;
		}

		for (int32 ParticleIndex = 0; ParticleIndex < Source.ActiveParticleCount; ++ParticleIndex)
		{
			const int32 DataIndex = ParticleIndices ? ParticleIndices[ParticleIndex] : ParticleIndex;
			const FBaseParticle* Particle = reinterpret_cast<const FBaseParticle*>(
				ParticleData + static_cast<size_t>(DataIndex) * Source.ParticleStride);
			const FVector Position = Source.bUseLocalSpace
				? LocalToWorld.TransformPositionWithW(Particle->Location)
				: Particle->Location;
			const FVector Extent(
				std::abs(Particle->Size.X * Source.Scale.X) * 0.5f,
				std::abs(Particle->Size.Y * Source.Scale.Y) * 0.5f,
				std::abs(Particle->Size.Z * Source.Scale.Z) * 0.5f);

			if (!bHasBounds)
			{
				Bounds = FBoundingBox(Position - Extent, Position + Extent);
				bHasBounds = true;
			}
			else
			{
				Bounds.Expand(Position - Extent);
				Bounds.Expand(Position + Extent);
			}
		}
	}

	if (bHasBounds)
	{
		const FColor BoxColor = FColor::White();
		const FColor SphereColor = FColor::Yellow();
		const FVector Center = Bounds.GetCenter();
		const float Radius = Bounds.GetExtent().Length();
		constexpr int32 SphereSegments = 32;
		constexpr float FullCircle = 6.28318530717958647692f;

		Scene.AddDebugAABB(Bounds.Min, Bounds.Max, BoxColor);
		if (Radius <= 0.0f)
		{
			return;
		}

		for (int32 Segment = 0; Segment < SphereSegments; ++Segment)
		{
			const float Angle0 = FullCircle * static_cast<float>(Segment) / static_cast<float>(SphereSegments);
			const float Angle1 = FullCircle * static_cast<float>(Segment + 1) / static_cast<float>(SphereSegments);
			const float Cos0 = std::cos(Angle0) * Radius;
			const float Sin0 = std::sin(Angle0) * Radius;
			const float Cos1 = std::cos(Angle1) * Radius;
			const float Sin1 = std::sin(Angle1) * Radius;

			Scene.AddDebugLine(
				Center + FVector(Cos0, Sin0, 0.0f),
				Center + FVector(Cos1, Sin1, 0.0f),
				SphereColor);
			Scene.AddDebugLine(
				Center + FVector(Cos0, 0.0f, Sin0),
				Center + FVector(Cos1, 0.0f, Sin1),
				SphereColor);
			Scene.AddDebugLine(
				Center + FVector(0.0f, Cos0, Sin0),
				Center + FVector(0.0f, Cos1, Sin1),
				SphereColor);
		}
	}
}

void FParticleSystemSceneProxy::RebuildRenderDataForView(const FFrameContext& Frame)
{
	const auto StartTime = std::chrono::steady_clock::now();
	SCOPE_STAT_CAT("ParticleRenderBuild", "Particles");

	DrawBufferCache.ClearViewData();
	RenderPackets.clear();
	CachedMeshBatches.clear();
	SectionDraws.clear();

	for (FDynamicEmitterDataBase* EmitterData : DynamicEmitters)
	{
		BuildEmitterForView(EmitterData, Frame);
	}

	SortRenderPacketsForView();
	RebuildSectionDrawsFromRenderPackets();
	const auto EndTime = std::chrono::steady_clock::now();
	LastRenderBuildTimeMs = std::chrono::duration<double, std::milli>(EndTime - StartTime).count();
}

void FParticleSystemSceneProxy::BuildEmitterForView(FDynamicEmitterDataBase* EmitterData, const FFrameContext& Frame)
{
	if (!EmitterData)
	{
		return;
	}

	const UParticleSystemComponent* Component = GetParticleComponent();
	const FMatrix LocalToWorld = Component ? Component->GetWorldMatrix() : FMatrix::Identity;

	const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
	switch (Source.EmitterType)
	{
	case EDynamicEmitterType::Sprite:
	{
		FParticleSpriteBuilder Builder(Frame, LocalToWorld, MaterialCache, DrawBufferCache.GetParticleVertices(),
			DrawBufferCache.GetParticleIndices(), RenderPackets);
		Builder.Build(static_cast<const FDynamicSpriteEmitterDataBase&>(*EmitterData));
		break;
	}
	case EDynamicEmitterType::Mesh:
	{
		FParticleMeshBuilder Builder(Frame, LocalToWorld, MaterialCache, DrawBufferCache.GetParticleVertices(),
			DrawBufferCache.GetParticleIndices(), DrawBufferCache.GetMeshInstances(), RenderPackets, CachedMeshBatches);
		Builder.Build(static_cast<const FDynamicMeshEmitterData&>(*EmitterData));
		break;
	}
	case EDynamicEmitterType::Beam:
	{
		FParticleTrailBuilder Builder(Frame, LocalToWorld, MaterialCache, DrawBufferCache.GetParticleVertices(),
			DrawBufferCache.GetParticleIndices(), RenderPackets);
		Builder.BuildBeam(static_cast<const FDynamicBeamEmitterData&>(*EmitterData));
		break;
	}
	case EDynamicEmitterType::Ribbon:
	{
		FParticleTrailBuilder Builder(Frame, LocalToWorld, MaterialCache, DrawBufferCache.GetParticleVertices(),
			DrawBufferCache.GetParticleIndices(), RenderPackets);
		Builder.BuildRibbon(static_cast<const FDynamicRibbonEmitterData&>(*EmitterData));
		break;
	}
	case EDynamicEmitterType::Unknown:
	default:
		break;
	}
}

void FParticleSystemSceneProxy::SortRenderPacketsForView()
{
	std::stable_sort(RenderPackets.begin(), RenderPackets.end(), ParticleRenderUtils::SortRenderPacketForDraw);
}

void FParticleSystemSceneProxy::RebuildSectionDrawsFromRenderPackets()
{
	SectionDraws.clear();
	SectionDraws.reserve(RenderPackets.size());

	for (const FParticleRenderPacket& Packet : RenderPackets)
	{
		if (!Packet.HasIndexRange())
		{
			continue;
		}

		FMeshSectionDraw Draw;
		Draw.Material = Packet.Material;
		Draw.FirstIndex = Packet.FirstIndex;
		Draw.IndexCount = Packet.IndexCount;
		Draw.bForceUnlitShader = Packet.PacketType == EParticleRenderPacketType::CpuExpandedSprite
			|| Packet.PacketType == EParticleRenderPacketType::CpuExpandedBeam
			|| Packet.PacketType == EParticleRenderPacketType::CpuExpandedRibbon;
		ApplyParticleBlendModeOverride(Draw, Packet.BlendMode);
		Draw.bHasTranslucencySort = Packet.bHasTranslucencySort;
		Draw.SortDepth = Packet.SortDepth;
		Draw.TranslucencySortPriority = Packet.TranslucencySortPriority;
		if (Packet.PacketType == EParticleRenderPacketType::InstancedMesh && Packet.Mesh)
		{
			FMeshBuffer* MeshBuffer = Packet.Mesh->GetLODMeshBuffer(0);
			if (!MeshBuffer || !MeshBuffer->IsValid() || !MeshBuffer->GetIndexBuffer().GetBuffer()
				|| Packet.InstanceCount == 0)
			{
				continue;
			}

			Draw.bInstanced = true;
			Draw.VertexBuffer = MeshBuffer->GetVertexBuffer().GetBuffer();
			Draw.VertexStride = MeshBuffer->GetVertexBuffer().GetStride();
			Draw.IndexBuffer = MeshBuffer->GetIndexBuffer().GetBuffer();
			Draw.InstanceBuffer = DrawBufferCache.GetInstanceBuffer();
			Draw.InstanceStride = sizeof(FMeshParticleInstanceVertex);
			Draw.InstanceCount = Packet.InstanceCount;
			Draw.StartInstance = Packet.FirstInstance;
		}
		SectionDraws.push_back(Draw);
	}

	LastDrawCallCount = static_cast<uint32>(SectionDraws.size());
}

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	TArray<FMeshSectionDraw>& MutableDraws = const_cast<TArray<FMeshSectionDraw>&>(SectionDraws);
	return DrawBufferCache.PrepareDrawBuffer(Device, Context, OutBuffer, MutableDraws);
}

UParticleSystemComponent* FParticleSystemSceneProxy::GetParticleComponent() const
{
	return static_cast<UParticleSystemComponent*>(GetOwner());
}
