#include "ParticleEmitterInstance.h"

#include "Component/Primitive/ParticleSystemComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleHelper.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/ParticleModuleEvent.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
	EParticleEmitterType ResolveEmitterType(const UParticleLODLevel* LODLevel)
	{
		if (!LODLevel)
		{
			return EParticleEmitterType::Sprite;
		}

		if (const UParticleModuleTypeDataBase* TypeData = LODLevel->GetTypeDataModule())
		{
			return TypeData->GetEmitterType();
		}
		return EParticleEmitterType::Sprite;
	}

	EParticleEmitterType ResolveEmitterType(const UParticleEmitter* EmitterTemplate)
	{
		if (!EmitterTemplate)
		{
			return EParticleEmitterType::Sprite;
		}

		return ResolveEmitterType(EmitterTemplate->GetLODLevelForIndex(0));
	}

	FVector ResolveModulePoint(const FParticleEmitterInstance* Instance, const FVector& Point, bool bAbsolute)
	{
		if (bAbsolute || !Instance || !Instance->Component)
		{
			return Point;
		}
		return Instance->Component->GetWorldMatrix().TransformPositionWithW(Point);
	}

	FVector ResolveModuleVector(const FParticleEmitterInstance* Instance, const FVector& Vector, bool bAbsolute)
	{
		if (bAbsolute || !Instance || !Instance->Component)
		{
			return Vector;
		}
		return Instance->Component->GetWorldMatrix().TransformVector(Vector);
	}
}

const FParticleModuleCache* FParticleLODLevelCompiledData::FindModuleCache(const UParticleModule* Module) const
{
	for (const FParticleModuleRuntimeCache& RuntimeCache : ModuleCaches)
	{
		if (RuntimeCache.Module == Module)
		{
			return &RuntimeCache.Cache;
		}
	}
	return nullptr;
}

FParticleEmitterInstance* FParticleEmitterInstance::Create(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate)
{
	FParticleEmitterInstance* Instance = nullptr;
	switch (ResolveEmitterType(InTemplate))
	{
	case EParticleEmitterType::Mesh:
		Instance = new FParticleMeshEmitterInstance();
		break;
	case EParticleEmitterType::Beam:
		Instance = new FParticleBeamEmitterInstance();
		break;
	case EParticleEmitterType::Ribbon:
		Instance = new FParticleRibbonEmitterInstance();
		break;
	case EParticleEmitterType::Sprite:
	default:
		Instance = new FParticleSpriteEmitterInstance();
		break;
	}

	Instance->Init(InComponent, InTemplate);
	return Instance;
}

void FParticleEmitterInstance::Init(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate)
{
	Component = InComponent;
	EmitterTemplate = InTemplate;
	EmitterTime = 0.0f;
	SpawnFraction = 0.0f;
	bBurstFired = false;
	ActiveParticles = 0;
	ParticleCounter = 0;
	EventCounter = 0;
	LastProcessedParticleEventCount = 0;
	LastEventReceiverSpawnRequestCount = 0;
	LastEventReceiverSpawnedCount = 0;
	bSpawningSuppressed = false;
	PendingEvents.clear();
	SetCurrentLODIndex(0);
}

void FParticleEmitterInstance::Reset()
{
	CurrentLODLevelIndex = -1;
	CurrentLODLevel = nullptr;
	LODData = {};
	DataContainer.Free();
	ParticleData = nullptr;
	ParticleIndices = nullptr;
	InstanceDataStorage.clear();
	InstanceData = nullptr;
	InstancePayloadSize = 0;
	PayloadOffset = 0;
	IntrinsicPayloadOffset = -1;
	ParticleSize = sizeof(FBaseParticle);
	ParticleStride = FMath::AlignBytes(ParticleSize, 16);
	ActiveParticles = 0;
	MaxActiveParticles = 0;
	ParticleCounter = 0;
	EventCounter = 0;
	LastProcessedParticleEventCount = 0;
	LastEventReceiverSpawnRequestCount = 0;
	LastEventReceiverSpawnedCount = 0;
	EmitterTime = 0.0f;
	SpawnFraction = 0.0f;
	bBurstFired = false;
	bSpawningSuppressed = false;
	PendingEvents.clear();
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	if (!EmitterTemplate || !CurrentLODLevel || DeltaTime <= 0.0f)
	{
		return;
	}

	UParticleModuleSpawn* SpawnRateModule = GetSpawnRateModule();
	const UParticleModuleRequired* RequiredModule = CurrentLODLevel->GetRequiredModule();
	const float PreviousEmitterTime = EmitterTime;
	const float EmitterDuration = RequiredModule ? (std::max)(0.0f, RequiredModule->EmitterDuration) : 0.0f;
	const bool bEmitterLooping = RequiredModule ? RequiredModule->bLooping : true;
	float SpawnDeltaTime = DeltaTime;
	EmitterTime += DeltaTime;
	bool bEmitterWrapped = false;

	if (EmitterDuration > FMath::Epsilon)
	{
		if (bEmitterLooping)
		{
			if (EmitterTime >= EmitterDuration)
			{
				EmitterTime = std::fmod(EmitterTime, EmitterDuration);
				if (EmitterTime < 0.0f)
				{
					EmitterTime += EmitterDuration;
				}

				bEmitterWrapped = true;
				bBurstFired = false;
			}
		}
		else
		{
			if (PreviousEmitterTime >= EmitterDuration)
			{
				SpawnDeltaTime = 0.0f;
			}
			else if (EmitterTime > EmitterDuration)
			{
				SpawnDeltaTime = (std::max)(0.0f, EmitterDuration - PreviousEmitterTime);
				EmitterTime = EmitterDuration;
			}
		}
	}

	ClearPendingEvents();

	if (SpawnRateModule && !bSpawningSuppressed && SpawnDeltaTime > 0.0f)
	{
		FVector InitialLocation = FVector::ZeroVector;
		if (RequiredModule)
		{
			if (!RequiredModule->bUseLocalSpace)
			{
				InitialLocation = GetSpawnReferenceLocation();
			}
		}

		UObject* DistributionData = GetDistributionData();

		// 버스트
		const int32 BurstCount = std::max(0, static_cast<int32>(std::floor(SpawnRateModule->BurstCount.GetValue(SpawnRateModule->BurstTime, DistributionData) + 0.5f)));
		const bool bCrossedBurstTime = bEmitterWrapped
			? (SpawnRateModule->BurstTime >= PreviousEmitterTime || SpawnRateModule->BurstTime <= EmitterTime)
			: (PreviousEmitterTime <= SpawnRateModule->BurstTime && EmitterTime >= SpawnRateModule->BurstTime);
		if (!bBurstFired && BurstCount > 0 && bCrossedBurstTime)
		{
			NotifyParticleBurst(BurstCount, InitialLocation);
			SpawnParticles(BurstCount, SpawnRateModule->BurstTime, 0.0f, InitialLocation, FVector::ZeroVector);
			bBurstFired = true;
		}

		// 초당 얼마나 생성할지 따져서 현재 프레임에 생성할 파티클의 개수 결정
		// 소수점의 경우가 있을 수 있고 파티클은 소수점으로 못나타내서 다음 프레임으로 누적
		const float Rate = SpawnRateModule->Rate.GetValue(EmitterTime, DistributionData);
		const float RateScale = SpawnRateModule->RateScale.GetValue(EmitterTime, DistributionData);
		const float SpawnRate = std::max(0.0f, Rate * RateScale);
		SpawnFraction += SpawnRate * SpawnDeltaTime;
		const int32 SpawnCount = static_cast<int32>(SpawnFraction);
		SpawnFraction -= static_cast<float>(SpawnCount);

		if (SpawnCount > 0)
		{
			const float SpawnIncrement = SpawnDeltaTime / static_cast<float>(SpawnCount);
			SpawnParticles(SpawnCount, PreviousEmitterTime, SpawnIncrement, InitialLocation, FVector::ZeroVector);
		}
	}

	UpdateParticleLifetimesAndMovement(DeltaTime);

	for (UParticleModule* Module : CurrentLODLevel->GetUpdateModules())
	{
		if (!Module)
		{
			continue;
		}

		const FParticleModuleCache* Cache = LODData.FindModuleCache(Module);
		const int32 Offset = Cache ? Cache->ParticlePayloadOffset : 0;
		UParticleModule::FUpdateContext Context(*this, Offset, DeltaTime);
		Module->Update(Context);
	}
}

