#include "Physics/Cloth/ClothAssetManager.h"

#include "Asset/AssetPackage.h"
#include "Core/Logging/Log.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Mesh/Importer/FbxImporter.h"
#include "Mesh/Importer/MeshImportOptions.h"
#include "Mesh/Importer/ObjImporter.h"
#include "Mesh/Importer/Fbx/FbxImportTypes.h"
#include "Object/Object.h"
#include "Physics/Cloth/ClothAsset.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace
{
	void SetError(FString* OutError, const FString& Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
	}

	FString NormalizeProjectPath(const FString& Path)
	{
		return FPaths::MakeProjectRelative(Path);
	}

	std::filesystem::path ResolveProjectPath(const FString& Path)
	{
		std::filesystem::path FullPath(FPaths::ToWide(Path));
		if (!FullPath.is_absolute())
		{
			FullPath = std::filesystem::path(FPaths::RootDir()) / FullPath;
		}
		return FullPath.lexically_normal();
	}

	bool BuildSourceMetadata(const FString& SourcePath, FAssetImportMetadata& OutMetadata)
	{
		OutMetadata.SourcePath = NormalizeProjectPath(SourcePath);

		const std::filesystem::path FullPath = ResolveProjectPath(SourcePath);
		if (!std::filesystem::exists(FullPath) || !std::filesystem::is_regular_file(FullPath))
		{
			return false;
		}

		OutMetadata.SourceFileSize = static_cast<uint64>(std::filesystem::file_size(FullPath));
		OutMetadata.SourceTimestamp = static_cast<uint64>(std::filesystem::last_write_time(FullPath).time_since_epoch().count());
		return true;
	}

	FString BuildUniqueClothAssetPath(const FString& SourceAssetPath)
	{
		std::filesystem::path SourcePath(FPaths::ToWide(NormalizeProjectPath(SourceAssetPath)));
		if (SourcePath.empty() || SourcePath.wstring() == L"None")
		{
			SourcePath = std::filesystem::path(L"Content") / L"NewClothSource.uasset";
		}

		std::filesystem::path Directory = SourcePath.parent_path();
		if (Directory.empty())
		{
			Directory = L"Content";
		}

		const std::wstring BaseStem = SourcePath.stem().wstring() + L"_Cloth";
		for (int32 Suffix = 0;; ++Suffix)
		{
			std::wstring FileName = BaseStem;
			if (Suffix > 0)
			{
				FileName += L"_";
				FileName += std::to_wstring(Suffix);
			}
			FileName += L".uasset";

			const std::filesystem::path Candidate = Directory / FileName;
			const std::filesystem::path FullCandidate = std::filesystem::path(FPaths::RootDir()) / Candidate;
			if (!std::filesystem::exists(FullCandidate))
			{
				FPaths::CreateDir(FullCandidate.parent_path().wstring());
				return FPaths::ToUtf8(Candidate.generic_wstring());
			}
		}
	}

	bool IsNearlyEqual(float A, float B, float Tolerance = 0.01f)
	{
		return std::abs(A - B) <= Tolerance;
	}

	bool IsNearlyEqual(const FVector& A, const FVector& B, float Tolerance = 0.01f)
	{
		return IsNearlyEqual(A.X, B.X, Tolerance)
			&& IsNearlyEqual(A.Y, B.Y, Tolerance)
			&& IsNearlyEqual(A.Z, B.Z, Tolerance);
	}

	bool IsLegacyDefaultQuadClothAsset(const UClothAsset& Asset)
	{
		if (Asset.GetParticleCount() != 4 || Asset.GetIndexCount() != 6)
		{
			return false;
		}

		static const FVector LegacyPositions[4] =
		{
			FVector(-50.0f, 0.0f, 50.0f),
			FVector(50.0f, 0.0f, 50.0f),
			FVector(-50.0f, 0.0f, -50.0f),
			FVector(50.0f, 0.0f, -50.0f),
		};
		static const uint32 LegacyIndices[6] = { 0, 1, 2, 1, 3, 2 };

		const TArray<FVector>& Positions = Asset.GetRestPositions();
		const TArray<uint32>& Indices = Asset.GetIndices();
		for (uint32 Index = 0; Index < 4; ++Index)
		{
			if (!IsNearlyEqual(Positions[Index], LegacyPositions[Index]))
			{
				return false;
			}
		}
		for (uint32 Index = 0; Index < 6; ++Index)
		{
			if (Indices[Index] != LegacyIndices[Index])
			{
				return false;
			}
		}

		const TArray<float>& InvMasses = Asset.GetInvMasses();
		const TArray<float>& PinMask = Asset.GetPinMask();
		for (uint32 Index = 0; Index < 4; ++Index)
		{
			if (Index < InvMasses.size() && !IsNearlyEqual(InvMasses[Index], 1.0f))
			{
				return false;
			}
			if (Index < PinMask.size() && !IsNearlyEqual(PinMask[Index], 0.0f))
			{
				return false;
			}
		}

		return true;
	}

	UMaterial* ResolveImportedMaterial(const FStaticMesh& Mesh, const TArray<FStaticMaterial>& Materials)
	{
		if (!Mesh.Sections.empty())
		{
			const int32 MaterialIndex = Mesh.Sections[0].MaterialIndex;
			if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(Materials.size()))
			{
				return Materials[MaterialIndex].MaterialInterface;
			}
		}

		return !Materials.empty() ? Materials[0].MaterialInterface : nullptr;
	}

	void ExtractRawMeshData(
		const FStaticMesh& Mesh,
		TArray<FVector>& OutPositions,
		TArray<FVector4>& OutColors,
		TArray<FVector2>& OutUVs,
		TArray<uint32>& OutIndices)
	{
		OutPositions.clear();
		OutColors.clear();
		OutUVs.clear();
		OutIndices = Mesh.Indices;

		OutPositions.reserve(Mesh.Vertices.size());
		OutColors.reserve(Mesh.Vertices.size());
		OutUVs.reserve(Mesh.Vertices.size());

		for (const FNormalVertex& Vertex : Mesh.Vertices)
		{
			OutPositions.push_back(Vertex.pos);
			OutColors.push_back(Vertex.color);
			OutUVs.push_back(Vertex.tex);
		}
	}

	bool ImportStaticMeshSourceForCloth(
		const FString& SourcePath,
		const FImportOptions& ImportOptions,
		TArray<FVector>& OutPositions,
		TArray<FVector4>& OutColors,
		TArray<FVector2>& OutUVs,
		TArray<uint32>& OutIndices,
		UMaterial*& OutMaterial,
		FString* OutError)
	{
		FString Extension = FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(SourcePath)).extension().wstring());
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

		FStaticMesh Mesh;
		TArray<FStaticMaterial> Materials;
		if (Extension == ".obj")
		{
			if (!FObjImporter::Import(SourcePath, ImportOptions, Mesh, Materials))
			{
				SetError(OutError, "OBJ import failed.");
				return false;
			}
		}
		else if (Extension == ".fbx")
		{
			FFbxStaticMeshImportResult ImportResult;
			FString ImportMessage;
			if (!FFbxImporter::ImportStaticMesh(SourcePath, &ImportOptions, ImportResult, &ImportMessage))
			{
				SetError(OutError, ImportMessage.empty() ? FString("FBX static mesh import failed.") : ImportMessage);
				return false;
			}

			Mesh = std::move(ImportResult.Mesh);
			Materials = std::move(ImportResult.Materials);
		}
		else
		{
			SetError(OutError, "ClothAsset import supports only OBJ and FBX source files.");
			return false;
		}

		ExtractRawMeshData(Mesh, OutPositions, OutColors, OutUVs, OutIndices);
		OutMaterial = ResolveImportedMaterial(Mesh, Materials);

		if (OutPositions.empty())
		{
			SetError(OutError, "Imported mesh has no vertices.");
			return false;
		}
		if (OutIndices.size() < 3 || OutIndices.size() % 3 != 0)
		{
			SetError(OutError, "Imported mesh has no triangle indices.");
			return false;
		}

		return true;
	}

	template <typename SourceMeshType, typename BuildFn>
	bool CreateFromMeshImpl(
		SourceMeshType* SourceMesh,
		const FString& SourcePath,
		FString& OutPackagePath,
		UClothAsset** OutAsset,
		FString* OutError,
		const FClothAssetBuildOptions& Options,
		BuildFn&& Builder)
	{
		if (!SourceMesh)
		{
			SetError(OutError, "Source mesh is null.");
			return false;
		}

		OutPackagePath = BuildUniqueClothAssetPath(SourcePath);

		UClothAsset* Asset = UObjectManager::Get().CreateObject<UClothAsset>();
		Asset->SetSourcePath(OutPackagePath);

		if (!Builder(SourceMesh, *Asset, Options, OutError))
		{
			UObjectManager::Get().DestroyObject(Asset);
			return false;
		}

		FAssetImportMetadata Metadata;
		BuildSourceMetadata(SourcePath, Metadata);

		if (!FClothAssetManager::Get().Save(Asset, &Metadata))
		{
			SetError(OutError, "Failed to save ClothAsset package.");
			UObjectManager::Get().DestroyObject(Asset);
			return false;
		}

		if (OutAsset)
		{
			*OutAsset = Asset;
		}
		return true;
	}
}

