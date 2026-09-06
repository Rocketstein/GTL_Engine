#pragma once

#include "Core/Types/CoreTypes.h"

class UPhysicsAsset;

// PhysicsAsset(.uasset) 로드/세이브. 다른 에셋 매니저(FParticleSystemManager 등)와 동일 패턴.
class FPhysicsAssetManager
{
public:
	static FPhysicsAssetManager& Get();

	UPhysicsAsset* Load(const FString& Path);
	bool           Save(UPhysicsAsset* Asset);

private:
	FPhysicsAssetManager() = default;
};
