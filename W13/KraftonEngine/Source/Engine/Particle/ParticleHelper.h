#pragma once

#include "Particle/ParticleTypes.h"

#define DECLARE_PARTICLE(Name, Address) \
	FBaseParticle& Name = *reinterpret_cast<FBaseParticle*>(Address);

#define DECLARE_PARTICLE_CONST(Name, Address) \
	const FBaseParticle& Name = *reinterpret_cast<const FBaseParticle*>(Address);

#define DECLARE_PARTICLE_PTR(Name, Address) \
	FBaseParticle* Name = reinterpret_cast<FBaseParticle*>(Address);

#define BEGIN_UPDATE_LOOP \
	{ \
		int32& ActiveParticles = Context.Owner.ActiveParticles; \
		const int32 Offset = Context.Offset; \
		uint32 CurrentOffset = static_cast<uint32>(Offset); \
		const float DeltaTime = Context.DeltaTime; \
		uint8* ParticleData = Context.Owner.ParticleData; \
		const int32 ParticleStride = Context.Owner.ParticleStride; \
		uint16* ParticleIndices = Context.Owner.ParticleIndices; \
		for (int32 i = ActiveParticles - 1; i >= 0; --i) \
		{ \
			const int32 CurrentIndex = ParticleIndices[i]; \
			uint8* ParticleBase = ParticleData + static_cast<size_t>(CurrentIndex) * ParticleStride; \
			FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(ParticleBase); \
			if ((Particle.Flags & STATE_Particle_Freeze) == 0) \
			{

#define END_UPDATE_LOOP \
			} \
			CurrentOffset = static_cast<uint32>(Offset); \
		} \
	}

#define CONTINUE_UPDATE_LOOP \
	CurrentOffset = static_cast<uint32>(Offset); \
	continue;

#define SPAWN_INIT \
	const int32 ActiveParticles = Context.Owner.ActiveParticles; \
	const int32 ParticleStride = Context.Owner.ParticleStride; \
	uint32 CurrentOffset = static_cast<uint32>(Context.Offset); \
	FBaseParticle* ParticleBase = Context.ParticleBase; \
	FBaseParticle& Particle = *ParticleBase; \
	(void)ActiveParticles; \
	(void)ParticleStride;

#define PARTICLE_ELEMENT(Type, Name) \
	Type& Name = *reinterpret_cast<Type*>(reinterpret_cast<uint8*>(ParticleBase) + CurrentOffset); \
	CurrentOffset += sizeof(Type);


#define KILL_CURRENT_PARTICLE																							\
	{																													\
		ParticleIndices[i]					= ParticleIndices[ActiveParticles-1];										\
		ParticleIndices[ActiveParticles-1]	= CurrentIndex;																\
		ActiveParticles--;																								\
	}
