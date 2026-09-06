#include "ParticleDynamicData.h"

#include "Particle/ParticleModule.h"

#include <algorithm>
#include <cstring>

void FParticleDataContainer::Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
{
	Free();

	ParticleDataNumBytes = FMath::AlignBytes(std::max(0, InParticleDataNumBytes), 16);
	ParticleIndicesNumShorts = std::max(0, InParticleIndicesNumShorts);
	MemBlockSize = ParticleDataNumBytes + ParticleIndicesNumShorts * static_cast<int32>(sizeof(uint16));

	if (MemBlockSize <= 0)
	{
		return;
	}

	MemBlock.assign(static_cast<size_t>(MemBlockSize), 0);
	ParticleData = MemBlock.data();
	ParticleIndices = ParticleIndicesNumShorts > 0
		? reinterpret_cast<uint16*>(ParticleData + ParticleDataNumBytes)
		: nullptr;
}

void FParticleDataContainer::Free()
{
	MemBlock.clear();
	MemBlockSize = 0;
	ParticleDataNumBytes = 0;
	ParticleIndicesNumShorts = 0;
	ParticleData = nullptr;
	ParticleIndices = nullptr;
}

void FParticleDataContainer::CopyFrom(const uint8* InParticleData, int32 InParticleDataNumBytes, const uint16* InParticleIndices, int32 InParticleIndicesNumShorts)
{
	Alloc(InParticleDataNumBytes, InParticleIndicesNumShorts);

	if (ParticleData && InParticleData && InParticleDataNumBytes > 0)
	{
		std::memcpy(ParticleData, InParticleData, static_cast<size_t>(InParticleDataNumBytes));
	}
	if (ParticleIndices && InParticleIndices && InParticleIndicesNumShorts > 0)
	{
		std::memcpy(ParticleIndices, InParticleIndices, static_cast<size_t>(InParticleIndicesNumShorts) * sizeof(uint16));
	}
}

FDynamicSpriteEmitterReplayDataBase::FDynamicSpriteEmitterReplayDataBase(const UParticleModuleRequired* InRequiredModule)
	: RequiredModule(InRequiredModule)
{
	EmitterType = EDynamicEmitterType::Sprite;

	if (!RequiredModule)
	{
		MaterialPath = ParticleDefaults::DefaultSpriteMaterialPath;
		BlendMode = EParticleBlendMode::AlphaBlend;
		ScreenAlignment = EParticleScreenAlignment::PSA_FacingCameraPosition;
		return;
	}

	SortMode = RequiredModule->SortMode;
	TranslucencySortPriority = RequiredModule->TranslucencySortPriority;
	bUseLocalSpace = RequiredModule->bUseLocalSpace;
	MaterialPath = RequiredModule->MaterialPath.ToString();
	BlendMode = RequiredModule->BlendMode;
	ScreenAlignment = RequiredModule->ScreenAlignment;
}

FDynamicSpriteEmitterDataBase::FDynamicSpriteEmitterDataBase(const UParticleModuleRequired* RequiredModule)
	: Source(RequiredModule)
{
}

FDynamicSpriteEmitterData::FDynamicSpriteEmitterData(const UParticleModuleRequired* RequiredModule)
	: FDynamicSpriteEmitterDataBase(RequiredModule)
{
	Source.EmitterType = EDynamicEmitterType::Sprite;
}

FDynamicMeshEmitterData::FDynamicMeshEmitterData(const UParticleModuleRequired* RequiredModule)
	: FDynamicSpriteEmitterData(RequiredModule)
{
	Source.EmitterType = EDynamicEmitterType::Mesh;
}

FDynamicBeamEmitterReplayData::FDynamicBeamEmitterReplayData(const UParticleModuleRequired* RequiredModule)
	: FDynamicSpriteEmitterReplayDataBase(RequiredModule)
{
	EmitterType = EDynamicEmitterType::Beam;
}

FDynamicTrailsEmitterReplayData::FDynamicTrailsEmitterReplayData(const UParticleModuleRequired* RequiredModule)
	: FDynamicSpriteEmitterReplayDataBase(RequiredModule)
{
}

FDynamicRibbonEmitterReplayData::FDynamicRibbonEmitterReplayData(const UParticleModuleRequired* RequiredModule)
	: FDynamicTrailsEmitterReplayData(RequiredModule)
{
	EmitterType = EDynamicEmitterType::Ribbon;
}

FDynamicBeamEmitterData::FDynamicBeamEmitterData(const UParticleModuleRequired* RequiredModule)
	: FDynamicSpriteEmitterDataBase(RequiredModule)
	, Source(RequiredModule)
{
}

FDynamicTrailsEmitterData::FDynamicTrailsEmitterData(const UParticleModuleRequired* RequiredModule)
	: FDynamicSpriteEmitterDataBase(RequiredModule)
{
}

FDynamicRibbonEmitterData::FDynamicRibbonEmitterData(const UParticleModuleRequired* RequiredModule)
	: FDynamicTrailsEmitterData(RequiredModule)
	, Source(RequiredModule)
{
}
