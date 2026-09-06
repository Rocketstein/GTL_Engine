#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Physics/Cloth/ClothAsset.h"

class UMaterial;
class USkeletalMesh;
class UStaticMesh;

struct FClothAssetBuildOptions
{
	bool bBuildDefaultPinnedGrid32x32 = true;
	bool bWeldVertices = true;
	float WeldPositionTolerance = 0.001f;
	bool bBuildBendConstraints = true;
};

class FClothAssetBuilder
{
public:
	static bool BuildFromRawMesh(
		const TArray<FVector>& Positions,
		const TArray<FVector4>& Colors,
		const TArray<FVector2>& UVs,
		const TArray<uint32>& Indices,
		UMaterial* Material,
		UClothAsset& OutAsset,
		const FClothAssetBuildOptions& Options,
		FString* OutError = nullptr);

	static bool BuildFromStaticMesh(
		const UStaticMesh* SourceMesh,
		UClothAsset& OutAsset,
		const FClothAssetBuildOptions& Options,
		FString* OutError = nullptr);

	static bool BuildFromSkeletalMesh(
		const USkeletalMesh* SourceMesh,
		UClothAsset& OutAsset,
		const FClothAssetBuildOptions& Options,
		FString* OutError = nullptr);

	static uint32 RebuildTethersFromPins(UClothAsset& Asset, float LengthScale = 1.0f);
	static uint32 AppendBendConstraintsFromTriangles(UClothAsset& Asset);
};
