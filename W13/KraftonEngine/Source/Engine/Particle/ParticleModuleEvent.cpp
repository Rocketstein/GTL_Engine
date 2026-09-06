#include "ParticleModuleEvent.h"

#include "Component/Primitive/ParticleSystemComponent.h"
#include "Math/MathUtils.h"
#include "Particle/Asset/ParticleSerialization.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleLODLevel.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>

namespace
{
	template<typename TDistribution>
	TDistribution* NewDistribution(UObject* Outer)
	{
		return UObjectManager::Get().CreateObject<TDistribution>(Outer);
	}

	UDistributionFloatConstant* NewFloatConstant(UObject* Outer, float Value)
	{
		UDistributionFloatConstant* Distribution = NewDistribution<UDistributionFloatConstant>(Outer);
		Distribution->Constant = Value;
		return Distribution;
	}

	UDistributionVectorConstant* NewVectorConstant(UObject* Outer, const FVector& Value)
	{
		UDistributionVectorConstant* Distribution = NewDistribution<UDistributionVectorConstant>(Outer);
		Distribution->Constant = Value;
		return Distribution;
	}

	bool IsNoneEventName(FName Name)
	{
		return !Name.IsValid() || Name == FName::None || Name.ToString().empty();
	}

	FName GetDefaultEventName(EParticleEventType Type)
	{
		switch (Type)
		{
		case EParticleEventType::Spawn:
			return FName("Spawn");
		case EParticleEventType::Death:
			return FName("Death");
		case EParticleEventType::Collision:
			return FName("Collision");
		case EParticleEventType::Burst:
			return FName("Burst");
		case EParticleEventType::Kismet:
			return FName("Kismet");
		case EParticleEventType::Any:
		default:
			return FName("Any");
		}
	}

	FName ResolveEventName(const FParticleEventGenerateInfo& Info)
	{
		return IsNoneEventName(Info.CustomName) ? GetDefaultEventName(Info.Type) : Info.CustomName;
	}

	bool UsesLocalSpace(FParticleEmitterInstance& Owner)
	{
		UParticleLODLevel* LODLevel = Owner.GetCurrentLODLevel();
		return LODLevel
			&& LODLevel->GetRequiredModule()
			&& LODLevel->GetRequiredModule()->bUseLocalSpace;
	}

	FVector ToWorldPosition(FParticleEmitterInstance& Owner, const FVector& Position)
	{
		return UsesLocalSpace(Owner) && Owner.Component
			? Owner.Component->GetWorldMatrix().TransformPositionWithW(Position)
			: Position;
	}

	FVector ToWorldVector(FParticleEmitterInstance& Owner, const FVector& Vector)
	{
		return UsesLocalSpace(Owner) && Owner.Component
			? Owner.Component->GetWorldMatrix().TransformVector(Vector)
			: Vector;
	}

	FVector ToReceiverPosition(FParticleEmitterInstance& Owner, const FVector& WorldPosition)
	{
		return UsesLocalSpace(Owner) && Owner.Component
			? Owner.Component->GetWorldMatrix().GetInverse().TransformPositionWithW(WorldPosition)
			: WorldPosition;
	}

	FVector ToReceiverVector(FParticleEmitterInstance& Owner, const FVector& WorldVector)
	{
		return UsesLocalSpace(Owner) && Owner.Component
			? Owner.Component->GetWorldMatrix().GetInverse().TransformVector(WorldVector)
			: WorldVector;
	}

	float GetParticleTime(const FBaseParticle* Particle)
	{
		return Particle ? FMath::Clamp(Particle->RelativeTime, 0.0f, 1.0f) : 0.0f;
	}

	FParticleEventData MakeParticleEvent(
		EParticleEventType Type,
		FParticleEmitterInstance& Owner,
		const FBaseParticle* Particle)
	{
		FParticleEventData EventData;
		EventData.Type = Type;
		EventData.EmitterTime = Owner.GetEmitterTime();
		EventData.ParticleTime = GetParticleTime(Particle);
		if (Particle)
		{
			EventData.Location = ToWorldPosition(Owner, Particle->Location);
			EventData.Velocity = ToWorldVector(Owner, Particle->Velocity);
			EventData.Direction = EventData.Velocity.Normalized();
		}
		return EventData;
	}
}