void FParticleEmitterInstance::ProcessParticleEvents(const TArray<FParticleEventData>& Events)
{
	LastProcessedParticleEventCount = 0;
	LastEventReceiverSpawnRequestCount = 0;
	LastEventReceiverSpawnedCount = 0;

	if (!CurrentLODLevel || Events.empty())
	{
		return;
	}

	for (UParticleModule* Module : CurrentLODLevel->GetEventReceiverModules())
	{
		UParticleModuleEventReceiverBase* Receiver = Cast<UParticleModuleEventReceiverBase>(Module);
		if (!Receiver || !Receiver->IsEnabled())
		{
			continue;
		}

		for (const FParticleEventData& EventData : Events)
		{
			if (Receiver->WillProcessParticleEvent(EventData))
			{
				++LastProcessedParticleEventCount;
				Receiver->ProcessParticleEvent(EventData, *this);
			}
		}
	}
}

bool FParticleEmitterInstance::SetCurrentLODIndex(int32 InLODIndex)
{
	if (!EmitterTemplate)
	{
		return false;
	}

	UParticleLODLevel* NewLODLevel = EmitterTemplate->GetLODLevelForIndex(InLODIndex);
	if (!NewLODLevel)
	{
		Reset();
		EmitterTemplate = nullptr;
		return false;
	}

	const int32 NewLODIndex = NewLODLevel->GetLevel();
	if (CurrentLODLevel == NewLODLevel && CurrentLODLevelIndex == NewLODIndex)
	{
		return true;
	}

	CurrentLODLevel = NewLODLevel;
	CurrentLODLevelIndex = NewLODIndex;

	// LOD에 따른 모듈 별 메모리 배치 정보 빌드
	BuildLODData(CurrentLODLevel);

	// 배치된 메모리로 실제 allocate
	AllocateParticleDataPreservingBaseParticles();
	return true;
}

void FParticleEmitterInstance::BuildLODData(UParticleLODLevel* LODLevel)
{
	LODData = {};
	if (!LODLevel)
	{
		return;
	}

	LODLevel->RebuildModuleLists();
	LODData.ParticleSize = sizeof(FBaseParticle);
	LODData.ParticleStride = FMath::AlignBytes(LODData.ParticleSize, 16);
	LODData.InstancePayloadSize = 0;
	ParticleSize = LODData.ParticleSize;
	ParticleStride = LODData.ParticleStride;
	PayloadOffset = ParticleSize;
	InstancePayloadSize = 0;
	IntrinsicPayloadOffset = -1;

	const int32 IntrinsicPayloadSize = GetIntrinsicParticlePayloadSize();
	if (IntrinsicPayloadSize > 0)
	{
		LODData.ParticleSize = FMath::AlignBytes(LODData.ParticleSize, 16);
		IntrinsicPayloadOffset = LODData.ParticleSize;
		LODData.ParticleSize += IntrinsicPayloadSize;
	}

	for (UParticleModule* Module : LODLevel->GetModules())
	{
		if (!Module || !Module->IsEnabled())
		{
			continue;
		}

		FParticleModuleRuntimeCache RuntimeCache;
		RuntimeCache.Module = Module;
		RuntimeCache.Cache.ParticlePayloadSize = Module->GetParticlePayloadSize();

		// ParticlePayLoadOffset -> 현재 시작한 사이즈에서 파티클 사이즈 데이터 정렬
		// ParticleSize -> 뒷부분 패딩을 제외한 데이터 크기
		if (RuntimeCache.Cache.ParticlePayloadSize > 0)
		{
			LODData.ParticleSize = FMath::AlignBytes(LODData.ParticleSize, 16);
			RuntimeCache.Cache.ParticlePayloadOffset = LODData.ParticleSize;
			LODData.ParticleSize += RuntimeCache.Cache.ParticlePayloadSize;
		}

		RuntimeCache.Cache.InstancePayloadOffset = LODData.InstancePayloadSize;
		RuntimeCache.Cache.InstancePayloadSize = Module->GetInstancePayloadSize();
		LODData.InstancePayloadSize += FMath::AlignBytes(RuntimeCache.Cache.InstancePayloadSize, 16);

		LODData.ModuleCaches.push_back(RuntimeCache);
	}

	LODData.ParticleStride = FMath::AlignBytes(LODData.ParticleSize, 16);
	ParticleSize = LODData.ParticleSize;
	ParticleStride = LODData.ParticleStride;
	PayloadOffset = ParticleSize;
	InstancePayloadSize = LODData.InstancePayloadSize;
}

