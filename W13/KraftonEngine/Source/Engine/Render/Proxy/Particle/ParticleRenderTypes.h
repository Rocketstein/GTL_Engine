#pragma once

#include "Core/Types/ResourceTypes.h"
#include "Particle/ParticleDynamicData.h"

class UMaterial;
class UStaticMesh;

/*
	FParticleProxyParticle
	----------------------
	ReplayData 안의 FBaseParticle을 proxy가 다루기 쉬운 형태로 복사한 View용 파티클.
*/
struct FParticleProxyParticle
{
	int32 SourceParticleIndex = -1;
	FVector OldPosition = FVector::ZeroVector;
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector Size = FVector::OneVector;
	FLinearColor Color = FLinearColor::White();
	float Rotation = 0.0f;
	float RelativeTime = 0.0f;
	float Age = 0.0f;
	float SubImageIndex = 0.0f;
	float CameraDistanceSq = 0.0f;
	float ViewDepth = 0.0f;
	uint32 RibbonSpawnSerial = 0;
	uint32 RibbonSourceParticleId = 0;
	int32 RibbonTrailIndex = 0;
};

struct FParticleMeshRenderBatch
{
	UMaterial* Material = nullptr;
	FString MeshPath;
	EParticleBlendMode BlendMode = EParticleBlendMode::AlphaBlend;
	TArray<FMeshParticleInstanceVertex> Instances;
};

enum class EParticleRenderPacketType : uint8
{
	CpuExpandedSprite,
	CpuExpandedMesh,
	CpuExpandedBeam,
	CpuExpandedRibbon,
	InstancedMesh,
};

struct FParticleRenderPacket
{
	EDynamicEmitterType EmitterType = EDynamicEmitterType::Unknown;
	EParticleRenderPacketType PacketType = EParticleRenderPacketType::CpuExpandedSprite;

	UMaterial* Material = nullptr;
	UStaticMesh* Mesh = nullptr;
	FString MeshPath;
	EParticleBlendMode BlendMode = EParticleBlendMode::AlphaBlend;

	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
	uint32 BaseVertex = 0;

	uint32 FirstInstance = 0;
	uint32 InstanceCount = 0;

	float SortDepth = 0.0f;
	int32 TranslucencySortPriority = 0;
	bool bHasTranslucencySort = false;

	bool HasIndexRange() const { return IndexCount > 0; }
};
