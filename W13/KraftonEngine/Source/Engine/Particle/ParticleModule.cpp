#include "ParticleModule.h"

#include "Component/Primitive/ParticleSystemComponent.h"
#include "Math/MathUtils.h"
#include "Math/Matrix.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleHelper.h"
#include "Particle/ParticleLODLevel.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace
{
	struct FSpawnPerUnitInstancePayload
	{
		FVector PreviousLocation = FVector::ZeroVector;
		float SpawnRemainder = 0.0f;
		float bInitialized = 0.0f;
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

	UDistributionFloatConstantCurve* NewSubImageIndexCurve(UObject* Outer)
	{
		UDistributionFloatConstantCurve* Distribution = NewDistribution<UDistributionFloatConstantCurve>(Outer);
		Distribution->ConstantCurve.AddKey(0.0f, 0.0f);
		Distribution->ConstantCurve.AddKey(1.0f, 0.0f);
		Distribution->ConstantCurve.AutoSetTangents();
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

	FLinearColor ToLinearColor(const FVector& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Alpha);
	}

	FLinearColor ApplyEmissiveIntensity(const FLinearColor& Color, float Intensity)
	{
		const float SafeIntensity = std::max(0.0f, Intensity);
		return FLinearColor(
			Color.R * SafeIntensity,
			Color.G * SafeIntensity,
			Color.B * SafeIntensity,
			Color.A);
	}

	FLinearColor LerpColor(const FLinearColor& A, const FLinearColor& B, float Alpha)
	{
		return FLinearColor(
			FMath::Lerp(A.R, B.R, Alpha),
			FMath::Lerp(A.G, B.G, Alpha),
			FMath::Lerp(A.B, B.B, Alpha),
			FMath::Lerp(A.A, B.A, Alpha));
	}

	uint32 ScrambleSeed(uint32 Seed)
	{
		Seed ^= Seed >> 16;
		Seed *= 0x7feb352du;
		Seed ^= Seed >> 15;
		Seed *= 0x846ca68bu;
		Seed ^= Seed >> 16;
		return Seed;
	}

	uint32 MakeBeamNoiseSeed(const UParticleModule::FSpawnContext& Context)
	{
		const uint32 SpawnBits = static_cast<uint32>(std::fabs(Context.SpawnTime) * 100000.0f);
		const uint32 EmitterBits = static_cast<uint32>(std::fabs(Context.Owner.GetEmitterTime()) * 100000.0f);
		uint32 Seed = Context.Owner.ParticleCounter * 196314165u + 907633515u;
		Seed ^= SpawnBits * 1664525u;
		Seed ^= EmitterBits * 1013904223u;
		Seed ^= static_cast<uint32>(std::rand());
		return ScrambleSeed(Seed);
	}

	float BeamNoiseRandom01(uint32& Seed)
	{
		Seed = Seed * 196314165u + 907633515u;
		return static_cast<float>((Seed >> 8) & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
	}

	float BeamNoiseRandomSigned(uint32& Seed)
	{
		return BeamNoiseRandom01(Seed) * 2.0f - 1.0f;
	}

	FVector MakeBeamNoiseOffset(uint32& Seed)
	{
		FVector Offset(BeamNoiseRandomSigned(Seed), BeamNoiseRandomSigned(Seed), 0.0f);
		if (Offset.IsNearlyZero())
		{
			Offset = FVector(1.0f, 0.0f, 0.0f);
		}
		return Offset;
	}

	void PickBeamNoiseTarget(FParticleBeamNoisePayload& Payload, int32 PointIndex, bool bSnap)
	{
		if (PointIndex < 0 || PointIndex >= MaxParticleBeamNoisePoints)
		{
			return;
		}

		FParticleBeamNoisePoint& Point = Payload.Points[PointIndex];
		Point.SourceOffset = Point.CurrentOffset;
		Point.TargetOffset = MakeBeamNoiseOffset(Payload.RandomSeed);
		Point.LockTimer = 0.0f;
		if (bSnap)
		{
			Point.CurrentOffset = Point.TargetOffset;
			Point.SourceOffset = Point.CurrentOffset;
		}
	}

	int32 ResolveBeamNoiseFrequency(int32 FrequencyLowRange, int32 Frequency, uint32& Seed)
	{
		const int32 MaxFrequency = std::min(std::max(0, Frequency), MaxParticleBeamNoisePoints);
		const int32 MinFrequency = std::min(std::max(0, FrequencyLowRange), MaxFrequency);
		if (MinFrequency > 0 && MinFrequency < MaxFrequency)
		{
			const int32 Range = MaxFrequency - MinFrequency + 1;
			return MinFrequency + static_cast<int32>(BeamNoiseRandom01(Seed) * static_cast<float>(Range)) % Range;
		}
		return MaxFrequency;
	}

	void AdvanceBeamNoisePoint(FParticleBeamNoisePayload& Payload, int32 PointIndex, float DeltaTime)
	{
		if (PointIndex < 0 || PointIndex >= Payload.Frequency || PointIndex >= MaxParticleBeamNoisePoints)
		{
			return;
		}

		FParticleBeamNoisePoint& Point = Payload.Points[PointIndex];
		Point.LockTimer += DeltaTime;

		const bool bHasLockTime = Payload.NoiseLockTime > FMath::Epsilon;
		const bool bHasSpeed = Payload.NoiseSpeed > FMath::Epsilon;
		if (bHasLockTime && Point.LockTimer >= Payload.NoiseLockTime)
		{
			PickBeamNoiseTarget(Payload, PointIndex, !bHasSpeed);
		}

		if (bHasSpeed)
		{
			const FVector ToTarget = Point.TargetOffset - Point.CurrentOffset;
			const float Distance = ToTarget.Length();
			const float MaxStep = Payload.NoiseSpeed * DeltaTime;
			if (Distance <= FMath::Epsilon || Distance <= MaxStep)
			{
				Point.CurrentOffset = Point.TargetOffset;
				if (!bHasLockTime)
				{
					PickBeamNoiseTarget(Payload, PointIndex, false);
				}
			}
			else
			{
				Point.CurrentOffset += ToTarget * (MaxStep / Distance);
			}
		}
	}
}


const FTransform& UParticleModule::FContext::GetTransform() const
{
	return Owner.GetComponentTransform();
}

UObject* UParticleModule::FContext::GetDistributionData() const
{
	return Owner.GetDistributionData();
}

FString UParticleModule::FContext::GetTemplateName() const
{
	return Owner.GetTemplateName();
}

FString UParticleModule::FContext::GetInstanceName() const
{
	return Owner.GetInstanceName();
}


void UParticleModule::Serialize(FArchive& Ar)
{
	SerializeProperties(Ar, PF_Save);
}

UParticleModuleTypeDataSprite::UParticleModuleTypeDataSprite()
{
}

bool UParticleModuleTypeDataSprite::ShouldExposeProperty(const FProperty& Property) const
{
	if (Property.Name &&
		(std::strcmp(Property.Name, "SubUVResourceName") == 0 ||
		 std::strcmp(Property.Name, "SubImagesX") == 0 ||
		 std::strcmp(Property.Name, "SubImagesY") == 0 ||
		 std::strcmp(Property.Name, "SubUVFrameRate") == 0 ||
		 std::strcmp(Property.Name, "bLoopSubUV") == 0))
	{
		return bUseSubUV;
	}

	return UParticleModuleTypeDataBase::ShouldExposeProperty(Property);
}

UParticleModuleSubUV::UParticleModuleSubUV()
{
	SubImageIndex.SetDistribution(NewSubImageIndexCurve(this));
}

int32 UParticleModuleSubUV::GetParticlePayloadSize() const
{
	return sizeof(FParticleSubUVPayload);
}

float UParticleModuleSubUV::DetermineImageIndex(const FContext& Context, const FBaseParticle* Particle) const
{
	const float RelativeTime = Particle ? FMath::Clamp(Particle->RelativeTime, 0.0f, 1.0f) : 0.0f;
	return std::max(0.0f, SubImageIndex.GetValue(RelativeTime, Context.GetDistributionData()));
}

void UParticleModuleSubUV::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	SPAWN_INIT
	PARTICLE_ELEMENT(FParticleSubUVPayload, SubUVPayload)
	SubUVPayload.ImageIndex = DetermineImageIndex(Context, &Particle);
}

void UParticleModuleSubUV::Update(const FUpdateContext& Context)
{
	if (bLockSubImageOnSpawn)
	{
		return;
	}

	BEGIN_UPDATE_LOOP
		FParticleSubUVPayload& SubUVPayload = *reinterpret_cast<FParticleSubUVPayload*>(
			ParticleBase + Context.Offset);
		SubUVPayload.ImageIndex = DetermineImageIndex(Context, &Particle);
	END_UPDATE_LOOP
}

UParticleModuleTypeDataMesh::UParticleModuleTypeDataMesh()
{
}

UParticleModuleTypeDataBeam::UParticleModuleTypeDataBeam()
{
}

UParticleModuleTypeDataRibbon::UParticleModuleTypeDataRibbon()
{
}

int32 UParticleModuleBeamNoise::GetParticlePayloadSize() const
{
	return sizeof(FParticleBeamNoisePayload);
}

void UParticleModuleBeamNoise::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	SPAWN_INIT
	PARTICLE_ELEMENT(FParticleBeamNoisePayload, BeamNoisePayload)
	BeamNoisePayload = FParticleBeamNoisePayload();
	BeamNoisePayload.RandomSeed = MakeBeamNoiseSeed(Context);
	BeamNoisePayload.Frequency = ResolveBeamNoiseFrequency(FrequencyLowRange, Frequency, BeamNoisePayload.RandomSeed);
	BeamNoisePayload.Strength = std::max(0.0f, Strength);
	BeamNoisePayload.NoiseSpeed = std::max(0.0f, Speed);
	BeamNoisePayload.NoiseLockTime = std::max(0.0f, NoiseLockTime);

	for (int32 PointIndex = 0; PointIndex < BeamNoisePayload.Frequency; ++PointIndex)
	{
		PickBeamNoiseTarget(BeamNoisePayload, PointIndex, true);
	}
}