FDynamicEmitterDataBase* FParticleEmitterInstance::CreateDynamicData(int32 EmitterIndex) const
{
	if (!CurrentLODLevel)
	{
		return nullptr;
	}

	const UParticleModuleRequired* RequiredModule = CurrentLODLevel->GetRequiredModule();
	FDynamicEmitterDataBase* DynamicData = CreateDynamicEmitterData(RequiredModule);
	if (!DynamicData)
	{
		return nullptr;
	}

	DynamicData->EmitterIndex = EmitterIndex;

	FDynamicEmitterReplayDataBase& Source = DynamicData->GetSource();
	Source.EmitterType = GetDynamicEmitterType();
	Source.ActiveParticleCount = ActiveParticles;
	Source.ParticleStride = ParticleStride;
	Source.EmitterTime = EmitterTime;
	Source.Scale = Component ? Component->GetWorldScale() : FVector::OneVector;

	if (Source.EmitterType == EDynamicEmitterType::Sprite || Source.EmitterType == EDynamicEmitterType::Mesh)
	{
		FDynamicSpriteEmitterReplayDataBase& SpriteSource = static_cast<FDynamicSpriteEmitterDataBase*>(DynamicData)->GetSpriteSource();
		if (const UParticleModuleTypeDataSprite* SpriteTypeData = Cast<UParticleModuleTypeDataSprite>(CurrentLODLevel->GetTypeDataModule()))
		{
			SpriteSource.bUseSubUV = SpriteTypeData->bUseSubUV;
			SpriteSource.SubUVResourceName = SpriteTypeData->SubUVResourceName.ToString();
			SpriteSource.SubImagesX = std::max(1, SpriteTypeData->SubImagesX);
			SpriteSource.SubImagesY = std::max(1, SpriteTypeData->SubImagesY);
			SpriteSource.SubUVFrameRate = std::max(0.0f, SpriteTypeData->SubUVFrameRate);
			SpriteSource.bLoopSubUV = SpriteTypeData->bLoopSubUV;
		}

		for (UParticleModule* Module : CurrentLODLevel->GetModules())
		{
			if (!Module || !Module->IsEnabled())
			{
				continue;
			}

			if (const UParticleModuleSubUV* SubUVModule = Cast<UParticleModuleSubUV>(Module))
			{
				SpriteSource.bUseSubUV = true;
				SpriteSource.SubUVResourceName = SubUVModule->SubUVResourceName.ToString();
				SpriteSource.SubImagesX = std::max(1, SubUVModule->SubImagesX);
				SpriteSource.SubImagesY = std::max(1, SubUVModule->SubImagesY);
				SpriteSource.SubUVFrameRate = 0.0f;
				SpriteSource.bLoopSubUV = false;
				if (const FParticleModuleCache* Cache = LODData.FindModuleCache(SubUVModule))
				{
					SpriteSource.SubUVPayloadOffset = Cache->ParticlePayloadOffset;
				}
				break;
			}
		}

		if (const UParticleModuleTypeDataMesh* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(CurrentLODLevel->GetTypeDataModule()))
		{
			SpriteSource.MeshPath = MeshTypeData->MeshPath.ToString();
		}
	}
	else if (Source.EmitterType == EDynamicEmitterType::Beam)
	{
		FDynamicBeamEmitterReplayData& BeamSource = static_cast<FDynamicBeamEmitterData*>(DynamicData)->GetBeamSource();
		const FVector ComponentLocation = Component ? Component->GetWorldLocation() : FVector::ZeroVector;
		BeamSource.SourcePoint = ComponentLocation;
		BeamSource.TargetPoint = ComponentLocation + FVector(0.0f, 0.0f, 100.0f);
		if (const UParticleModuleTypeDataBeam* BeamTypeData = Cast<UParticleModuleTypeDataBeam>(CurrentLODLevel->GetTypeDataModule()))
		{
			BeamSource.MaxBeamCount = std::max(1, BeamTypeData->MaxBeamCount);
			BeamSource.Sheets = std::max(1, BeamTypeData->Sheets);
			BeamSource.InterpolationPoints = std::max(1, BeamTypeData->InterpolationPoints);
		}
		for (UParticleModule* Module : CurrentLODLevel->GetModules())
		{
			if (!Module || !Module->IsEnabled())
			{
				continue;
			}

			if (const UParticleModuleBeamSource* SourceModule = Cast<UParticleModuleBeamSource>(Module))
			{
				BeamSource.SourceMethod = SourceModule->SourceMethod;
				BeamSource.SourcePoint = SourceModule->SourceMethod == EParticleBeamEndpointMethod::Emitter
					? ComponentLocation
					: ResolveModulePoint(this, SourceModule->SourcePoint, SourceModule->bSourceAbsolute);
				BeamSource.SourceTangent = ResolveModuleVector(this, SourceModule->SourceTangent, SourceModule->bSourceAbsolute);
				BeamSource.bUseSourceTangent = SourceModule->bUseSourceTangent;
			}
			else if (const UParticleModuleBeamTarget* TargetModule = Cast<UParticleModuleBeamTarget>(Module))
			{
				BeamSource.TargetMethod = TargetModule->TargetMethod;
				BeamSource.TargetPoint = TargetModule->TargetMethod == EParticleBeamEndpointMethod::Emitter
					? ComponentLocation
					: ResolveModulePoint(this, TargetModule->TargetPoint, TargetModule->bTargetAbsolute);
				BeamSource.TargetTangent = ResolveModuleVector(this, TargetModule->TargetTangent, TargetModule->bTargetAbsolute);
				BeamSource.bUseTargetTangent = TargetModule->bUseTargetTangent;
			}
			else if (const UParticleModuleBeamNoise* NoiseModule = Cast<UParticleModuleBeamNoise>(Module))
			{
				BeamSource.NoiseFrequency = std::max(0, NoiseModule->Frequency);
				BeamSource.NoiseStrength = std::max(0.0f, NoiseModule->Strength);
				BeamSource.NoiseSpeed = std::max(0.0f, NoiseModule->Speed);
				if (const FParticleModuleCache* Cache = LODData.FindModuleCache(NoiseModule))
				{
					BeamSource.NoisePayloadOffset = Cache->ParticlePayloadOffset;
					BeamSource.NoisePayloadSize = Cache->ParticlePayloadSize;
				}
			}
		}
	}
	else if (Source.EmitterType == EDynamicEmitterType::Ribbon)
	{
		FDynamicRibbonEmitterReplayData& RibbonSource = static_cast<FDynamicRibbonEmitterData*>(DynamicData)->GetRibbonSource();
		if (const UParticleModuleTypeDataRibbon* RibbonTypeData = Cast<UParticleModuleTypeDataRibbon>(CurrentLODLevel->GetTypeDataModule()))
		{
			RibbonSource.TrailCount = std::max(1, RibbonTypeData->MaxTrailCount);
			RibbonSource.MaxParticlesInTrail = std::max(2, RibbonTypeData->MaxParticlesInTrail);
			RibbonSource.Sheets = std::max(1, RibbonTypeData->SheetsPerTrail);
			RibbonSource.TilingDistance = std::max(0.0f, RibbonTypeData->TilingDistance);
			RibbonSource.RenderAxis = RibbonTypeData->RenderAxis;
		}
		RibbonSource.MaxActiveParticleCount = MaxActiveParticles;
		RibbonSource.RibbonPayloadOffset = IntrinsicPayloadOffset;
	}

	const int32 ParticleDataBytes = MaxActiveParticles * ParticleStride;
	Source.DataContainer.CopyFrom(ParticleData, ParticleDataBytes, ParticleIndices, ActiveParticles);

	return DynamicData;
}

