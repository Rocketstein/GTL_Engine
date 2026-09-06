#pragma once

#include "Core/Types/ResourceTypes.h"
#include "Particle/ParticleDynamicData.h"

class UMaterial;

class FParticleMaterialCache
{
public:
	~FParticleMaterialCache();

	UMaterial* ResolveParticleMaterial(const FDynamicSpriteEmitterReplayDataBase& Source) const;
	const FTextureAtlasResource* ResolveSubUVResource(const FDynamicSpriteEmitterReplayDataBase& Source) const;
	void Clear();

private:
	UMaterial* GetOrCreateSubUVAtlasMaterial(const FDynamicSpriteEmitterReplayDataBase& Source, UMaterial* BaseMaterial,
		const FTextureAtlasResource* Resource) const;

	mutable TMap<FString, UMaterial*> SubUVAtlasMaterialCache;
};