void UParticleModuleBeamNoise::Update(const FUpdateContext& Context)
{
	BEGIN_UPDATE_LOOP
		FParticleBeamNoisePayload& BeamNoisePayload = *reinterpret_cast<FParticleBeamNoisePayload*>(
			ParticleBase + Context.Offset);
		const int32 FrequencyCount = std::min(std::max(0, BeamNoisePayload.Frequency), MaxParticleBeamNoisePoints);
		BeamNoisePayload.Frequency = FrequencyCount;
		for (int32 PointIndex = 0; PointIndex < FrequencyCount; ++PointIndex)
		{
			AdvanceBeamNoisePoint(BeamNoisePayload, PointIndex, DeltaTime);
		}
	END_UPDATE_LOOP
}

UParticleModuleSpawn::UParticleModuleSpawn()
{
	Rate.SetDistribution(NewFloatConstant(this, 10.0f));
	RateScale.SetDistribution(NewFloatConstant(this, 1.0f));
	BurstCount.SetDistribution(NewFloatConstant(this, 0.0f));
}

UParticleModuleSpawnPerUnit::UParticleModuleSpawnPerUnit()
{
	SpawnPerUnit.SetDistribution(NewFloatConstant(this, 1.0f));
}

UParticleModuleLifetime::UParticleModuleLifetime()
{
	Lifetime.SetDistribution(NewFloatUniform(this, 1.0f, 1.0f));
}