FDynamicEmitterDataBase* FParticleEmitterInstance::CreateDynamicEmitterData(const UParticleModuleRequired* RequiredModule) const
{
	return new FDynamicSpriteEmitterData(RequiredModule);
}

const FTransform& FParticleEmitterInstance::GetComponentTransform() const
{
	static const FTransform IdentityTransform;
	return Component ? Component->GetComponentTransformForParticles() : IdentityTransform;
}

FVector FParticleEmitterInstance::GetComponentLocation() const
{
	return Component ? Component->GetWorldLocation() : FVector::ZeroVector;
}

FVector FParticleEmitterInstance::GetSpawnReferenceLocation() const
{
	return GetComponentLocation();
}

FVector FParticleEmitterInstance::ConvertWorldLocationToParticleSpace(const FVector& WorldLocation) const
{
	const UParticleModuleRequired* Required = CurrentLODLevel ? CurrentLODLevel->GetRequiredModule() : nullptr;
	if (Required && Required->bUseLocalSpace && Component)
	{
		return Component->GetWorldInverseMatrix().TransformPositionWithW(WorldLocation);
	}
	return WorldLocation;
}

uint8* FParticleEmitterInstance::GetModuleInstanceData(const UParticleModule* Module)
{
	return const_cast<uint8*>(static_cast<const FParticleEmitterInstance*>(this)->GetModuleInstanceData(Module));
}

const uint8* FParticleEmitterInstance::GetModuleInstanceData(const UParticleModule* Module) const
{
	if (!Module || !InstanceData || InstancePayloadSize <= 0)
	{
		return nullptr;
	}

	const FParticleModuleCache* Cache = LODData.FindModuleCache(Module);
	if (!Cache || Cache->InstancePayloadSize <= 0 || Cache->InstancePayloadOffset < 0)
	{
		return nullptr;
	}

	const int32 PayloadEnd = Cache->InstancePayloadOffset + Cache->InstancePayloadSize;
	if (PayloadEnd > InstancePayloadSize)
	{
		return nullptr;
	}

	return InstanceData + Cache->InstancePayloadOffset;
}

UObject* FParticleEmitterInstance::GetDistributionData() const
{
	return Component;
}

FString FParticleEmitterInstance::GetTemplateName() const
{
	return EmitterTemplate ? EmitterTemplate->GetEmitterName().ToString() : FString();
}

FString FParticleEmitterInstance::GetInstanceName() const
{
	return Component ? Component->GetInstanceNameForParticles() : FString();
}

UParticleModuleSpawn* FParticleEmitterInstance::GetSpawnRateModule() const
{
	if (!CurrentLODLevel)
	{
		return nullptr;
	}

	for (UParticleModule* Module : CurrentLODLevel->GetModules())
	{
		if (UParticleModuleSpawn* SpawnModule = Cast<UParticleModuleSpawn>(Module))
		{
			return SpawnModule->IsEnabled() ? SpawnModule : nullptr;
		}
	}
	return nullptr;
}

FBaseParticle* FParticleEmitterInstance::GetParticleDirect(int32 ParticleIndex)
{
	if (ParticleIndex < 0 || ParticleIndex >= MaxActiveParticles || !ParticleData)
	{
		return nullptr;
	}
	return reinterpret_cast<FBaseParticle*>(ParticleData + static_cast<size_t>(ParticleIndex) * ParticleStride);
}

const FBaseParticle* FParticleEmitterInstance::GetParticleDirect(int32 ParticleIndex) const
{
	if (ParticleIndex < 0 || ParticleIndex >= MaxActiveParticles || !ParticleData)
	{
		return nullptr;
	}
	return reinterpret_cast<const FBaseParticle*>(ParticleData + static_cast<size_t>(ParticleIndex) * ParticleStride);
}

