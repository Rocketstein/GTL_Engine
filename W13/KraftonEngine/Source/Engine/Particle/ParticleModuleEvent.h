#pragma once

#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Particle/ParticleEvent.h"
#include "Particle/ParticleModule.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Particle/ParticleModuleEvent.generated.h"

class FArchive;

UCLASS()
class UParticleModuleEventSendToGame : public UObject
{
public:
	GENERATED_BODY()
	UParticleModuleEventSendToGame() = default;

	void Serialize(FArchive& Ar) override;
	virtual void DoEvent(const FParticleEventData& EventData, FParticleEmitterInstance& Owner);
};

UCLASS()
class UParticleModuleEventBase : public UParticleModule
{
public:
	GENERATED_BODY()
	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Event; }
};

USTRUCT()
struct FParticleEventGenerateInfo
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Event", DisplayName="Type", Enum=EParticleEventType)
	EParticleEventType Type = EParticleEventType::Spawn;

	UPROPERTY(Edit, Save, Category="Event", DisplayName="Frequency", Min=1.0f, Max=100000.0f, Speed=1.0f)
	int32 Frequency = 1;

	UPROPERTY(Edit, Save, Category="Event", DisplayName="Particle Frequency", Min=1.0f, Max=100000.0f, Speed=1.0f)
	int32 ParticleFrequency = 1;

	UPROPERTY(Edit, Save, Category="Event", DisplayName="First Time Only")
	bool bFirstTimeOnly = false;

	UPROPERTY(Edit, Save, Category="Event", DisplayName="Last Time Only")
	bool bLastTimeOnly = false;

	UPROPERTY(Edit, Save, Category="Event", DisplayName="Use Reflected Impact Vector")
	bool bUseReflectedImpactVector = false;

	UPROPERTY(Edit, Save, Category="Event", DisplayName="Use Orbit Offset")
	bool bUseOrbitOffset = false;

	UPROPERTY(Edit, Save, Category="Event", DisplayName="Custom Name")
	FName CustomName;
};

UCLASS()
class UParticleModuleEventGenerator : public UParticleModuleEventBase
{
public:
	GENERATED_BODY()
	UParticleModuleEventGenerator() = default;
	~UParticleModuleEventGenerator() override;

	bool IsSpawnModule() const override { return true; }
	void Spawn(const FSpawnContext& Context) override;
	void Serialize(FArchive& Ar) override;

	void HandleParticleSpawned(FParticleEmitterInstance& Owner, const FBaseParticle* Particle) const;
	void HandleParticleKilled(FParticleEmitterInstance& Owner, const FBaseParticle* Particle) const;
	void HandleParticleBurst(FParticleEmitterInstance& Owner, int32 ParticleCount, const FVector& Location) const;
	void HandleParticleCollision(
		FParticleEmitterInstance& Owner,
		const FBaseParticle* Particle,
		const FVector& Location,
		const FVector& Normal,
		const FVector& Direction) const;

	UPROPERTY(Edit, Save, Category="Event", DisplayName="Events", Type=Array, Struct=FParticleEventGenerateInfo)
	TArray<FParticleEventGenerateInfo> Events;

	UPROPERTY(Edit, Category="Event", DisplayName="Send To Game", Type=Array, AllowedClass=UParticleModuleEventSendToGame)
	TArray<UParticleModuleEventSendToGame*> ParticleModuleEventsToSendToGame;

private:
	void DispatchEvent(FParticleEmitterInstance& Owner, const FParticleEventGenerateInfo& Info, FParticleEventData EventData) const;
	bool ShouldGenerateEvent(FParticleEmitterInstance& Owner, const FParticleEventGenerateInfo& Info, const FParticleEventData& EventData) const;
};

UCLASS()
class UParticleModuleEventReceiverBase : public UParticleModuleEventBase
{
public:
	GENERATED_BODY()

	bool IsUpdateModule() const override { return true; }
	void Update(const FUpdateContext& Context) override;

	virtual bool WillProcessParticleEvent(const FParticleEventData& EventData) const;
	virtual void ProcessParticleEvent(const FParticleEventData& EventData, FParticleEmitterInstance& Owner);

	UPROPERTY(Edit, Save, Category="Event Receiver", DisplayName="Event Generator Type", Enum=EParticleEventType)
	EParticleEventType EventGeneratorType = EParticleEventType::Any;

	UPROPERTY(Edit, Save, Category="Event Receiver", DisplayName="Event Name")
	FName EventName;
};

UCLASS()
class UParticleModuleEventReceiverSpawn : public UParticleModuleEventReceiverBase
{
public:
	GENERATED_BODY()
	UParticleModuleEventReceiverSpawn();

	void ProcessParticleEvent(const FParticleEventData& EventData, FParticleEmitterInstance& Owner) override;

	UPROPERTY(Edit, Save, Instanced, Category="Event Receiver Spawn", DisplayName="Spawn Count", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=SpawnCount.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat SpawnCount;

	UPROPERTY(Edit, Save, Category="Event Receiver Spawn", DisplayName="Use Particle Time")
	bool bUseParticleTime = false;

	UPROPERTY(Edit, Save, Category="Event Receiver Spawn", DisplayName="Use Particle System Location")
	bool bUsePSysLocation = false;

	UPROPERTY(Edit, Save, Category="Event Receiver Spawn", DisplayName="Use Event Location Only")
	bool bUseEventLocationOnly = true;

	UPROPERTY(Edit, Save, Category="Event Receiver Spawn", DisplayName="Event Location Offset")
	FVector EventLocationOffset = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Event Receiver Spawn", DisplayName="Event Normal Offset", Min=0.0f, Max=10000.0f, Speed=0.1f)
	float EventNormalOffset = 0.0f;

	UPROPERTY(Edit, Save, Category="Event Receiver Spawn", DisplayName="Inherit Velocity")
	bool bInheritVelocity = false;

	UPROPERTY(Edit, Save, Instanced, Category="Event Receiver Spawn", DisplayName="Inherit Velocity Scale", Type=ObjectRef, AllowedClass=UDistributionVector, Member=InheritVelocityScale.Distribution, CppType=UDistributionVector*)
	;
	FRawDistributionVector InheritVelocityScale;
};

UCLASS()
class UParticleModuleEventReceiverKillParticles : public UParticleModuleEventReceiverBase
{
public:
	GENERATED_BODY()

	void ProcessParticleEvent(const FParticleEventData& EventData, FParticleEmitterInstance& Owner) override;

	UPROPERTY(Edit, Save, Category="Event Receiver Kill", DisplayName="Stop Spawning")
	bool bStopSpawning = false;
};