UParticleModuleLocation::UParticleModuleLocation()
{
	StartLocation.SetDistribution(NewVectorUniform(this, FVector::ZeroVector, FVector::ZeroVector));
}

UParticleModuleVelocity::UParticleModuleVelocity()
{
	StartVelocity.SetDistribution(NewVectorConstant(this, FVector(0.0f, 0.0f, 10.0f)));

}

UParticleModuleDrag::UParticleModuleDrag()
{
	DragCoefficient.SetDistribution(NewFloatConstant(this, 1.0f));
}

UParticleModulePointAttractor::UParticleModulePointAttractor()
{
	Strength.SetDistribution(NewFloatConstant(this, 1.0f));
}

UParticleModuleColor::UParticleModuleColor()
{
	StartColor.SetDistribution(NewVectorConstant(this, FVector(1.0f, 1.0f, 1.0f)));
	StartAlpha.SetDistribution(NewFloatConstant(this, 1.0f));
	EndColor.SetDistribution(NewVectorConstant(this, FVector(1.0f, 1.0f, 1.0f)));
	EndAlpha.SetDistribution(NewFloatConstant(this, 0.0f));
}

void UParticleModuleColor::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	const bool bHasEmissiveIntensity =
		Ar.IsSaving() ||
		Ar.GetPackageVersion() == 0 ||
		Ar.GetPackageVersion() >= 2;

	if (bHasEmissiveIntensity && Ar.HasProperty("EmissiveIntensity"))
	{
		Ar.BeginProperty("EmissiveIntensity");
		Ar << EmissiveIntensity;
		Ar.EndProperty();
	}
	else if (Ar.IsLoading())
	{
		EmissiveIntensity = 1.0f;
	}
}