FBaseParticle* FParticleEmitterInstance::GetActiveParticle(int32 ActiveIndex)
{
	if (ActiveIndex < 0 || ActiveIndex >= ActiveParticles || !ParticleIndices)
	{
		return nullptr;
	}
	return GetParticleDirect(ParticleIndices[ActiveIndex]);
}

const FBaseParticle* FParticleEmitterInstance::GetActiveParticle(int32 ActiveIndex) const
{
	if (ActiveIndex < 0 || ActiveIndex >= ActiveParticles || !ParticleIndices)
	{
		return nullptr;
	}
	return GetParticleDirect(ParticleIndices[ActiveIndex]);
}

void FParticleEmitterInstance::AllocateParticleDataPreservingBaseParticles()
{
	TArray<FBaseParticle> ExistingParticles;
	ExistingParticles.reserve(static_cast<size_t>(ActiveParticles));
	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		if (const FBaseParticle* Particle = GetActiveParticle(Index))
		{
			ExistingParticles.push_back(*Particle);
		}
	}

	MaxActiveParticles = 0;
	if (CurrentLODLevel)
	{
		if (const UParticleModuleRequired* RequiredModule = CurrentLODLevel->GetRequiredModule())
		{
			MaxActiveParticles = std::min(std::max(0, RequiredModule->MaxParticles), 65535);
		}
	}

	const int32 ParticleDataBytes = std::max(0, MaxActiveParticles) * std::max(0, ParticleStride);
	DataContainer.Alloc(ParticleDataBytes, MaxActiveParticles);
	ParticleData = DataContainer.ParticleData;
	ParticleIndices = DataContainer.ParticleIndices;
	InstanceDataStorage.assign(static_cast<size_t>(std::max(0, InstancePayloadSize)), 0);
	InstanceData = InstanceDataStorage.empty() ? nullptr : InstanceDataStorage.data();
	InitializeParticleIndices();

	ActiveParticles = 0;
	const int32 PreserveCount = std::min(static_cast<int32>(ExistingParticles.size()), MaxActiveParticles);
	for (int32 Index = 0; Index < PreserveCount; ++Index)
	{
		if (FBaseParticle* Particle = GetParticleDirect(Index))
		{
			*Particle = ExistingParticles[Index];
			ParticleIndices[Index] = static_cast<uint16>(Index);
			++ActiveParticles;
		}
	}
}

void FParticleEmitterInstance::InitializeParticleIndices()
{
	if (!ParticleIndices)
	{
		return;
	}

	for (int32 Index = 0; Index < MaxActiveParticles; ++Index)
	{
		ParticleIndices[Index] = static_cast<uint16>(Index);
	}
}

void FParticleEmitterInstance::SpawnParticles(
	int32 Count,
	float StartTime,
	float Increment,
	const FVector& InitialLocation,
	const FVector& InitialVelocity,
	bool bLockInitialLocation)
{
	if (!CurrentLODLevel || !ParticleData || !ParticleIndices || Count <= 0 || bSpawningSuppressed)
	{
		return;
	}

	for (int32 SpawnIndex = 0; SpawnIndex < Count; ++SpawnIndex)
	{
		if (ActiveParticles >= MaxActiveParticles)
		{
			return;
		}

		const int32 ParticleIndex = ParticleIndices[ActiveParticles];
		uint8* ParticleBaseAddress = ParticleData + static_cast<size_t>(ParticleIndex) * ParticleStride;
		DECLARE_PARTICLE_PTR(Particle, ParticleBaseAddress);
		if (!Particle)
		{
			return;
		}

		const float SpawnTime = StartTime + Increment * static_cast<float>(SpawnIndex);
		PreSpawn(Particle, InitialLocation, InitialVelocity);

		for (UParticleModule* Module : CurrentLODLevel->GetSpawnModules())
		{
			if (!Module)
			{
				continue;
			}

			const FParticleModuleCache* Cache = LODData.FindModuleCache(Module);
			const int32 Offset = Cache ? Cache->ParticlePayloadOffset : 0;
			UParticleModule::FSpawnContext Context(*this, Offset, SpawnTime, Particle, bLockInitialLocation);
			Module->Spawn(Context);
		}

		const float Interp = Increment > 0.0f ? static_cast<float>(SpawnIndex) / static_cast<float>(Count) : 0.0f;
		PostSpawn(Particle, Interp, SpawnTime);
		++ActiveParticles;
	}
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	if (!Particle)
	{
		return;
	}

	std::memset(Particle, 0, static_cast<size_t>(ParticleSize));
	*Particle = FBaseParticle();
	Particle->Flags |= STATE_Particle_JustSpawned;
	Particle->Location = InitialLocation;
	Particle->OldLocation = InitialLocation;
	Particle->Velocity = InitialVelocity;
	Particle->BaseVelocity = InitialVelocity;
	Particle->ParticleId = ++ParticleCounter;
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime)
{
	(void)Interp;
	if (!Particle)
	{
		return;
	}

	const float SpawnAge = std::max(0.0f, EmitterTime - SpawnTime);
	Particle->RelativeTime += SpawnAge * Particle->OneOverMaxLifetime;
	Particle->OldLocation = Particle->Location - Particle->Velocity * SpawnAge;
	Particle->Flags &= ~STATE_Particle_JustSpawned;
}

void FParticleEmitterInstance::KillParticle(int32 ActiveIndex)
{
	if (ActiveIndex < 0 || ActiveIndex >= ActiveParticles)
	{
		return;
	}

	NotifyParticleKilled(GetActiveParticle(ActiveIndex));

	const uint16 CurrentIndex = ParticleIndices[ActiveIndex];
	ParticleIndices[ActiveIndex] = ParticleIndices[ActiveParticles - 1];
	ParticleIndices[ActiveParticles - 1] = CurrentIndex;
	ActiveParticles--;
}

void FParticleEmitterInstance::KillAllParticles()
{
	while (ActiveParticles > 0)
	{
		KillParticle(ActiveParticles - 1);
	}
}

void FParticleEmitterInstance::QueueParticleEvent(const FParticleEventData& EventData)
{
	PendingEvents.push_back(EventData);
	if (Component)
	{
		Component->ReportParticleEvent(EventData);
	}
}

