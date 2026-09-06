#include "Physics/Asset/PhysicsAssetManager.h"

#include "Physics/Asset/PhysicsAsset.h"
#include "Asset/AssetPackage.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

FPhysicsAssetManager& FPhysicsAssetManager::Get()
{
	static FPhysicsAssetManager Instance;
	return Instance;
}

UPhysicsAsset* FPhysicsAssetManager::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	if (!FAssetPackage::IsAssetPackagePath(NormalizedPath))
	{
		return nullptr;
	}

	FWindowsBinReader Ar(NormalizedPath);
	if (!Ar.IsValid())
	{
		return nullptr;
	}

	FAssetPackageHeader Header;
	Ar << Header;
	if (!Header.IsValid(EAssetPackageType::PhysicsAsset))
	{
		return nullptr;
	}

	FAssetImportMetadata Metadata;
	Ar << Metadata;
	Ar.SetPackageVersion(Header.Version);

	UPhysicsAsset* NewAsset = UObjectManager::Get().CreateObject<UPhysicsAsset>();
	NewAsset->Serialize(Ar);

	if (!Ar.IsValid())
	{
		UObjectManager::Get().DestroyObject(NewAsset);
		return nullptr;
	}

	NewAsset->SetSourcePath(NormalizedPath);
	return NewAsset;
}

bool FPhysicsAssetManager::Save(UPhysicsAsset* Asset)
{
	if (!Asset)
	{
		return false;
	}

	const FString& Path = Asset->GetSourcePath();
	if (Path.empty())
	{
		return false;
	}

	FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
	if (!Ar.IsValid())
	{
		return false;
	}

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::PhysicsAsset);

	FAssetImportMetadata Metadata;

	Ar << Header;
	Ar << Metadata;
	Ar.SetPackageVersion(Header.Version);
	Asset->Serialize(Ar);

	return Ar.IsValid();
}