UParticleModuleSize::UParticleModuleSize()
{
	StartSize.SetDistribution(NewVectorUniform(this, FVector::OneVector, FVector::OneVector));
	EndSize.SetDistribution(NewVectorConstant(this, FVector::OneVector));
}

void UParticleModuleLifetime::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const float LifetimeValue = std::max(0.01f, Lifetime.GetValue(Context.SpawnTime, Context.GetDistributionData()));

	Context.ParticleBase->RelativeTime = 0.0f;
	Context.ParticleBase->OneOverMaxLifetime = LifetimeValue > 0.0f ? 1.0f / LifetimeValue : 1.0f;
}

void UParticleModuleLocation::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}
	if (Context.bLockInitialLocation)
	{
		Context.ParticleBase->OldLocation = Context.ParticleBase->Location;
		return;
	}

	const FVector SpawnLocation = StartLocation.GetValue(Context.SpawnTime, Context.GetDistributionData());
	Context.ParticleBase->Location += SpawnLocation;
	Context.ParticleBase->OldLocation = Context.ParticleBase->Location;
}

void UParticleModuleVelocity::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const FVector SpawnVelocity = StartVelocity.GetValue(Context.SpawnTime, Context.GetDistributionData());
	Context.ParticleBase->Velocity = SpawnVelocity;
	Context.ParticleBase->BaseVelocity = SpawnVelocity;
}

void UParticleModuleAccelerationConstant::Update(const FUpdateContext& Context)
{
	BEGIN_UPDATE_LOOP
		Particle.BaseVelocity += Acceleration * DeltaTime;
	END_UPDATE_LOOP
}

void UParticleModuleDrag::Update(const FUpdateContext& Context)
{
	UObject* DistributionData = Context.GetDistributionData();
	BEGIN_UPDATE_LOOP
		const float RelativeTime = FMath::Clamp(Particle.RelativeTime, 0.0f, 1.0f);
		const float Coefficient = std::max(0.0f, DragCoefficient.GetValue(RelativeTime, DistributionData));
		if (Coefficient > 0.0f)
		{
			const float Damping = std::exp(-Coefficient * DeltaTime);
			Particle.BaseVelocity *= Damping;
		}
	END_UPDATE_LOOP
}

void UParticleModulePointAttractor::Update(const FUpdateContext& Context)
{
	const UParticleLODLevel* LODLevel = Context.Owner.GetCurrentLODLevel();
	const UParticleModuleRequired* RequiredModule = LODLevel ? LODLevel->GetRequiredModule() : nullptr;
	const bool bParticleDataIsLocalSpace = RequiredModule && RequiredModule->bUseLocalSpace;
	const FMatrix ComponentToWorld = Context.Owner.Component
		? Context.Owner.Component->GetWorldMatrix()
		: FMatrix::Identity;
	const FMatrix WorldToComponent = bParticleDataIsLocalSpace
		? FMatrix::Identity
		: ComponentToWorld.GetInverse();

	UObject* DistributionData = Context.GetDistributionData();
	BEGIN_UPDATE_LOOP
		const float RelativeTime = FMath::Clamp(Particle.RelativeTime, 0.0f, 1.0f);
		const float StrengthValue = std::max(0.0f, Strength.GetValue(RelativeTime, DistributionData));
		if (StrengthValue > 0.0f)
		{
			const FVector ParticlePositionLocal = bParticleDataIsLocalSpace
				? Particle.Location
				: WorldToComponent.TransformPositionWithW(Particle.Location);
			const FVector LocalAcceleration = (AttractorPosition - ParticlePositionLocal) * StrengthValue;
			const FVector AppliedAcceleration = bParticleDataIsLocalSpace
				? LocalAcceleration
				: ComponentToWorld.TransformVector(LocalAcceleration);
			Particle.BaseVelocity += AppliedAcceleration * DeltaTime;
		}
	END_UPDATE_LOOP
}

int32 UParticleModuleSpawnPerUnit::GetInstancePayloadSize() const
{
	return sizeof(FSpawnPerUnitInstancePayload);
}

