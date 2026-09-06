#pragma once

#include "Particle/ParticleModule.h"

#include "Source/Engine/Particle/ParticleModuleCollisionBase.generated.h"

UENUM()
enum class EParticleCollisionComplete : uint8
{
	Kill,
	Freeze,
	HaltCollisions,
	FreezeTranslation,
	FreezeRotation,
	FreezeMovement,
};

UCLASS()
class UParticleModuleCollisionBase : public UParticleModule
{
public:
	GENERATED_BODY()
	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Collision; }
};
