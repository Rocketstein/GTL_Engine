#pragma once

#include "Core/Types/CollisionTypes.h"
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Particle/ParticleModuleCollisionBase.h"

#include "Source/Engine/Particle/ParticleModuleCollision.generated.h"

struct FHitResult;
struct FParticleEmitterInstance;
class AActor;

UCLASS()
class UParticleModuleCollision : public UParticleModuleCollisionBase
{
public:
	GENERATED_BODY()
	UParticleModuleCollision();

	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }
	int32 GetParticlePayloadSize() const override;
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;

	virtual bool PerformCollisionCheck(
		FParticleEmitterInstance& Owner,
		const FBaseParticle& Particle,
		FHitResult& Hit,
		const FVector& Start,
		const FVector& End,
		const AActor* IgnoreActor) const;

	UPROPERTY(Edit, Save, Instanced, Category="Collision", DisplayName="Damping Factor", Type=ObjectRef, AllowedClass=UDistributionVector, Member=DampingFactor.Distribution, CppType=UDistributionVector*)
	;
	FRawDistributionVector DampingFactor;

	UPROPERTY(Edit, Save, Instanced, Category="Collision", DisplayName="Damping Factor Rotation", Type=ObjectRef, AllowedClass=UDistributionVector, Member=DampingFactorRotation.Distribution, CppType=UDistributionVector*)
	;
	FRawDistributionVector DampingFactorRotation;

	UPROPERTY(Edit, Save, Instanced, Category="Collision", DisplayName="Max Collisions", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=MaxCollisions.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat MaxCollisions;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Completion Option", Enum=EParticleCollisionComplete)
	EParticleCollisionComplete CollisionCompletionOption = EParticleCollisionComplete::Kill;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Collision Type", Enum=ECollisionChannel)
	ECollisionChannel CollisionType = ECollisionChannel::WorldStatic;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Ignore Source Actor")
	bool bIgnoreSourceActor = true;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Ignore Trigger Volumes")
	bool bIgnoreTriggerVolumes = true;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Pawns Do Not Decrement Count")
	bool bPawnsDoNotDecrementCount = true;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Only Vertical Normals Decrement Count")
	bool bOnlyVerticalNormalsDecrementCount = false;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Vertical Fudge Factor", Min=0.0f, Max=1.0f, Speed=0.01f)
	float VerticalFudgeFactor = 0.1f;

	UPROPERTY(Edit, Save, Instanced, Category="Collision", DisplayName="Delay Amount", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=DelayAmount.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat DelayAmount;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Directional Scalar", Min=0.001f, Max=1000.0f, Speed=0.1f)
	float DirScalar = 3.5f;

	UPROPERTY(Edit, Save, Category="Performance", DisplayName="Max Collision Distance", Min=0.0f, Max=1000000.0f, Speed=10.0f)
	float MaxCollisionDistance = 1000.0f;

	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Apply Physics")
	bool bApplyPhysics = false;
};
