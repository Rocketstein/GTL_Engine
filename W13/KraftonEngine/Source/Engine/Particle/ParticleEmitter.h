#pragma once

#include "Object/FName.h"
#include "Object/Object.h"
#include "Particle/ParticleLODLevel.h"

#include "Source/Engine/Particle/ParticleEmitter.generated.h"

class FArchive;

UCLASS()
class UParticleEmitter : public UObject
{
public:
	GENERATED_BODY()
	UParticleEmitter();
	~UParticleEmitter() override;

	void Serialize(FArchive& Ar) override;

	UParticleLODLevel* AddLODLevel();
	UParticleLODLevel* InsertLODLevel(int32 Index, const UParticleLODLevel* SourceLODLevel = nullptr);
	bool RemoveLODLevel(UParticleLODLevel* InLODLevel);
	bool RemoveLODLevelAt(int32 Index);
	void ClearLODLevels();
	void CacheEmitterModuleInfo();
	void ReindexLODLevels();

	UParticleLODLevel* GetLODLevel(int32 Index) const;
	UParticleLODLevel* GetLODLevelForIndex(int32 Index) const;
	UParticleLODLevel* GetHighestEnabledLODLevel() const;
	const TArray<UParticleLODLevel*>& GetLODLevels() const { return LODLevels; }
	int32 GetLODLevelCount() const { return static_cast<int32>(LODLevels.size()); }

	const FName& GetEmitterName() const { return EmitterName; }
	int32 GetParticleSize() const { return ParticleSize; }
	int32 GetParticleStride() const { return ParticleStride; }
	int32 GetInstancePayloadSize() const { return InstancePayloadSize; }
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

private:
	UPROPERTY(Edit, Save, Category="Emitter", DisplayName="Name")
	FName EmitterName = FName("Emitter");

	UPROPERTY(Edit, Save, Category="Emitter", DisplayName="Enabled")
	bool bEnabled = true;

	UPROPERTY(Edit, Category="LOD", DisplayName="LOD Levels", AllowedClass=UParticleLODLevel)
	TArray<UParticleLODLevel*> LODLevels;

	int32 ParticleSize = sizeof(FBaseParticle);
	int32 ParticleStride = sizeof(FBaseParticle);
	int32 InstancePayloadSize = 0;
};