void FParticleEmitterInstance::ClearPendingEvents()
{
	PendingEvents.clear();
}

void FParticleEmitterInstance::RecordEventReceiverSpawn(int32 RequestedCount, int32 SpawnedCount)
{
	LastEventReceiverSpawnRequestCount += std::max(0, RequestedCount);
	LastEventReceiverSpawnedCount += std::max(0, SpawnedCount);
}

void FParticleEmitterInstance::NotifyParticleKilled(const FBaseParticle* Particle)
{
	if (!CurrentLODLevel || !Particle)
	{
		return;
	}

	for (UParticleModule* Module : CurrentLODLevel->GetEventGeneratorModules())
	{
		if (UParticleModuleEventGenerator* Generator = Cast<UParticleModuleEventGenerator>(Module))
		{
			Generator->HandleParticleKilled(*this, Particle);
		}
	}
}

void FParticleEmitterInstance::NotifyParticleBurst(int32 ParticleCount, const FVector& Location)
{
	if (!CurrentLODLevel || ParticleCount <= 0)
	{
		return;
	}

	for (UParticleModule* Module : CurrentLODLevel->GetEventGeneratorModules())
	{
		if (UParticleModuleEventGenerator* Generator = Cast<UParticleModuleEventGenerator>(Module))
		{
			Generator->HandleParticleBurst(*this, ParticleCount, Location);
		}
	}
}

void FParticleEmitterInstance::UpdateParticleLifetimesAndMovement(float DeltaTime)
{
	for (int32 ActiveIndex = ActiveParticles - 1; ActiveIndex >= 0; --ActiveIndex)
	{
		FBaseParticle* Particle = GetActiveParticle(ActiveIndex);
		if (!Particle)
		{
			continue;
		}

		if ((Particle->Flags & STATE_Particle_Freeze) != 0)
		{
			continue;
		}

		Particle->RelativeTime += DeltaTime * Particle->OneOverMaxLifetime;
		if (Particle->RelativeTime >= 1.0f)
		{
			KillParticle(ActiveIndex);
			continue;
		}

		Particle->OldLocation = Particle->Location;
		Particle->Velocity = Particle->BaseVelocity;
		if ((Particle->Flags & STATE_Particle_FreezeTranslation) == 0)
		{
			Particle->Location += Particle->Velocity * DeltaTime;
		}
		Particle->RotationRate = Particle->BaseRotationRate;
		if ((Particle->Flags & STATE_Particle_FreezeRotation) == 0)
		{
			Particle->Rotation += Particle->RotationRate * DeltaTime;
		}
	}
}

FDynamicEmitterDataBase* FParticleMeshEmitterInstance::CreateDynamicEmitterData(const UParticleModuleRequired* RequiredModule) const
{
	return new FDynamicMeshEmitterData(RequiredModule);
}

FDynamicEmitterDataBase* FParticleBeamEmitterInstance::CreateDynamicEmitterData(const UParticleModuleRequired* RequiredModule) const
{
	return new FDynamicBeamEmitterData(RequiredModule);
}

FDynamicEmitterDataBase* FParticleRibbonEmitterInstance::CreateDynamicEmitterData(const UParticleModuleRequired* RequiredModule) const
{
	return new FDynamicRibbonEmitterData(RequiredModule);
}

void FParticleRibbonEmitterInstance::Init(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate)
{
	CurrentSourceLocation = FVector::ZeroVector;
	bHasCurrentSource = false;
	PendingTrailIndex = 0;
	PendingSourceParticleId = 0;
	RibbonSpawnSerial = 0;
	bSingleSourceInitialized = false;
	SourceTrails.clear();
	FParticleEmitterInstance::Init(InComponent, InTemplate);
}

void FParticleRibbonEmitterInstance::Reset()
{
	CurrentSourceLocation = FVector::ZeroVector;
	bHasCurrentSource = false;
	PendingTrailIndex = 0;
	PendingSourceParticleId = 0;
	RibbonSpawnSerial = 0;
	bSingleSourceInitialized = false;
	SourceTrails.clear();
	FParticleEmitterInstance::Reset();
}

const UParticleModuleTrailSource* FParticleRibbonEmitterInstance::FindTrailSourceModule() const
{
	if (!CurrentLODLevel)
	{
		return nullptr;
	}

	for (UParticleModule* Module : CurrentLODLevel->GetModules())
	{
		if (const UParticleModuleTrailSource* Source = Cast<UParticleModuleTrailSource>(Module))
		{
			return Source->IsEnabled() ? Source : nullptr;
		}
	}
	return nullptr;
}

const UParticleModuleSpawnPerUnit* FParticleRibbonEmitterInstance::FindSpawnPerUnitModule() const
{
	if (!CurrentLODLevel)
	{
		return nullptr;
	}

	for (UParticleModule* Module : CurrentLODLevel->GetModules())
	{
		if (const UParticleModuleSpawnPerUnit* SpawnPerUnit = Cast<UParticleModuleSpawnPerUnit>(Module))
		{
			return SpawnPerUnit->IsEnabled() ? SpawnPerUnit : nullptr;
		}
	}
	return nullptr;
}

bool FParticleRibbonEmitterInstance::ResolveSingleSourceLocation(FVector& OutLocation) const
{
	const UParticleModuleTrailSource* Source = FindTrailSourceModule();
	if (!Source || Source->SourceMethod == ETrail2SourceMethod::Default)
	{
		OutLocation = GetComponentLocation() + (Source ? Source->SourceOffset : FVector::ZeroVector);
		return true;
	}
	if (Source->SourceMethod != ETrail2SourceMethod::Actor || !Component)
	{
		return false;
	}

	UObject* ObjectSource = nullptr;
	if (!Component->GetObjectParameter(Source->SourceName, ObjectSource))
	{
		return false;
	}
	if (const USceneComponent* SourceComponent = Cast<USceneComponent>(ObjectSource))
	{
		OutLocation = SourceComponent->GetWorldLocation() + Source->SourceOffset;
		return true;
	}
	if (const AActor* SourceActor = Cast<AActor>(ObjectSource))
	{
		OutLocation = SourceActor->GetActorLocation() + Source->SourceOffset;
		return true;
	}
	return false;
}

