#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Vector.h"
#include "Object/Reflection/ObjectMacros.h"

UENUM()
enum class EParticleEmitterType : uint8
{
	Sprite,
	Mesh,
	Beam,
	Ribbon,
};

UENUM()
enum class EParticleModuleType : uint8
{
	General,
	TypeData,
	Beam,
	Trail,
	Spawn,
	Required,
	Event,
	Light,
	SubUV,
	Collision,
};

UENUM()
enum class EParticleSortMode : uint8
{
	None,
	ViewDepth,
	Age,
	DistanceToCamera
};

UENUM()
enum class EParticleBlendMode : uint8
{
	Opaque,
	AlphaBlend,
	Additive,
};

UENUM()
enum class EParticleScreenAlignment : uint8
{
	PSA_FacingCameraPosition,
	PSA_Velocity,
};

UENUM()
enum class EParticleBeamEndpointMethod : uint8
{
	Emitter,
	Particle,
	UserSet,
};

UENUM()
enum class EParticleTrailRenderAxis : uint8
{
	CameraFacing,
	Velocity,
	WorldUp,
};

UENUM()
enum class ETrail2SourceMethod : uint8
{
	Default,
	Particle,
	Actor,
};

struct FBaseParticle
{
	FVector OldLocation = FVector(0.0f, 0.0f, 0.0f);
	FVector Location = FVector(0.0f, 0.0f, 0.0f);

	FVector BaseVelocity = FVector(0.0f, 0.0f, 0.0f);
	float Rotation = 0.0f;

	FVector Velocity = FVector(0.0f, 0.0f, 0.0f);
	float BaseRotationRate = 0.0f;

	FVector BaseSize = FVector(1.0f, 1.0f, 1.0f);
	float RotationRate = 0.0f;

	FVector Size = FVector(1.0f, 1.0f, 1.0f);
	uint32 Flags = 0;

	FLinearColor Color = FLinearColor::White();
	FLinearColor BaseColor = FLinearColor::White();

	float RelativeTime = 0.0f;
	float OneOverMaxLifetime = 1.0f;
	float Placeholder0 = 0.0f;
	float Placeholder1 = 0.0f;
	uint32 ParticleId = 0;
};

struct FRibbonParticlePayload
{
	uint32 SpawnSerial = 0;
	uint32 SourceParticleId = 0;
	int32 TrailIndex = 0;
};

enum EParticleStates
{
	/** Ignore updates to the particle						*/
	STATE_Particle_JustSpawned = 0x02000000,
	/** Ignore updates to the particle						*/
	STATE_Particle_Freeze = 0x04000000,
	/** Ignore collision updates to the particle			*/
	STATE_Particle_IgnoreCollisions = 0x08000000,
	/**	Stop translations of the particle					*/
	STATE_Particle_FreezeTranslation = 0x10000000,
	/**	Stop rotations of the particle						*/
	STATE_Particle_FreezeRotation = 0x20000000,
	/** Combination for a single check of 'ignore' flags	*/
	STATE_Particle_CollisionIgnoreCheck = STATE_Particle_Freeze | STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation | STATE_Particle_FreezeRotation,
	/** Delay collision updates to the particle				*/
	STATE_Particle_DelayCollisions = 0x40000000,
	/** Flag indicating the particle has had at least one collision	*/
	STATE_Particle_CollisionHasOccurred = 0x80000000,
	/** State mask. */
	STATE_Mask = 0xFE000000,
	/** Counter mask. */
	STATE_CounterMask = (~STATE_Mask)
};

struct FParticleModuleCache
{
	int32 ParticlePayloadOffset = 0;
	int32 ParticlePayloadSize = 0;
	int32 InstancePayloadOffset = 0;
	int32 InstancePayloadSize = 0;
};

struct FParticleSubUVPayload
{
	float ImageIndex = 0.0f;
};

constexpr int32 MaxParticleBeamNoisePoints = 64;

struct FParticleBeamNoisePoint
{
	FVector CurrentOffset = FVector::ZeroVector;
	FVector SourceOffset = FVector::ZeroVector;
	FVector TargetOffset = FVector::ZeroVector;
	float LockTimer = 0.0f;
};

struct FParticleBeamNoisePayload
{
	uint32 RandomSeed = 0;
	int32 Frequency = 0;
	float Strength = 0.0f;
	float NoiseSpeed = 0.0f;
	float NoiseLockTime = 0.0f;
	FParticleBeamNoisePoint Points[MaxParticleBeamNoisePoints];
};
