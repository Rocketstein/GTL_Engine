#include "ParticleModuleCollision.h"

#include "Component/Primitive/ParticleSystemComponent.h"
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleHelper.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModuleEvent.h"

#include <algorithm>
#include <cmath>

namespace
{
	struct FParticleCollisionPayload
	{
		FVector UsedDampingFactor = FVector::OneVector;
		FVector UsedDampingFactorRotation = FVector::OneVector;
		int32 UsedCollisions = 0;
		float Delay = 0.0f;
	};

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

	UDistributionFloatUniform* NewFloatUniform(UObject* Outer, float Min, float Max)
	{
		UDistributionFloatUniform* Distribution = NewDistribution<UDistributionFloatUniform>(Outer);
		Distribution->Min = Min;
		Distribution->Max = Max;
		return Distribution;
	}

	UDistributionVectorConstant* NewVectorConstant(UObject* Outer, const FVector& Value)
	{
		UDistributionVectorConstant* Distribution = NewDistribution<UDistributionVectorConstant>(Outer);
		Distribution->Constant = Value;
		return Distribution;
	}

	UDistributionVectorUniform* NewVectorUniform(UObject* Outer, const FVector& Min, const FVector& Max)
	{
		UDistributionVectorUniform* Distribution = NewDistribution<UDistributionVectorUniform>(Outer);
		Distribution->Min = Min;
		Distribution->Max = Max;
		return Distribution;
	}

	FVector ReflectVector(const FVector& Vector, const FVector& Normal)
	{
		return Vector - Normal * (2.0f * Vector.Dot(Normal));
	}

	FVector ResolveHitNormal(const FHitResult& Hit, const FVector& Direction)
	{
		FVector Normal = Hit.WorldNormal;
		if (Normal.IsNearlyZero())
		{
			Normal = Hit.ImpactNormal;
		}
		if (Normal.IsNearlyZero())
		{
			Normal = Direction * -1.0f;
		}
		if (Normal.IsNearlyZero())
		{
			Normal = FVector::UpVector;
		}
		return Normal.Normalized();
	}

	float MaxComponent(const FVector& Value)
	{
		return (std::max)(std::fabs(Value.X), (std::max)(std::fabs(Value.Y), std::fabs(Value.Z)));
	}

	FVector ToWorldPosition(const UParticleSystemComponent* Component, bool bUseLocalSpace, const FVector& Position)
	{
		return bUseLocalSpace && Component
			? Component->GetWorldMatrix().TransformPositionWithW(Position)
			: Position;
	}

	FVector ToLocalPosition(const UParticleSystemComponent* Component, bool bUseLocalSpace, const FVector& Position)
	{
		return bUseLocalSpace && Component
			? Component->GetWorldMatrix().GetInverse().TransformPositionWithW(Position)
			: Position;
	}

	FVector ToWorldVector(const UParticleSystemComponent* Component, bool bUseLocalSpace, const FVector& Vector)
	{
		return bUseLocalSpace && Component
			? Component->GetWorldMatrix().TransformVector(Vector)
			: Vector;
	}

	FVector ToLocalVector(const UParticleSystemComponent* Component, bool bUseLocalSpace, const FVector& Vector)
	{
		return bUseLocalSpace && Component
			? Component->GetWorldMatrix().GetInverse().TransformVector(Vector)
			: Vector;
	}

	void DispatchCollisionEvent(
		FParticleEmitterInstance& Owner,
		const FBaseParticle& Particle,
		const FVector& Location,
		const FVector& Normal,
		const FVector& Direction)
	{
		UParticleLODLevel* LODLevel = Owner.GetCurrentLODLevel();
		if (!LODLevel)
		{
			return;
		}

		for (UParticleModule* Module : LODLevel->GetEventGeneratorModules())
		{
			if (UParticleModuleEventGenerator* Generator = Cast<UParticleModuleEventGenerator>(Module))
			{
				Generator->HandleParticleCollision(Owner, &Particle, Location, Normal, Direction);
			}
		}
	}
}

UParticleModuleCollision::UParticleModuleCollision()
{
	DampingFactor.SetDistribution(NewVectorUniform(this, FVector(0.5f, 0.5f, 0.5f), FVector(0.5f, 0.5f, 0.5f)));
	DampingFactorRotation.SetDistribution(NewVectorConstant(this, FVector::OneVector));
	MaxCollisions.SetDistribution(NewFloatUniform(this, 1.0f, 1.0f));
	DelayAmount.SetDistribution(NewFloatConstant(this, 0.0f));
}

