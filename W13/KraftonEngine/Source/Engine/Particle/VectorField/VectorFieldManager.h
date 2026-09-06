#pragma once

#include "Asset/AssetRegistry.h"
#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"

class UVectorFieldAsset;

class FVectorFieldManager : public TSingleton<FVectorFieldManager>
{
	friend class TSingleton<FVectorFieldManager>;

public:
	UVectorFieldAsset* Load(const FString& Path);
	UVectorFieldAsset* Find(const FString& Path) const;

	bool Save(UVectorFieldAsset* Asset, const struct FAssetImportMetadata* MetadataOverride = nullptr);
	bool ImportFga(const FString& SourceFgaPath, FString& OutPackagePath, UVectorFieldAsset** OutAsset = nullptr, FString* OutError = nullptr);
	bool Reimport(const FString& PackagePath, UVectorFieldAsset** OutAsset = nullptr, FString* OutError = nullptr);

	void RefreshAvailableVectorFields();
	const TArray<FAssetListItem>& GetAvailableVectorFieldFiles() const { return AvailableVectorFieldFiles; }

private:
	TMap<FString, UVectorFieldAsset*> LoadedVectorFields;
	TArray<FAssetListItem> AvailableVectorFieldFiles;
};
