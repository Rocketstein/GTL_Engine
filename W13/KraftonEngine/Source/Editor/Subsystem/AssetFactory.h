#pragma once

#include "Core/Types/CoreTypes.h"

class FAssetFactory
{
public:
	static bool CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateCameraShake(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateAnimGraph(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateParticleSystem(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateVectorField(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath, int32 SizeX = 16, int32 SizeY = 16, int32 SizeZ = 16);
	static bool CreateClothAsset(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
};