void UParticleModuleEventSendToGame::Serialize(FArchive& Ar)
{
	SerializeProperties(Ar, PF_Save);
}

void UParticleModuleEventSendToGame::DoEvent(const FParticleEventData& EventData, FParticleEmitterInstance& Owner)
{
	(void)EventData;
	(void)Owner;
}

UParticleModuleEventGenerator::~UParticleModuleEventGenerator()
{
	ParticleSerialization::DestroyObjectArray(ParticleModuleEventsToSendToGame);
}

void UParticleModuleEventGenerator::Serialize(FArchive& Ar)
{
	UParticleModuleEventBase::Serialize(Ar);
	ParticleSerialization::SerializeInstancedObjectArray(Ar, ParticleModuleEventsToSendToGame, this);
}

void UParticleModuleEventGenerator::Spawn(const FSpawnContext& Context)
{
	HandleParticleSpawned(Context.Owner, Context.ParticleBase);
}

void UParticleModuleEventGenerator::HandleParticleSpawned(FParticleEmitterInstance& Owner, const FBaseParticle* Particle) const
{
	FParticleEventData EventData = MakeParticleEvent(EParticleEventType::Spawn, Owner, Particle);
	for (const FParticleEventGenerateInfo& Info : Events)
	{
		if (Info.Type == EParticleEventType::Spawn)
		{
			DispatchEvent(Owner, Info, EventData);
		}
	}
}

void UParticleModuleEventGenerator::HandleParticleKilled(FParticleEmitterInstance& Owner, const FBaseParticle* Particle) const
{
	FParticleEventData EventData = MakeParticleEvent(EParticleEventType::Death, Owner, Particle);
	for (const FParticleEventGenerateInfo& Info : Events)
	{
		if (Info.Type == EParticleEventType::Death)
		{
			DispatchEvent(Owner, Info, EventData);
		}
	}
}

void UParticleModuleEventGenerator::HandleParticleBurst(FParticleEmitterInstance& Owner, int32 ParticleCount, const FVector& Location) const
{
	FParticleEventData EventData;
	EventData.Type = EParticleEventType::Burst;
	EventData.EmitterTime = Owner.GetEmitterTime();
	EventData.ParticleCount = ParticleCount;
	EventData.Location = ToWorldPosition(Owner, Location);

	for (const FParticleEventGenerateInfo& Info : Events)
	{
		if (Info.Type == EParticleEventType::Burst)
		{
			DispatchEvent(Owner, Info, EventData);
		}
	}
}

void UParticleModuleEventGenerator::HandleParticleCollision(
	FParticleEmitterInstance& Owner,
	const FBaseParticle* Particle,
	const FVector& Location,
	const FVector& Normal,
	const FVector& Direction) const
{
	FParticleEventData EventData = MakeParticleEvent(EParticleEventType::Collision, Owner, Particle);
	EventData.Location = Location;
	EventData.Normal = Normal;
	EventData.Direction = Direction;

	if (!Direction.IsNearlyZero())
	{
		EventData.Velocity = Direction.Normalized() * EventData.Velocity.Length();
	}

	for (const FParticleEventGenerateInfo& Info : Events)
	{
		if (Info.Type == EParticleEventType::Collision)
		{
			DispatchEvent(Owner, Info, EventData);
		}
	}
}

void UParticleModuleEventGenerator::DispatchEvent(FParticleEmitterInstance& Owner, const FParticleEventGenerateInfo& Info, FParticleEventData EventData) const
{
	EventData.EventName = ResolveEventName(Info);
	if (!ShouldGenerateEvent(Owner, Info, EventData))
	{
		return;
	}

	for (UParticleModuleEventSendToGame* Sender : ParticleModuleEventsToSendToGame)
	{
		if (Sender)
		{
			Sender->DoEvent(EventData, Owner);
		}
	}

	Owner.QueueParticleEvent(EventData);
}