UClothAsset* FClothAssetManager::Load(const FString& Path)
{
	const FString NormalizedPath = NormalizeProjectPath(Path);
	auto It = LoadedClothAssets.find(NormalizedPath);
	if (It != LoadedClothAssets.end())
	{
		UClothAsset* Asset = It->second;
		UpgradeLegacyDefaultQuadTo32x32(Asset);
		return Asset;
	}

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
	if (!Header.IsValid(EAssetPackageType::ClothAsset))
	{
		return nullptr;
	}

	FAssetImportMetadata Metadata;
	Ar << Metadata;
	Ar.SetPackageVersion(Header.Version);

	UClothAsset* Asset = UObjectManager::Get().CreateObject<UClothAsset>();
	Asset->Serialize(Ar);

	if (!Ar.IsValid() || !Asset->HasValidSimulationData())
	{
		UObjectManager::Get().DestroyObject(Asset);
		return nullptr;
	}

	Asset->SetSourcePath(NormalizedPath);
	UpgradeLegacyDefaultQuadTo32x32(Asset);
	LoadedClothAssets[NormalizedPath] = Asset;
	return Asset;
}

UClothAsset* FClothAssetManager::Find(const FString& Path) const
{
	const FString NormalizedPath = NormalizeProjectPath(Path);
	auto It = LoadedClothAssets.find(NormalizedPath);
	return It != LoadedClothAssets.end() ? It->second : nullptr;
}

