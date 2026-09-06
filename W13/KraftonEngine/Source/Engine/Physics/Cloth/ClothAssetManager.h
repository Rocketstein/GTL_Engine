#pragma once

#include "Asset/AssetRegistry.h"
#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Physics/Cloth/ClothAssetBuilder.h"

class UClothAsset;
class USkeletalMesh;
class UStaticMesh;
class UMaterial;
struct FImportOptions;

class FClothAssetManager : public TSingleton<FClothAssetManager>
{
	friend class TSingleton<FClothAssetManager>;

public:
	UClothAsset* Load(const FString& Path);
	UClothAsset* Find(const FString& Path) const;
	bool UpgradeLegacyDefaultQuadTo32x32(UClothAsset* Asset);

	bool Save(UClothAsset* Asset, const struct FAssetImportMetadata* MetadataOverride = nullptr);

	bool CreateFromRawMesh(
		const FString& SourcePath,
		const TArray<FVector>& Positions,
		const TArray<FVector4>& Colors,
		const TArray<FVector2>& UVs,
		const TArray<uint32>& Indices,
		UMaterial* Material,
		FString& OutPackagePath,
		UClothAsset** OutAsset = nullptr,
		FString* OutError = nullptr,
		const FClothAssetBuildOptions& Options = FClothAssetBuildOptions());

	bool CreateFromMeshSourceFile(
		const FString& SourcePath,
		const FImportOptions& ImportOptions,
		FString& OutPackagePath,
		UClothAsset** OutAsset = nullptr,
		FString* OutError = nullptr,
		const FClothAssetBuildOptions& BuildOptions = FClothAssetBuildOptions());

	bool ReplaceFromRawMesh(
		UClothAsset* TargetAsset,
		const FString& SourcePath,
		const TArray<FVector>& Positions,
		const TArray<FVector4>& Colors,
		const TArray<FVector2>& UVs,
		const TArray<uint32>& Indices,
		UMaterial* Material,
		FString* OutError = nullptr,
		const FClothAssetBuildOptions& Options = FClothAssetBuildOptions());

	bool ReplaceFromMeshSourceFile(
		UClothAsset* TargetAsset,
		const FString& SourcePath,
		const FImportOptions& ImportOptions,
		FString* OutError = nullptr,
		const FClothAssetBuildOptions& BuildOptions = FClothAssetBuildOptions());

	bool CreateFromStaticMesh(
		UStaticMesh* SourceMesh,
		FString& OutPackagePath,
		UClothAsset** OutAsset = nullptr,
		FString* OutError = nullptr,
		const FClothAssetBuildOptions& Options = FClothAssetBuildOptions());

	bool CreateFromSkeletalMesh(
		USkeletalMesh* SourceMesh,
		FString& OutPackagePath,
		UClothAsset** OutAsset = nullptr,
		FString* OutError = nullptr,
		const FClothAssetBuildOptions& Options = FClothAssetBuildOptions());

	void RefreshAvailableClothAssets();
	const TArray<FAssetListItem>& GetAvailableClothAssetFiles() const { return AvailableClothAssetFiles; }

private:
	FClothAssetManager() = default;

	TMap<FString, UClothAsset*> LoadedClothAssets;
	TArray<FAssetListItem> AvailableClothAssetFiles;
};