bool UParticleModuleEventGenerator::ShouldGenerateEvent(FParticleEmitterInstance& Owner, const FParticleEventGenerateInfo& Info, const FParticleEventData& EventData) const
{
	if (Info.Type != EventData.Type)
	{
		return false;
	}

	if (Info.bFirstTimeOnly && EventData.ParticleTime > 0.0f)
	{
		return false;
	}

	if (Info.bLastTimeOnly && EventData.Type != EParticleEventType::Death)
	{
		return false;
	}

	const uint32 Serial = Owner.AllocateEventSerial();
	const int32 Frequency = std::max(1, Info.Frequency);
	const int32 ParticleFrequency = std::max(1, Info.ParticleFrequency);
	return (Serial % static_cast<uint32>(Frequency)) == 0
		&& (Serial % static_cast<uint32>(ParticleFrequency)) == 0;
}

void UParticleModuleEventReceiverBase::Update(const FUpdateContext& Context)
{
	const TArray<FParticleEventData> EventsSnapshot = Context.Owner.Component
		? Context.Owner.Component->GetParticleEvents()
		: Context.Owner.GetPendingEvents();
	for (const FParticleEventData& EventData : EventsSnapshot)
	{
		if (WillProcessParticleEvent(EventData))
		{
			ProcessParticleEvent(EventData, Context.Owner);
		}
	}
}

bool UParticleModuleEventReceiverBase::WillProcessParticleEvent(const FParticleEventData& EventData) const
{
	return EventData.Matches(EventGeneratorType, EventName);
}

void UParticleModuleEventReceiverBase::ProcessParticleEvent(const FParticleEventData& EventData, FParticleEmitterInstance& Owner)
{
	(void)EventData;
	(void)Owner;
}

UParticleModuleEventReceiverSpawn::UParticleModuleEventReceiverSpawn()
{
	SpawnCount.SetDistribution(NewFloatConstant(this, 1.0f));
	InheritVelocityScale.SetDistribution(NewVectorConstant(this, FVector::OneVector));
}

void UParticleModuleEventReceiverSpawn::ProcessParticleEvent(const FParticleEventData& EventData, FParticleEmitterInstance& Owner)
{
	UObject* DistributionData = Owner.GetDistributionData();
	const float EvalTime = bUseParticleTime ? EventData.ParticleTime : Owner.GetEmitterTime();
	const float SpawnCountValue = SpawnCount.IsCreated()
		? SpawnCount.GetValue(EvalTime, DistributionData)
		: 1.0f;
	const int32 Count = std::max(0, static_cast<int32>(std::floor(SpawnCountValue + 0.5f)));
	if (Count <= 0)
	{
		return;
	}

	const FVector EventNormal = EventData.Normal.IsNearlyZero()
		? FVector::ZeroVector
		: EventData.Normal.Normalized();
	const FVector EventWorldOffset = ToWorldVector(Owner, EventLocationOffset)
		+ EventNormal * EventNormalOffset;

	const FVector SpawnLocation = bUsePSysLocation
		? (UsesLocalSpace(Owner) ? FVector::ZeroVector : Owner.GetComponentLocation())
		: ToReceiverPosition(Owner, EventData.Location + EventWorldOffset);

	FVector SpawnVelocity = FVector::ZeroVector;
	if (bInheritVelocity)
	{
		SpawnVelocity = ToReceiverVector(Owner, EventData.Velocity) * InheritVelocityScale.GetValue(EvalTime, DistributionData);
	}

	const int32 ActiveBeforeSpawn = Owner.GetActiveParticleCount();
	Owner.SpawnParticles(Count, Owner.GetEmitterTime(), 0.0f, SpawnLocation, SpawnVelocity, bUseEventLocationOnly);
	const int32 SpawnedCount = std::max(0, Owner.GetActiveParticleCount() - ActiveBeforeSpawn);
	Owner.RecordEventReceiverSpawn(Count, SpawnedCount);
}

void UParticleModuleEventReceiverKillParticles::ProcessParticleEvent(const FParticleEventData& EventData, FParticleEmitterInstance& Owner)
{
	(void)EventData;
	if (bStopSpawning)
	{
		Owner.SetSpawningSuppressed(true);
	}
	Owner.KillAllParticles();
}
