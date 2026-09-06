#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Particle/ParticleEvent.generated.h"

UENUM()
enum class EParticleEventType : uint8
{
	Any,
	Spawn,
	Death,
	Collision,
	Burst,
	Kismet,
};

USTRUCT()
struct FParticleEventData
{
	GENERATED_BODY()

	bool Matches(EParticleEventType InType, FName InEventName) const;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Type", Enum=EParticleEventType)
	EParticleEventType Type = EParticleEventType::Any;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Event Name")
	FName EventName;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Emitter Time")
	float EmitterTime = 0.0f;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Particle Time")
	float ParticleTime = 0.0f;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Particle Count")
	int32 ParticleCount = 0;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Location")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Velocity")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Direction")
	FVector Direction = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Normal")
	FVector Normal = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Particle Event", DisplayName="Bone Name")
	FName BoneName;
};
