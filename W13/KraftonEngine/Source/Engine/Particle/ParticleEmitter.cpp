#include "ParticleEmitter.h"

#include "Math/MathUtils.h"
#include "Particle/Asset/ParticleSerialization.h"
#include "Serialization/Archive.h"

#include <algorithm>

UParticleEmitter::UParticleEmitter()
{
	AddLODLevel();
	CacheEmitterModuleInfo();
}

UParticleEmitter::~UParticleEmitter()
{
	ParticleSerialization::DestroyObjectArray(LODLevels);
}

void UParticleEmitter::Serialize(FArchive& Ar)
{
	SerializeProperties(Ar, PF_Save);
	ParticleSerialization::SerializeInstancedObjectArray(Ar, LODLevels, this);

	if (Ar.IsLoading())
	{
		ReindexLODLevels();
		CacheEmitterModuleInfo();
	}
}

UParticleLODLevel* UParticleEmitter::AddLODLevel()
{
	return InsertLODLevel(static_cast<int32>(LODLevels.size()));
}

UParticleLODLevel* UParticleEmitter::InsertLODLevel(int32 Index, const UParticleLODLevel* SourceLODLevel)
{
	if (Index < 0)
	{
		Index = 0;
	}
	if (Index > static_cast<int32>(LODLevels.size()))
	{
		Index = static_cast<int32>(LODLevels.size());
	}

	UParticleLODLevel* NewLODLevel = SourceLODLevel
		? Cast<UParticleLODLevel>(SourceLODLevel->Duplicate(this))
		: UObjectManager::Get().CreateObject<UParticleLODLevel>(this);
	if (!NewLODLevel)
	{
		return nullptr;
	}

	LODLevels.insert(LODLevels.begin() + Index, NewLODLevel);
	ReindexLODLevels();
	CacheEmitterModuleInfo();
	return NewLODLevel;
}

bool UParticleEmitter::RemoveLODLevel(UParticleLODLevel* InLODLevel)
{
	auto It = std::find(LODLevels.begin(), LODLevels.end(), InLODLevel);
	if (It == LODLevels.end())
	{
		return false;
	}

	UParticleLODLevel* Removed = *It;
	LODLevels.erase(It);
	ParticleSerialization::DestroyObjectTree(Removed);
	ReindexLODLevels();
	CacheEmitterModuleInfo();
	return true;
}

bool UParticleEmitter::RemoveLODLevelAt(int32 Index)
{
	return RemoveLODLevel(GetLODLevel(Index));
}

void UParticleEmitter::ClearLODLevels()
{
	ParticleSerialization::DestroyObjectArray(LODLevels);
	CacheEmitterModuleInfo();
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	ParticleSize = sizeof(FBaseParticle);
	ParticleStride = FMath::AlignBytes(ParticleSize, 16);
	InstancePayloadSize = 0;

	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (!LODLevel)
		{
			continue;
		}

		LODLevel->RebuildModuleLists();

		int32 LODParticleSize = sizeof(FBaseParticle);
		int32 LODInstancePayloadSize = 0;
		if (LODLevel->GetTypeDataModule()
			&& LODLevel->GetTypeDataModule()->IsA<UParticleModuleTypeDataRibbon>())
		{
			LODParticleSize = FMath::AlignBytes(LODParticleSize, 16);
			LODParticleSize += sizeof(FRibbonParticlePayload);
		}

		for (UParticleModule* Module : LODLevel->GetModules())
		{
			if (!Module || !Module->IsEnabled())
			{
				continue;
			}

			const int32 ParticlePayloadSize = Module->GetParticlePayloadSize();
			if (ParticlePayloadSize > 0)
			{
				LODParticleSize = FMath::AlignBytes(LODParticleSize, 16);
				LODParticleSize += ParticlePayloadSize;
			}
			LODInstancePayloadSize += FMath::AlignBytes(Module->GetInstancePayloadSize(), 16);
		}

		const int32 LODParticleStride = FMath::AlignBytes(LODParticleSize, 16);
		ParticleSize = std::max(ParticleSize, LODParticleSize);
		ParticleStride = std::max(ParticleStride, LODParticleStride);
		InstancePayloadSize = std::max(InstancePayloadSize, LODInstancePayloadSize);
	}
}

void UParticleEmitter::ReindexLODLevels()
{
	for (int32 Index = 0; Index < static_cast<int32>(LODLevels.size()); ++Index)
	{
		if (LODLevels[Index])
		{
			LODLevels[Index]->SetLevelIndex(Index);
		}
	}
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 Index) const
{
	return Index >= 0 && Index < static_cast<int32>(LODLevels.size()) ? LODLevels[Index] : nullptr;
}

UParticleLODLevel* UParticleEmitter::GetLODLevelForIndex(int32 Index) const
{
	if (LODLevels.empty())
	{
		return nullptr;
	}

	int32 ClampedIndex = Index;
	if (ClampedIndex < 0)
	{
		ClampedIndex = 0;
	}
	if (ClampedIndex >= static_cast<int32>(LODLevels.size()))
	{
		ClampedIndex = static_cast<int32>(LODLevels.size()) - 1;
	}

	for (int32 LODIndex = ClampedIndex; LODIndex >= 0; --LODIndex)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel && LODLevel->IsEnabled())
		{
			return LODLevel;
		}
	}

	for (int32 LODIndex = ClampedIndex + 1; LODIndex < static_cast<int32>(LODLevels.size()); ++LODIndex)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel && LODLevel->IsEnabled())
		{
			return LODLevel;
		}
	}

	return nullptr;
}

UParticleLODLevel* UParticleEmitter::GetHighestEnabledLODLevel() const
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel && LODLevel->IsEnabled())
		{
			return LODLevel;
		}
	}
	return nullptr;
}
