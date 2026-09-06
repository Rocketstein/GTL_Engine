#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Transform.h"
#include "Object/FName.h"
#include "Particle/ParticleEvent.h"
#include "Particle/ParticleDynamicData.h"

class UObject;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleSpawn;
class UParticleModuleSpawnPerUnit;
class UParticleModuleTrailSource;
class UParticleSystemComponent;

struct FParticleModuleRuntimeCache
{
	UParticleModule* Module = nullptr;
	FParticleModuleCache Cache;
};

struct FParticleLODLevelCompiledData
{
	int32 ParticleSize = sizeof(FBaseParticle);
	int32 ParticleStride = sizeof(FBaseParticle);
	int32 InstancePayloadSize = 0;
	TArray<FParticleModuleRuntimeCache> ModuleCaches;

	const FParticleModuleCache* FindModuleCache(const UParticleModule* Module) const;
};

struct FParticleEmitterInstance
{
	FParticleEmitterInstance() = default;
	virtual ~FParticleEmitterInstance() = default;

	static FParticleEmitterInstance* Create(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate);

	virtual EParticleEmitterType GetEmitterType() const = 0;
	virtual void Init(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate);
	virtual void Reset();
	virtual void Tick(float DeltaTime);
	void ProcessParticleEvents(const TArray<FParticleEventData>& Events);

	bool SetCurrentLODIndex(int32 InLODIndex);
	void BuildLODData(UParticleLODLevel* LODLevel);

	void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, bool bLockInitialLocation = false);
	virtual void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);
	virtual void PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime);
	void KillParticle(int32 ActiveIndex);
	void KillAllParticles();
	void QueueParticleEvent(const FParticleEventData& EventData);
	void ClearPendingEvents();
	const TArray<FParticleEventData>& GetPendingEvents() const { return PendingEvents; }
	uint32 AllocateEventSerial() { return ++EventCounter; }
	void RecordEventReceiverSpawn(int32 RequestedCount, int32 SpawnedCount);
	void SetSpawningSuppressed(bool bInSuppressed) { bSpawningSuppressed = bInSuppressed; }
	bool IsSpawningSuppressed() const { return bSpawningSuppressed; }

	virtual FDynamicEmitterDataBase* CreateDynamicData(int32 EmitterIndex) const;

	UParticleEmitter* GetEmitterTemplate() const { return EmitterTemplate; }
	UParticleLODLevel* GetCurrentLODLevel() const { return CurrentLODLevel; }
	int32 GetCurrentLODIndex() const { return CurrentLODLevelIndex; }
	int32 GetActiveParticleCount() const { return ActiveParticles; }
	int32 GetMaxActiveParticleCount() const { return MaxActiveParticles; }
	int32 GetLastProcessedParticleEventCount() const { return LastProcessedParticleEventCount; }
	int32 GetLastEventReceiverSpawnRequestCount() const { return LastEventReceiverSpawnRequestCount; }
	int32 GetLastEventReceiverSpawnedCount() const { return LastEventReceiverSpawnedCount; }
	int32 GetParticleStride() const { return ParticleStride; }
	float GetEmitterTime() const { return EmitterTime; }
	uint64 GetAllocatedMemoryBytes() const
	{
		return static_cast<uint64>(DataContainer.MemBlockSize)
			+ static_cast<uint64>(InstanceDataStorage.size());
	}
	uint8* GetParticleData() { return ParticleData; }
	const uint8* GetParticleData() const { return ParticleData; }
	uint16* GetParticleIndices() { return ParticleIndices; }
	const uint16* GetParticleIndices() const { return ParticleIndices; }
	uint8* GetModuleInstanceData(const UParticleModule* Module);
	const uint8* GetModuleInstanceData(const UParticleModule* Module) const;

	const FTransform& GetComponentTransform() const;
	FVector GetComponentLocation() const;
	virtual FVector GetSpawnReferenceLocation() const;
	FVector ConvertWorldLocationToParticleSpace(const FVector& WorldLocation) const;
	UObject* GetDistributionData() const;
	FString GetTemplateName() const;
	FString GetInstanceName() const;

	UParticleEmitter* EmitterTemplate = nullptr;
	UParticleSystemComponent* Component = nullptr;
	int32 CurrentLODLevelIndex = -1;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	/** Pointer to the particle data array.                             */
	uint8* ParticleData = nullptr;
	/** Pointer to the particle index array.                            */
	uint16* ParticleIndices = nullptr;
	/** Pointer to the instance data array.                             */
	uint8* InstanceData = nullptr;
	/** The size of the Instance data array.                            */
	int32 InstancePayloadSize = 0;
	/** The offset to the particle data.                                */
	int32 PayloadOffset = 0;
	/** The total size of a particle (in bytes).                        */
	int32 ParticleSize = sizeof(FBaseParticle);
	/** The stride between particles in the ParticleData array.         */
	int32 ParticleStride = sizeof(FBaseParticle);
	/** The number of particles currently active in the emitter.        */
	int32 ActiveParticles = 0;
	/** Monotonically increasing counter. */
	uint32 ParticleCounter = 0;
	/** The maximum number of active particles that can be held in
	 *  the particle data array.
	 */
	int32 MaxActiveParticles = 0;
	// The fraction of time left over from spawning
	float SpawnFraction = 0.0f;


	// 실제 메모리 소유자 실제 Alloc / Free를 담당
	// 위쪽의 ParticleData와 ParitcleIndices는 결국 메크로에서 편하게 사용하기 위해 빼놓은 정보
	FParticleDataContainer DataContainer;
	TArray<uint8> InstanceDataStorage;

	float EmitterTime = 0.0f;
	bool  bBurstFired = false;
	bool bSpawningSuppressed = false;
	uint32 EventCounter = 0;
	int32 LastProcessedParticleEventCount = 0;
	int32 LastEventReceiverSpawnRequestCount = 0;
	int32 LastEventReceiverSpawnedCount = 0;
	TArray<FParticleEventData> PendingEvents;

