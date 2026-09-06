#include "Render/Proxy/Particle/ParticleMaterialCache.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Particle/ParticleModule.h"
#include "Resource/ResourceManager.h"
#include "Render/Types/MaterialTextureSlot.h"

namespace
{
	bool IsNonePath(const FString& Path)
	{
		return Path.empty() || Path == "None";
	}
}

FParticleMaterialCache::~FParticleMaterialCache()
{
	Clear();
}

void FParticleMaterialCache::Clear()
{
	for (auto& Pair : SubUVAtlasMaterialCache)
	{
		if (Pair.second)
		{
			UObjectManager::Get().DestroyObject(Pair.second);
		}
	}
	SubUVAtlasMaterialCache.clear();
}

UMaterial* FParticleMaterialCache::ResolveParticleMaterial(const FDynamicSpriteEmitterReplayDataBase& Source) const
{
	FString MaterialPath = Source.MaterialPath;
	if (IsNonePath(MaterialPath))
	{
		MaterialPath = ParticleDefaults::DefaultSpriteMaterialPath;
	}

	UMaterial* BaseMaterial = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
	if (const FTextureAtlasResource* SubUVResource = ResolveSubUVResource(Source))
	{
		return GetOrCreateSubUVAtlasMaterial(Source, BaseMaterial, SubUVResource);
	}

	return BaseMaterial;
}

const FTextureAtlasResource* FParticleMaterialCache::ResolveSubUVResource(const FDynamicSpriteEmitterReplayDataBase& Source) const
{
	if (!Source.bUseSubUV || IsNonePath(Source.SubUVResourceName))
	{
		return nullptr;
	}

	const FTextureAtlasResource* Resource = FResourceManager::Get().FindSubUVResource(FName(Source.SubUVResourceName.c_str()));
	if (!Resource || !Resource->IsLoaded())
	{
		return nullptr;
	}

	return Resource;
}

UMaterial* FParticleMaterialCache::GetOrCreateSubUVAtlasMaterial(const FDynamicSpriteEmitterReplayDataBase& Source, UMaterial* BaseMaterial,
	const FTextureAtlasResource* Resource) const
{
	if (!BaseMaterial || !Resource || !Resource->IsLoaded())
	{
		return BaseMaterial;
	}

	const FString CacheKey = Source.MaterialPath + "|SubUVAtlas|" + Resource->Name.ToString();
	auto It = SubUVAtlasMaterialCache.find(CacheKey);
	UMaterial* AtlasMaterial = It != SubUVAtlasMaterialCache.end() ? It->second : nullptr;
	if (!AtlasMaterial)
	{
		AtlasMaterial = UMaterial::CreateTransient(
			BaseMaterial->GetRenderPass(),
			BaseMaterial->GetBlendState(),
			BaseMaterial->GetDepthStencilState(),
			BaseMaterial->GetRasterizerState(),
			BaseMaterial->GetShader());
		AtlasMaterial->SetAssetPathFileName(CacheKey);
		SubUVAtlasMaterialCache[CacheKey] = AtlasMaterial;
	}

	const ID3D11ShaderResourceView* const* BaseSRVs = BaseMaterial->GetCachedSRVs();
	for (int32 Slot = 0; Slot < static_cast<int32>(EMaterialTextureSlot::Max); ++Slot)
	{
		AtlasMaterial->SetCachedSRV(static_cast<EMaterialTextureSlot>(Slot), const_cast<ID3D11ShaderResourceView*>(BaseSRVs[Slot]));
	}
	AtlasMaterial->SetCachedSRV(EMaterialTextureSlot::Diffuse, Resource->SRV);
	return AtlasMaterial;
}