int32 UParticleModuleCollision::GetParticlePayloadSize() const
{
	return sizeof(FParticleCollisionPayload);
}

void UParticleModuleCollision::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	SPAWN_INIT
	PARTICLE_ELEMENT(FParticleCollisionPayload, CollisionPayload)
	UObject* DistributionData = Context.GetDistributionData();
	CollisionPayload.UsedDampingFactor = DampingFactor.GetValue(Context.Owner.GetEmitterTime(), DistributionData);
	CollisionPayload.UsedDampingFactorRotation = DampingFactorRotation.GetValue(Context.Owner.GetEmitterTime(), DistributionData);
	CollisionPayload.UsedCollisions = (std::max)(0, static_cast<int32>(std::floor(MaxCollisions.GetValue(Context.Owner.GetEmitterTime(), DistributionData) + 0.5f)));
	CollisionPayload.Delay = (std::max)(0.0f, DelayAmount.GetValue(Context.Owner.GetEmitterTime(), DistributionData));

	if (CollisionPayload.Delay > 0.0f)
	{
		Particle.Flags |= STATE_Particle_DelayCollisions;
	}
	Particle.Flags &= ~STATE_Particle_CollisionHasOccurred;
}

void UParticleModuleCollision::Update(const FUpdateContext& Context)
{
	FParticleEmitterInstance& Owner = Context.Owner;
	UParticleSystemComponent* Component = Owner.Component;
	UParticleLODLevel* LODLevel = Owner.GetCurrentLODLevel();
	if (!Component || !LODLevel || Owner.ActiveParticles <= 0)
	{
		return;
	}

	const bool bUseLocalSpace = LODLevel->GetRequiredModule() && LODLevel->GetRequiredModule()->bUseLocalSpace;
	const AActor* IgnoreActor = bIgnoreSourceActor ? Component->GetOwner() : nullptr;
	const float SafeDirScalar = (std::max)(0.001f, DirScalar);

	if (MaxCollisionDistance > 0.0f && Component->GetLastLODDistance() > MaxCollisionDistance)
	{
		return;
	}

	for (int32 ActiveIndex = Owner.ActiveParticles - 1; ActiveIndex >= 0; --ActiveIndex)
	{
		const int32 CurrentIndex = Owner.ParticleIndices[ActiveIndex];
		uint8* ParticleBase = Owner.ParticleData + static_cast<size_t>(CurrentIndex) * Owner.ParticleStride;
		FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(ParticleBase);
		FParticleCollisionPayload& CollisionPayload = *reinterpret_cast<FParticleCollisionPayload*>(ParticleBase + Context.Offset);

		if ((Particle.Flags & STATE_Particle_CollisionIgnoreCheck) != 0)
		{
			continue;
		}

		if ((Particle.Flags & STATE_Particle_DelayCollisions) != 0)
		{
			if (CollisionPayload.Delay > Particle.RelativeTime)
			{
				continue;
			}
			Particle.Flags &= ~STATE_Particle_DelayCollisions;
		}

		const FVector WorldStart = ToWorldPosition(Component, bUseLocalSpace, Particle.OldLocation);
		const FVector WorldEnd = ToWorldPosition(Component, bUseLocalSpace, Particle.Location);
		const FVector MoveDelta = WorldEnd - WorldStart;
		const float MoveDistance = MoveDelta.Length();
		if (MoveDistance <= FMath::Epsilon)
		{
			continue;
		}

		const FVector WorldDirection = MoveDelta / MoveDistance;
		const FVector WorldSize = ToWorldVector(Component, bUseLocalSpace, Particle.Size);
		const float CollisionRadius = (std::max)(0.0f, MaxComponent(WorldSize)) / SafeDirScalar;
		const FVector TraceEnd = WorldEnd + WorldDirection * CollisionRadius;

		FHitResult Hit;
		if (!PerformCollisionCheck(Owner, Particle, Hit, WorldStart, TraceEnd, IgnoreActor))
		{
			continue;
		}

		if (bIgnoreTriggerVolumes
			&& Hit.HitComponent
			&& Hit.HitComponent->GetCollisionObjectType() == ECollisionChannel::Trigger)
		{
			continue;
		}

		bool bDecrementMaxCount = true;
		if (bPawnsDoNotDecrementCount && Cast<APawn>(Hit.HitActor))
		{
			bDecrementMaxCount = false;
		}

		const FVector WorldNormal = ResolveHitNormal(Hit, WorldDirection);
		if (bDecrementMaxCount && bOnlyVerticalNormalsDecrementCount)
		{
			if (std::fabs(WorldNormal.Z) + VerticalFudgeFactor < 1.0f)
			{
				bDecrementMaxCount = false;
			}
		}

		if (bDecrementMaxCount)
		{
			--CollisionPayload.UsedCollisions;
		}

		const FVector WorldBaseVelocity = ToWorldVector(Component, bUseLocalSpace, Particle.BaseVelocity);
		const FVector ReflectedWorldVelocity = ReflectVector(WorldBaseVelocity, WorldNormal) * CollisionPayload.UsedDampingFactor;
		Particle.BaseVelocity = ToLocalVector(Component, bUseLocalSpace, ReflectedWorldVelocity);
		Particle.Velocity = Particle.BaseVelocity;
		Particle.BaseRotationRate *= CollisionPayload.UsedDampingFactorRotation.X;
		Particle.RotationRate = Particle.BaseRotationRate;

		const float HitDistance = (std::max)(0.0f, (std::min)(Hit.Distance, MoveDistance + CollisionRadius));
		const float RemainingDistance = (std::max)(0.0f, MoveDistance + CollisionRadius - HitDistance);
		const FVector ReflectedStep = ReflectVector(WorldDirection, WorldNormal) * RemainingDistance * CollisionPayload.UsedDampingFactor;
		const FVector WorldHitLocation = Hit.WorldHitLocation;
		Particle.Location = ToLocalPosition(Component, bUseLocalSpace, WorldHitLocation + ReflectedStep);
		Particle.OldLocation = ToLocalPosition(Component, bUseLocalSpace, WorldHitLocation);

		if (CollisionPayload.UsedCollisions <= 0)
		{
			Particle.Location = ToLocalPosition(Component, bUseLocalSpace, WorldHitLocation);
			switch (CollisionCompletionOption)
			{
			case EParticleCollisionComplete::Kill:
				DispatchCollisionEvent(Owner, Particle, WorldHitLocation, WorldNormal, WorldDirection);
				Owner.KillParticle(ActiveIndex);
				continue;
			case EParticleCollisionComplete::Freeze:
				Particle.Flags |= STATE_Particle_Freeze;
				break;
			case EParticleCollisionComplete::HaltCollisions:
				Particle.Flags |= STATE_Particle_IgnoreCollisions;
				break;
			case EParticleCollisionComplete::FreezeTranslation:
				Particle.Flags |= STATE_Particle_FreezeTranslation;
				break;
			case EParticleCollisionComplete::FreezeRotation:
				Particle.Flags |= STATE_Particle_FreezeRotation;
				break;
			case EParticleCollisionComplete::FreezeMovement:
				Particle.Flags |= STATE_Particle_FreezeTranslation;
				Particle.Flags |= STATE_Particle_FreezeRotation;
				break;
			default:
				break;
			}
		}

		DispatchCollisionEvent(Owner, Particle, WorldHitLocation, WorldNormal, WorldDirection);
		Particle.Flags |= STATE_Particle_CollisionHasOccurred;
	}
}

bool UParticleModuleCollision::PerformCollisionCheck(
	FParticleEmitterInstance& Owner,
	const FBaseParticle& Particle,
	FHitResult& Hit,
	const FVector& Start,
	const FVector& End,
	const AActor* IgnoreActor) const
{
	(void)Particle;
	UParticleSystemComponent* Component = Owner.Component;
	UWorld* World = Component ? Component->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	const FVector Segment = End - Start;
	const float Distance = Segment.Length();
	if (Distance <= FMath::Epsilon)
	{
		return false;
	}

	return World->PhysicsRaycastByObjectTypes(
		Start,
		Segment / Distance,
		Distance,
		Hit,
		ObjectTypeBit(CollisionType),
		IgnoreActor);
}