protected:
	virtual EDynamicEmitterType GetDynamicEmitterType() const = 0;
	virtual FDynamicEmitterDataBase* CreateDynamicEmitterData(const UParticleModuleRequired* RequiredModule) const;
	virtual int32 GetIntrinsicParticlePayloadSize() const { return 0; }
	int32 IntrinsicPayloadOffset = -1;

protected:
	UParticleModuleSpawn* GetSpawnRateModule() const;
	FBaseParticle* GetParticleDirect(int32 ParticleIndex);
	const FBaseParticle* GetParticleDirect(int32 ParticleIndex) const;
	FBaseParticle* GetActiveParticle(int32 ActiveIndex);
	const FBaseParticle* GetActiveParticle(int32 ActiveIndex) const;

private:
	void AllocateParticleDataPreservingBaseParticles();
	void InitializeParticleIndices();
	void UpdateParticleLifetimesAndMovement(float DeltaTime);
	void NotifyParticleKilled(const FBaseParticle* Particle);
	void NotifyParticleBurst(int32 ParticleCount, const FVector& Location);

	FParticleLODLevelCompiledData LODData;
};

struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
	EParticleEmitterType GetEmitterType() const override { return EParticleEmitterType::Sprite; }

private:
	EDynamicEmitterType GetDynamicEmitterType() const override { return EDynamicEmitterType::Sprite; }
};

struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
	EParticleEmitterType GetEmitterType() const override { return EParticleEmitterType::Mesh; }

private:
	EDynamicEmitterType GetDynamicEmitterType() const override { return EDynamicEmitterType::Mesh; }
	FDynamicEmitterDataBase* CreateDynamicEmitterData(const UParticleModuleRequired* RequiredModule) const override;
};

struct FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
	EParticleEmitterType GetEmitterType() const override { return EParticleEmitterType::Beam; }

private:
	EDynamicEmitterType GetDynamicEmitterType() const override { return EDynamicEmitterType::Beam; }
	FDynamicEmitterDataBase* CreateDynamicEmitterData(const UParticleModuleRequired* RequiredModule) const override;
};

struct FParticleRibbonEmitterInstance : public FParticleEmitterInstance
{
	EParticleEmitterType GetEmitterType() const override { return EParticleEmitterType::Ribbon; }
	void Init(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate) override;
	void Reset() override;
	void Tick(float DeltaTime) override;
	void PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime) override;
	FVector GetSpawnReferenceLocation() const override;
	FDynamicEmitterDataBase* CreateDynamicData(int32 EmitterIndex) const override;

private:
	struct FSourceTrailState
	{
		int32 TrailIndex = 0;
		uint32 SourceParticleId = 0;
		FVector PreviousLocation = FVector::ZeroVector;
		float SpawnRemainder = 0.0f;
		bool bInitialized = false;
		bool bSeenThisFrame = false;
		bool bSourceAlive = true;
	};

	EDynamicEmitterType GetDynamicEmitterType() const override { return EDynamicEmitterType::Ribbon; }
	FDynamicEmitterDataBase* CreateDynamicEmitterData(const UParticleModuleRequired* RequiredModule) const override;
	int32 GetIntrinsicParticlePayloadSize() const override { return sizeof(FRibbonParticlePayload); }
	const UParticleModuleTrailSource* FindTrailSourceModule() const;
	const UParticleModuleSpawnPerUnit* FindSpawnPerUnitModule() const;
	bool ResolveSingleSourceLocation(FVector& OutLocation) const;
	void UpdateParticleSourceTrails();
	void SampleParticleSource(FSourceTrailState& Trail, const FVector& WorldLocation, const UParticleModuleSpawnPerUnit* SpawnPerUnit);
	void SpawnTrailPoint(int32 TrailIndex, uint32 SourceParticleId, const FVector& WorldLocation);
	void TrimTrail(int32 TrailIndex);
	FRibbonParticlePayload* GetRibbonPayload(FBaseParticle* Particle) const;
	const FRibbonParticlePayload* GetRibbonPayload(const FBaseParticle* Particle) const;

	FVector CurrentSourceLocation = FVector::ZeroVector;
	bool bHasCurrentSource = false;
	int32 PendingTrailIndex = 0;
	uint32 PendingSourceParticleId = 0;
	uint32 RibbonSpawnSerial = 0;
	bool bSingleSourceInitialized = false;
	TArray<FSourceTrailState> SourceTrails;
};