FVector FParticleRibbonEmitterInstance::GetSpawnReferenceLocation() const
{
	return bHasCurrentSource ? CurrentSourceLocation : GetComponentLocation();
}

void FParticleRibbonEmitterInstance::Tick(float DeltaTime)
{
	const UParticleModuleTrailSource* Source = FindTrailSourceModule();
	if (Source && Source->SourceMethod == ETrail2SourceMethod::Particle)
	{
		const bool bWasSuppressed = bSpawningSuppressed;
		bSpawningSuppressed = true;
		FParticleEmitterInstance::Tick(DeltaTime);
		bSpawningSuppressed = bWasSuppressed;
		UpdateParticleSourceTrails();
		return;
	}

	bHasCurrentSource = ResolveSingleSourceLocation(CurrentSourceLocation);
	const bool bWasSuppressed = bSpawningSuppressed;
	if (!bHasCurrentSource)
	{
		bSpawningSuppressed = true;
	}
	FParticleEmitterInstance::Tick(DeltaTime);
	bSpawningSuppressed = bWasSuppressed;
	if (bHasCurrentSource)
	{
		const UParticleModuleTypeDataRibbon* TypeData = CurrentLODLevel
			? Cast<UParticleModuleTypeDataRibbon>(CurrentLODLevel->GetTypeDataModule())
			: nullptr;
		if (!bSingleSourceInitialized && FindSpawnPerUnitModule()
			&& (!TypeData || TypeData->bSpawnInitialParticle))
		{
			SpawnTrailPoint(0, 0, CurrentSourceLocation);
		}
		bSingleSourceInitialized = true;
		TrimTrail(0);
	}
	else
	{
		bSingleSourceInitialized = false;
	}
}

void FParticleRibbonEmitterInstance::PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime)
{
	FParticleEmitterInstance::PostSpawn(Particle, Interp, SpawnTime);
	if (FRibbonParticlePayload* Payload = GetRibbonPayload(Particle))
	{
		Payload->SpawnSerial = ++RibbonSpawnSerial;
		Payload->SourceParticleId = PendingSourceParticleId;
		Payload->TrailIndex = PendingTrailIndex;
	}
}

FRibbonParticlePayload* FParticleRibbonEmitterInstance::GetRibbonPayload(FBaseParticle* Particle) const
{
	if (!Particle || IntrinsicPayloadOffset < 0)
	{
		return nullptr;
	}
	return reinterpret_cast<FRibbonParticlePayload*>(
		reinterpret_cast<uint8*>(Particle) + IntrinsicPayloadOffset);
}

const FRibbonParticlePayload* FParticleRibbonEmitterInstance::GetRibbonPayload(const FBaseParticle* Particle) const
{
	return GetRibbonPayload(const_cast<FBaseParticle*>(Particle));
}

void FParticleRibbonEmitterInstance::SpawnTrailPoint(int32 TrailIndex, uint32 SourceParticleId, const FVector& WorldLocation)
{
	const int32 PreviousTrailIndex = PendingTrailIndex;
	const uint32 PreviousSourceParticleId = PendingSourceParticleId;
	PendingTrailIndex = TrailIndex;
	PendingSourceParticleId = SourceParticleId;
	SpawnParticles(1, EmitterTime, 0.0f, ConvertWorldLocationToParticleSpace(WorldLocation), FVector::ZeroVector, true);
	PendingTrailIndex = PreviousTrailIndex;
	PendingSourceParticleId = PreviousSourceParticleId;
	TrimTrail(TrailIndex);
}

void FParticleRibbonEmitterInstance::TrimTrail(int32 TrailIndex)
{
	const UParticleModuleTypeDataRibbon* TypeData = CurrentLODLevel
		? Cast<UParticleModuleTypeDataRibbon>(CurrentLODLevel->GetTypeDataModule())
		: nullptr;
	const int32 MaxParticles = TypeData ? std::max(2, TypeData->MaxParticlesInTrail) : 64;

	while (true)
	{
		int32 Count = 0;
		int32 OldestActiveIndex = -1;
		uint32 OldestSerial = (std::numeric_limits<uint32>::max)();
		for (int32 ActiveIndex = 0; ActiveIndex < ActiveParticles; ++ActiveIndex)
		{
			const FRibbonParticlePayload* Payload = GetRibbonPayload(GetActiveParticle(ActiveIndex));
			if (!Payload || Payload->TrailIndex != TrailIndex)
			{
				continue;
			}
			++Count;
			if (Payload->SpawnSerial < OldestSerial)
			{
				OldestSerial = Payload->SpawnSerial;
				OldestActiveIndex = ActiveIndex;
			}
		}
		if (Count <= MaxParticles || OldestActiveIndex < 0)
		{
			return;
		}
		KillParticle(OldestActiveIndex);
	}
}