bool FClothAssetManager::UpgradeLegacyDefaultQuadTo32x32(UClothAsset* Asset)
{
	if (!Asset || !IsLegacyDefaultQuadClothAsset(*Asset))
	{
		return false;
	}

	const FString PackagePath = Asset->GetSourcePath();
	UMaterial* Material = Asset->GetMaterial();

	UClothAsset* TempAsset = UObjectManager::Get().CreateObject<UClothAsset>();
	TempAsset->SetSourcePath(PackagePath);

	TArray<FVector> Positions;
	TArray<FVector4> Colors;
	TArray<FVector2> UVs;
	TArray<uint32> Indices;

	FClothAssetBuildOptions BuildOptions;
	BuildOptions.bBuildDefaultPinnedGrid32x32 = true;

	FString Error;
	if (!FClothAssetBuilder::BuildFromRawMesh(Positions, Colors, UVs, Indices, Material, *TempAsset, BuildOptions, &Error))
	{
		UE_LOG("Failed to upgrade legacy default ClothAsset quad to 32x32 grid: %s", Error.c_str());
		UObjectManager::Get().DestroyObject(TempAsset);
		return false;
	}

	Asset->FabricData = std::move(TempAsset->FabricData);
	Asset->RestPositions = std::move(TempAsset->RestPositions);
	Asset->InvMasses = std::move(TempAsset->InvMasses);
	Asset->Indices = std::move(TempAsset->Indices);
	Asset->UVs = std::move(TempAsset->UVs);
	Asset->PinMask = std::move(TempAsset->PinMask);
	Asset->SetMaterial(TempAsset->GetMaterial());
	Asset->SetSourcePath(PackagePath);

	UObjectManager::Get().DestroyObject(TempAsset);

	const FString NormalizedPath = NormalizeProjectPath(PackagePath);
	if (!NormalizedPath.empty() && NormalizedPath != "None")
	{
		LoadedClothAssets[NormalizedPath] = Asset;
	}

	UE_LOG("Upgraded legacy default ClothAsset quad to 32x32 grid: %s", PackagePath.c_str());
	return true;
}

