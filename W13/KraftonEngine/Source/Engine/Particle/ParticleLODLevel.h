#pragma once

#include "Object/Object.h"
#include "Particle/ParticleModule.h"

#include "Source/Engine/Particle/ParticleLODLevel.generated.h"

class FArchive;

UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY()
	UParticleLODLevel();
	~UParticleLODLevel() override;

	void Serialize(FArchive& Ar) override;

	UParticleModuleRequired* GetRequiredModule() const { return RequiredModule; }
	UParticleModuleTypeDataBase* GetTypeDataModule() const { return TypeDataModule; }
	const TArray<UParticleModule*>& GetModules() const { return Modules; }
	const TArray<UParticleModule*>& GetSpawnModules() const { return SpawnModules; }
	const TArray<UParticleModule*>& GetUpdateModules() const { return UpdateModules; }
	const TArray<UParticleModule*>& GetEventGeneratorModules() const { return EventGeneratorModules; }
	const TArray<UParticleModule*>& GetEventReceiverModules() const { return EventReceiverModules; }

	void SetRequiredModule(UParticleModuleRequired* InModule);
	void SetTypeDataModule(UParticleModuleTypeDataBase* InModule);
	void AddModule(UParticleModule* InModule);
	void InsertModule(UParticleModule* InModule, int32 Index);
	bool DetachModule(UParticleModule* InModule);
	bool RemoveModule(UParticleModule* InModule);
	void ClearModules();
	void EnsureFixedModules();
	void RebuildModuleLists();
	bool IsFixedModule(const UParticleModule* InModule) const;
	int32 GetFirstMovableModuleIndex() const;

	int32 GetLevel() const { return Level; }
	void SetLevelIndex(int32 InLevel);
	bool IsEnabled() const { return bEnabled; }

private:
	UPROPERTY(Save, Category="LOD", DisplayName="Level")
	int32 Level = 0;

	UPROPERTY(Edit, Save, Category="LOD", DisplayName="Enabled")
	bool bEnabled = true;

	UPROPERTY(Edit, Category="Modules", DisplayName="Required Module", Type=ObjectRef, AllowedClass=UParticleModuleRequired)
	UParticleModuleRequired* RequiredModule = nullptr;

	UPROPERTY(Edit, Category="Modules", DisplayName="Type Data Module", Type=ObjectRef, AllowedClass=UParticleModuleTypeDataBase)
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

	UPROPERTY(Edit, Category="Modules", DisplayName="Modules", AllowedClass=UParticleModule)
	TArray<UParticleModule*> Modules;

	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
	TArray<UParticleModule*> EventGeneratorModules;
	TArray<UParticleModule*> EventReceiverModules;
};