void FParticleRibbonEmitterInstance::SampleParticleSource(
	FSourceTrailState& Trail, const FVector& WorldLocation, const UParticleModuleSpawnPerUnit* SpawnPerUnit)
{
	const UParticleModuleTypeDataRibbon* TypeData = CurrentLODLevel
		? Cast<UParticleModuleTypeDataRibbon>(CurrentLODLevel->GetTypeDataModule())
		: nullptr;
	if (!Trail.bInitialized)
	{
		Trail.PreviousLocation = WorldLocation;
		Trail.bInitialized = true;
		if (!TypeData || TypeData->bSpawnInitialParticle)
		{
			SpawnTrailPoint(Trail.TrailIndex, Trail.SourceParticleId, WorldLocation);
		}
		return;
	}

	FVector Travel = WorldLocation - Trail.PreviousLocation;
	const FVector PreviousLocation = Trail.PreviousLocation;
	Trail.PreviousLocation = WorldLocation;
	if (SpawnPerUnit && SpawnPerUnit->bIgnoreMovementAlongZ)
	{
		Travel.Z = 0.0f;
	}
	const float Distance = Travel.Length();
	if (Distance <= (SpawnPerUnit ? SpawnPerUnit->MovementTolerance : FMath::Epsilon))
	{
		return;
	}
	if (SpawnPerUnit && SpawnPerUnit->MaxFrameDistance > 0.0f && Distance > SpawnPerUnit->MaxFrameDistance)
	{
		Trail.SpawnRemainder = 0.0f;
		return;
	}

	int32 SpawnCount = 1;
	if (SpawnPerUnit)
	{
		const float SpawnValue = SpawnPerUnit->SpawnPerUnit.GetValue(EmitterTime, GetDistributionData());
		if (SpawnValue <= 0.0f || SpawnPerUnit->UnitScalar <= 0.0f)
		{
			return;
		}
		const float SpawnFloat = Trail.SpawnRemainder
			+ (Distance / std::max(0.001f, SpawnPerUnit->UnitScalar)) * SpawnValue;
		SpawnCount = static_cast<int32>(std::floor(SpawnFloat));
		Trail.SpawnRemainder = SpawnFloat - static_cast<float>(SpawnCount);
	}
	SpawnCount = std::min(SpawnCount, std::max(0, MaxActiveParticles - ActiveParticles));
	for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
	{
		const float Alpha = static_cast<float>(SpawnIndex + 1) / static_cast<float>(SpawnCount + 1);
		SpawnTrailPoint(Trail.TrailIndex, Trail.SourceParticleId, PreviousLocation + (WorldLocation - PreviousLocation) * Alpha);
	}
}

void FParticleRibbonEmitterInstance::UpdateParticleSourceTrails()
{
	const UParticleModuleTrailSource* TrailSource = FindTrailSourceModule();
	if (!TrailSource || !Component)
	{
		return;
	}
	const UParticleModuleTypeDataRibbon* TypeData = CurrentLODLevel
		? Cast<UParticleModuleTypeDataRibbon>(CurrentLODLevel->GetTypeDataModule())
		: nullptr;
	const int32 MaxTrails = TypeData ? std::max(1, TypeData->MaxTrailCount) : 1;
	const FParticleEmitterInstance* SourceEmitter = Component->FindEmitterInstanceByName(TrailSource->SourceName);
	for (FSourceTrailState& Trail : SourceTrails)
	{
		Trail.bSeenThisFrame = false;
	}
	if (!SourceEmitter || SourceEmitter == this)
	{
		for (FSourceTrailState& Trail : SourceTrails)
		{
			Trail.bSourceAlive = false;
		}
		return;
	}

	const UParticleModuleRequired* SourceRequired = SourceEmitter->GetCurrentLODLevel()
		? SourceEmitter->GetCurrentLODLevel()->GetRequiredModule()
		: nullptr;
	const bool bSourceUsesLocalSpace = SourceRequired && SourceRequired->bUseLocalSpace;
	const UParticleModuleSpawnPerUnit* SpawnPerUnit = FindSpawnPerUnitModule();
	int32 TrackedCount = 0;
	for (int32 ActiveIndex = 0; ActiveIndex < SourceEmitter->GetActiveParticleCount() && TrackedCount < MaxTrails; ++ActiveIndex)
	{
		const int32 ParticleIndex = SourceEmitter->GetParticleIndices()[ActiveIndex];
		const FBaseParticle* SourceParticle = reinterpret_cast<const FBaseParticle*>(
			SourceEmitter->GetParticleData() + static_cast<size_t>(ParticleIndex) * SourceEmitter->GetParticleStride());
		if (!SourceParticle)
		{
			continue;
		}

		FSourceTrailState* State = nullptr;
		for (FSourceTrailState& Existing : SourceTrails)
		{
			if (Existing.SourceParticleId == SourceParticle->ParticleId)
			{
				State = &Existing;
				break;
			}
		}
		if (!State)
		{
			for (FSourceTrailState& Existing : SourceTrails)
			{
				if (Existing.bSourceAlive)
				{
					continue;
				}
				bool bHasRemainingPoints = false;
				for (int32 RibbonActiveIndex = 0; RibbonActiveIndex < ActiveParticles; ++RibbonActiveIndex)
				{
					const FRibbonParticlePayload* Payload = GetRibbonPayload(GetActiveParticle(RibbonActiveIndex));
					if (Payload && Payload->TrailIndex == Existing.TrailIndex)
					{
						bHasRemainingPoints = true;
						break;
					}
				}
				if (!bHasRemainingPoints)
				{
					const int32 ReusedTrailIndex = Existing.TrailIndex;
					Existing = FSourceTrailState();
					Existing.TrailIndex = ReusedTrailIndex;
					Existing.SourceParticleId = SourceParticle->ParticleId;
					State = &Existing;
					break;
				}
			}
			if (!State)
			{
				if (static_cast<int32>(SourceTrails.size()) >= MaxTrails)
				{
					continue;
				}
				FSourceTrailState NewState;
				NewState.TrailIndex = static_cast<int32>(SourceTrails.size());
				NewState.SourceParticleId = SourceParticle->ParticleId;
				SourceTrails.push_back(NewState);
				State = &SourceTrails.back();
			}
		}

		FVector SourceLocation = SourceParticle->Location;
		if (bSourceUsesLocalSpace)
		{
			SourceLocation = Component->GetWorldMatrix().TransformPositionWithW(SourceLocation);
		}
		SourceLocation += TrailSource->SourceOffset;
		State->bSeenThisFrame = true;
		State->bSourceAlive = true;
		SampleParticleSource(*State, SourceLocation, SpawnPerUnit);
		++TrackedCount;
	}

	for (FSourceTrailState& Trail : SourceTrails)
	{
		if (!Trail.bSeenThisFrame)
		{
			Trail.bSourceAlive = false;
		}
	}
}

FDynamicEmitterDataBase* FParticleRibbonEmitterInstance::CreateDynamicData(int32 EmitterIndex) const
{
	FDynamicEmitterDataBase* DynamicData = FParticleEmitterInstance::CreateDynamicData(EmitterIndex);
	if (DynamicData)
	{
		static_cast<FDynamicRibbonEmitterData*>(DynamicData)->GetRibbonSource().RibbonPayloadOffset = IntrinsicPayloadOffset;
	}
	return DynamicData;
}