bool FClothAssetManager::Save(UClothAsset* Asset, const FAssetImportMetadata* MetadataOverride)
{
	if (!Asset || !Asset->HasValidSimulationData())
	{
		return false;
	}

	const FString PackagePath = NormalizeProjectPath(Asset->GetSourcePath());
	if (PackagePath.empty() || PackagePath == "None")
	{
		return false;
	}

	FWindowsBinWriter Ar(PackagePath);
	if (!Ar.IsValid())
	{
		return false;
	}

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::ClothAsset);

	FAssetImportMetadata Metadata;
	if (MetadataOverride)
	{
		Metadata = *MetadataOverride;
	}

	Ar << Header;
	Ar << Metadata;
	Ar.SetPackageVersion(Header.Version);
	Asset->Serialize(Ar);

	if (!Ar.IsValid())
	{
		return false;
	}

	Asset->SetSourcePath(PackagePath);
	LoadedClothAssets[PackagePath] = Asset;
	RefreshAvailableClothAssets();
	return true;
}

bool FClothAssetManager::CreateFromRawMesh(
	const FString& SourcePath,
	const TArray<FVector>& Positions,
	const TArray<FVector4>& Colors,
	const TArray<FVector2>& UVs,
	const TArray<uint32>& Indices,
	UMaterial* Material,
	FString& OutPackagePath,
	UClothAsset** OutAsset,
	FString* OutError,
	const FClothAssetBuildOptions& Options)
{
	OutPackagePath = BuildUniqueClothAssetPath(SourcePath);

	UClothAsset* Asset = UObjectManager::Get().CreateObject<UClothAsset>();
	Asset->SetSourcePath(OutPackagePath);

	if (!FClothAssetBuilder::BuildFromRawMesh(Positions, Colors, UVs, Indices, Material, *Asset, Options, OutError))
	{
		UObjectManager::Get().DestroyObject(Asset);
		return false;
	}

	FAssetImportMetadata Metadata;
	BuildSourceMetadata(SourcePath, Metadata);

	if (!Save(Asset, &Metadata))
	{
		SetError(OutError, "Failed to save ClothAsset package.");
		UObjectManager::Get().DestroyObject(Asset);
		return false;
	}

	if (OutAsset)
	{
		*OutAsset = Asset;
	}
	return true;
}

bool FClothAssetManager::CreateFromMeshSourceFile(
	const FString& SourcePath,
	const FImportOptions& ImportOptions,
	FString& OutPackagePath,
	UClothAsset** OutAsset,
	FString* OutError,
	const FClothAssetBuildOptions& BuildOptions)
{
	TArray<FVector> Positions;
	TArray<FVector4> Colors;
	TArray<FVector2> UVs;
	TArray<uint32> Indices;
	UMaterial* Material = nullptr;

	if (!ImportStaticMeshSourceForCloth(SourcePath, ImportOptions, Positions, Colors, UVs, Indices, Material, OutError))
	{
		return false;
	}

	return CreateFromRawMesh(SourcePath, Positions, Colors, UVs, Indices, Material, OutPackagePath, OutAsset, OutError, BuildOptions);
}