void UParticleModuleSpawnPerUnit::Update(const FUpdateContext& Context)
{
	const float SpawnPerUnitValue = SpawnPerUnit.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
	if (!Context.Owner.InstanceData || UnitScalar <= 0.0f || SpawnPerUnitValue <= 0.0f)
	{
		return;
	}

	FSpawnPerUnitInstancePayload* Payload = reinterpret_cast<FSpawnPerUnitInstancePayload*>(
		Context.Owner.GetModuleInstanceData(this));
	if (!Payload)
	{
		return;
	}

	const FVector CurrentLocation = Context.Owner.GetSpawnReferenceLocation();
	if (Payload->bInitialized <= 0.0f)
	{
		Payload->PreviousLocation = CurrentLocation;
		Payload->SpawnRemainder = 0.0f;
		Payload->bInitialized = 1.0f;
		return;
	}

	FVector Travel = CurrentLocation - Payload->PreviousLocation;
	if (bIgnoreMovementAlongZ)
	{
		Travel.Z = 0.0f;
	}

	const float Distance = Travel.Length();
	const FVector PreviousLocation = Payload->PreviousLocation;
	Payload->PreviousLocation = CurrentLocation;

	if (Distance <= MovementTolerance)
	{
		return;
	}
	if (MaxFrameDistance > 0.0f && Distance > MaxFrameDistance)
	{
		Payload->SpawnRemainder = 0.0f;
		return;
	}

	float SpawnFloat = Payload->SpawnRemainder + (Distance / std::max(0.001f, UnitScalar)) * SpawnPerUnitValue;
	int32 SpawnCount = static_cast<int32>(std::floor(SpawnFloat));
	Payload->SpawnRemainder = SpawnFloat - static_cast<float>(SpawnCount);

	const int32 AvailableCount = Context.Owner.GetMaxActiveParticleCount() - Context.Owner.GetActiveParticleCount();
	SpawnCount = std::min(SpawnCount, std::max(0, AvailableCount));
	if (SpawnCount <= 0)
	{
		return;
	}

	const FVector RawTravel = CurrentLocation - PreviousLocation;
	for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
	{
		const float Alpha = static_cast<float>(SpawnIndex + 1) / static_cast<float>(SpawnCount + 1);
		const FVector SpawnLocation = Context.Owner.ConvertWorldLocationToParticleSpace(PreviousLocation + RawTravel * Alpha);
		Context.Owner.SpawnParticles(1, Context.Owner.EmitterTime, 0.0f, SpawnLocation, FVector::ZeroVector);
	}
}

void UParticleModuleColor::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const FVector ColorValue = StartColor.GetValue(Context.SpawnTime, Context.GetDistributionData());
	const float AlphaValue = StartAlpha.GetValue(Context.SpawnTime, Context.GetDistributionData());
	const FLinearColor InitialColor = ApplyEmissiveIntensity(
		ToLinearColor(ColorValue, AlphaValue),
		EmissiveIntensity);
	Context.ParticleBase->Color = InitialColor;
	Context.ParticleBase->BaseColor = InitialColor;
}

void UParticleModuleColor::Update(const FUpdateContext& Context)
{
	if (!bColorOverLife)
	{
		return;
	}

	UObject* DistributionData = Context.GetDistributionData();
	BEGIN_UPDATE_LOOP
		const float RelativeTime = FMath::Clamp(Particle.RelativeTime, 0.0f, 1.0f);
		const FVector EndColorValue = EndColor.GetValue(RelativeTime, DistributionData);
		const float EndAlphaValue = EndAlpha.GetValue(RelativeTime, DistributionData);
		const FLinearColor FinalColor = ApplyEmissiveIntensity(
			ToLinearColor(EndColorValue, EndAlphaValue),
			EmissiveIntensity);
		Particle.Color = LerpColor(Particle.BaseColor, FinalColor, RelativeTime);
	END_UPDATE_LOOP
}

void UParticleModuleSize::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const FVector InitialSize = StartSize.GetValue(Context.SpawnTime, Context.GetDistributionData());
	Context.ParticleBase->Size = InitialSize;
	Context.ParticleBase->BaseSize = InitialSize;
}

void UParticleModuleSize::Update(const FUpdateContext& Context)
{
	if (!bSizeOverLife)
	{
		return;
	}

	BEGIN_UPDATE_LOOP
		const float RelativeTime = FMath::Clamp(Particle.RelativeTime, 0.0f, 1.0f);
		const FVector EndSizeValue = EndSize.GetValue(RelativeTime, Context.GetDistributionData());
		Particle.Size = Particle.BaseSize + (EndSizeValue - Particle.BaseSize) * RelativeTime;
	END_UPDATE_LOOP
}
