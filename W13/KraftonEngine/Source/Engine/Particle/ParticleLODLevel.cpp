#include "ParticleLODLevel.h"

#include "Particle/Asset/ParticleSerialization.h"
#include "Particle/ParticleModuleEvent.h"
#include "Serialization/Archive.h"

#include <algorithm>

UParticleLODLevel::UParticleLODLevel()
{
	RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>(this);
	TypeDataModule = UObjectManager::Get().CreateObject<UParticleModuleTypeDataSprite>(this);
	Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleSpawn>(this));
	RebuildModuleLists();
}

UParticleLODLevel::~UParticleLODLevel()
{
	ParticleSerialization::DestroyObjectTree(RequiredModule);
	ParticleSerialization::DestroyObjectTree(TypeDataModule);
	ParticleSerialization::DestroyObjectArray(Modules);
	SpawnModules.clear();
	UpdateModules.clear();
	EventGeneratorModules.clear();
	EventReceiverModules.clear();
}

void UParticleLODLevel::Serialize(FArchive& Ar)
{
	SerializeProperties(Ar, PF_Save);
	ParticleSerialization::SerializeInstancedObject(Ar, RequiredModule, this);
	ParticleSerialization::SerializeInstancedObject(Ar, TypeDataModule, this);
	ParticleSerialization::SerializeInstancedObjectArray(Ar, Modules, this);

	if (Ar.IsLoading())
	{
		EnsureFixedModules();
		RebuildModuleLists();
	}
}

void UParticleLODLevel::SetRequiredModule(UParticleModuleRequired* InModule)
{
	if (RequiredModule == InModule)
	{
		return;
	}

	ParticleSerialization::DestroyObjectTree(RequiredModule);
	RequiredModule = InModule;
	if (RequiredModule)
	{
		RequiredModule->SetOuter(this);
	}
}

void UParticleLODLevel::SetTypeDataModule(UParticleModuleTypeDataBase* InModule)
{
	if (TypeDataModule == InModule)
	{
		return;
	}

	ParticleSerialization::DestroyObjectTree(TypeDataModule);
	TypeDataModule = InModule;
	if (TypeDataModule)
	{
		TypeDataModule->SetOuter(this);
	}
}

void UParticleLODLevel::AddModule(UParticleModule* InModule)
{
	if (!InModule)
	{
		return;
	}

	InsertModule(InModule, static_cast<int32>(Modules.size()));
}

void UParticleLODLevel::InsertModule(UParticleModule* InModule, int32 Index)
{
	if (!InModule)
	{
		return;
	}

	const int32 FirstMovableIndex = GetFirstMovableModuleIndex();
	if (Index < FirstMovableIndex)
	{
		Index = FirstMovableIndex;
	}
	if (Index > static_cast<int32>(Modules.size()))
	{
		Index = static_cast<int32>(Modules.size());
	}

	InModule->SetOuter(this);
	Modules.insert(Modules.begin() + Index, InModule);
	RebuildModuleLists();
}

bool UParticleLODLevel::DetachModule(UParticleModule* InModule)
{
	if (!InModule || IsFixedModule(InModule))
	{
		return false;
	}

	auto It = std::find(Modules.begin(), Modules.end(), InModule);
	if (It == Modules.end())
	{
		return false;
	}

	Modules.erase(It);
	RebuildModuleLists();
	return true;
}

bool UParticleLODLevel::RemoveModule(UParticleModule* InModule)
{
	if (IsFixedModule(InModule))
	{
		return false;
	}

	auto It = std::find(Modules.begin(), Modules.end(), InModule);
	if (It == Modules.end())
	{
		return false;
	}

	UParticleModule* Removed = *It;
	Modules.erase(It);
	ParticleSerialization::DestroyObjectTree(Removed);
	RebuildModuleLists();
	return true;
}

void UParticleLODLevel::ClearModules()
{
	ParticleSerialization::DestroyObjectArray(Modules);
	Modules.push_back(UObjectManager::Get().CreateObject<UParticleModuleSpawn>(this));
	RebuildModuleLists();
}

void UParticleLODLevel::EnsureFixedModules()
{
	if (!RequiredModule)
	{
		RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>(this);
	}
	if (RequiredModule)
	{
		RequiredModule->SetOuter(this);
	}

	if (!TypeDataModule)
	{
		TypeDataModule = UObjectManager::Get().CreateObject<UParticleModuleTypeDataSprite>(this);
	}
	if (TypeDataModule)
	{
		TypeDataModule->SetOuter(this);
	}

	TArray<UParticleModule*> FixedSpawnModules;
	TArray<UParticleModule*> MovableModules;
	for (UParticleModule* Module : Modules)
	{
		if (UParticleModuleSpawn* Candidate = Cast<UParticleModuleSpawn>(Module))
		{
			FixedSpawnModules.push_back(Candidate);
		}
		else if (Module)
		{
			MovableModules.push_back(Module);
		}
	}
	if (FixedSpawnModules.empty())
	{
		FixedSpawnModules.push_back(UObjectManager::Get().CreateObject<UParticleModuleSpawn>(this));
	}

	Modules.clear();
	Modules.insert(Modules.end(), FixedSpawnModules.begin(), FixedSpawnModules.end());
	Modules.insert(Modules.end(), MovableModules.begin(), MovableModules.end());

	for (UParticleModule* Module : Modules)
	{
		if (Module)
		{
			Module->SetOuter(this);
		}
	}
}

void UParticleLODLevel::RebuildModuleLists()
{
	EnsureFixedModules();

	SpawnModules.clear();
	UpdateModules.clear();
	EventGeneratorModules.clear();
	EventReceiverModules.clear();

	for (UParticleModule* Module : Modules)
	{
		if (!Module || !Module->IsEnabled())
		{
			continue;
		}

		const bool bIsEventReceiver = Module->IsA<UParticleModuleEventReceiverBase>();

		if (Module->IsSpawnModule())
		{
			SpawnModules.push_back(Module);
		}
		if (Module->IsUpdateModule() && !bIsEventReceiver)
		{
			UpdateModules.push_back(Module);
		}
		if (Module->IsA<UParticleModuleEventGenerator>())
		{
			EventGeneratorModules.push_back(Module);
		}
		if (bIsEventReceiver)
		{
			EventReceiverModules.push_back(Module);
		}
	}
}

void UParticleLODLevel::SetLevelIndex(int32 InLevel)
{
	Level = InLevel < 0 ? 0 : InLevel;
}

bool UParticleLODLevel::IsFixedModule(const UParticleModule* InModule) const
{
	return InModule && InModule->IsA<UParticleModuleSpawn>();
}

int32 UParticleLODLevel::GetFirstMovableModuleIndex() const
{
	int32 Index = 0;
	while (Index < static_cast<int32>(Modules.size()))
	{
		const UParticleModule* Module = Modules[Index];
		if (!Module || !IsFixedModule(Module))
		{
			break;
		}
		++Index;
	}
	return Index;
}