bool FClothAssetManager::ReplaceFromRawMesh(
	UClothAsset* TargetAsset,
	const FString& SourcePath,
	const TArray<FVector>& Positions,
	const TArray<FVector4>& Colors,
	const TArray<FVector2>& UVs,
	const TArray<uint32>& Indices,
	UMaterial* Material,
	FString* OutError,
	const FClothAssetBuildOptions& Options)
{
	if (!TargetAsset)
	{
		SetError(OutError, "Target ClothAsset is null.");
		return false;
	}

	const FString PackagePath = NormalizeProjectPath(TargetAsset->GetSourcePath());
	if (PackagePath.empty() || PackagePath == "None")
	{
		SetError(OutError, "Target ClothAsset has no package path.");
		return false;
	}

	UClothAsset* TempAsset = UObjectManager::Get().CreateObject<UClothAsset>();
	TempAsset->SetSourcePath(PackagePath);
	if (!FClothAssetBuilder::BuildFromRawMesh(Positions, Colors, UVs, Indices, Material, *TempAsset, Options, OutError))
	{
		UObjectManager::Get().DestroyObject(TempAsset);
		return false;
	}

	TargetAsset->FabricData = std::move(TempAsset->FabricData);
	TargetAsset->RestPositions = std::move(TempAsset->RestPositions);
	TargetAsset->InvMasses = std::move(TempAsset->InvMasses);
	TargetAsset->Indices = std::move(TempAsset->Indices);
	TargetAsset->UVs = std::move(TempAsset->UVs);
	TargetAsset->PinMask = std::move(TempAsset->PinMask);
	TargetAsset->SetMaterial(TempAsset->GetMaterial());
	TargetAsset->SetSourcePath(PackagePath);

	UObjectManager::Get().DestroyObject(TempAsset);

	FAssetImportMetadata Metadata;
	BuildSourceMetadata(SourcePath, Metadata);
	if (!Save(TargetAsset, &Metadata))
	{
		SetError(OutError, "Failed to save ClothAsset package.");
		return false;
	}

	return true;
}

bool FClothAssetManager::ReplaceFromMeshSourceFile(
	UClothAsset* TargetAsset,
	const FString& SourcePath,
	const FImportOptions& ImportOptions,
	FString* OutError,
	const FClothAssetBuildOptions& BuildOptions)
{
	TArray<FVector> Positions;
	TArray<FVector4> Colors;
	TArray<FVector2> UVs;
	TArray<uint32> Indices;
	UMaterial* Material = nullptr;

	if (!ImportStaticMeshSourceForCloth(SourcePath, ImportOptions, Positions, Colors, UVs, Indices, Material, OutError))
	{
		return false;
	}

	return ReplaceFromRawMesh(TargetAsset, SourcePath, Positions, Colors, UVs, Indices, Material, OutError, BuildOptions);
}

bool FClothAssetManager::CreateFromStaticMesh(
	UStaticMesh* SourceMesh,
	FString& OutPackagePath,
	UClothAsset** OutAsset,
	FString* OutError,
	const FClothAssetBuildOptions& Options)
{
	const FString SourcePath = SourceMesh ? SourceMesh->GetAssetPathFileName() : FString();
	return CreateFromMeshImpl(
		SourceMesh,
		SourcePath,
		OutPackagePath,
		OutAsset,
		OutError,
		Options,
		[](UStaticMesh* Mesh, UClothAsset& Asset, const FClothAssetBuildOptions& BuildOptions, FString* Error)
		{
			return FClothAssetBuilder::BuildFromStaticMesh(Mesh, Asset, BuildOptions, Error);
		});
}

bool FClothAssetManager::CreateFromSkeletalMesh(
	USkeletalMesh* SourceMesh,
	FString& OutPackagePath,
	UClothAsset** OutAsset,
	FString* OutError,
	const FClothAssetBuildOptions& Options)
{
	const FString SourcePath = SourceMesh ? SourceMesh->GetAssetPathFileName() : FString();
	return CreateFromMeshImpl(
		SourceMesh,
		SourcePath,
		OutPackagePath,
		OutAsset,
		OutError,
		Options,
		[](USkeletalMesh* Mesh, UClothAsset& Asset, const FClothAssetBuildOptions& BuildOptions, FString* Error)
		{
			return FClothAssetBuilder::BuildFromSkeletalMesh(Mesh, Asset, BuildOptions, Error);
		});
}

void FClothAssetManager::RefreshAvailableClothAssets()
{
	AvailableClothAssetFiles.clear();

	const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::RootDir()) / L"Content";
	if (!std::filesystem::exists(ContentRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	for (const auto& Entry : std::filesystem::recursive_directory_iterator(ContentRoot))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		std::wstring Ext = Entry.path().extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".uasset")
		{
			continue;
		}

		const FString RelPath = FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());
		FAssetImportMetadata Metadata;
		if (!FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::ClothAsset, Metadata))
		{
			continue;
		}

		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().wstring());
		Item.FullPath = RelPath;
		AvailableClothAssetFiles.push_back(std::move(Item));
	}
}
