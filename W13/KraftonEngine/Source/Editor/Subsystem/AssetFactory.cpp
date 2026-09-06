#include "Editor/Subsystem/AssetFactory.h"

#include "Animation/Graph/AnimGraphAsset.h"
#include "Animation/Graph/AnimGraphManager.h"
#include "CameraShake/CameraShakeAsset.h"
#include "CameraShake/CameraShakeManager.h"
#include "FloatCurve/FloatCurveManager.h"
#include "FloatCurve/FloatCurveAsset.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Particle/ParticleSystem.h"
#include "Particle/Asset/ParticleSystemManager.h"
#include "Particle/VectorField/VectorFieldAsset.h"
#include "Particle/VectorField/VectorFieldManager.h"
#include "Platform/Paths.h"
#include "Physics/Cloth/ClothAsset.h"
#include "Physics/Cloth/ClothAssetBuilder.h"
#include "Physics/Cloth/ClothAssetManager.h"

#include <filesystem>

namespace
{
	FString SanitizeAssetStem(const FString& AssetName)
	{
		return AssetName.empty() ? FString("NewFloatCurve") : AssetName;
	}

	std::filesystem::path BuildUniqueAssetPath(const std::filesystem::path& Directory, const FString& AssetName, const wchar_t* Extension)
	{
		const FString BaseStem = SanitizeAssetStem(AssetName);

		int32 Suffix = 0;
		for (;;)
		{
			FString CandidateStem = BaseStem;
			if (Suffix > 0)
			{
				CandidateStem += "_";
				CandidateStem += std::to_string(Suffix);
			}

			std::filesystem::path CandidatePath = Directory / (FPaths::ToWide(CandidateStem) + Extension);
			if (!std::filesystem::exists(CandidatePath))
			{
				return CandidatePath;
			}

			++Suffix;
		}
	}
}

bool FAssetFactory::CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UFloatCurveAsset* NewAsset = UObjectManager::Get().CreateObject<UFloatCurveAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));

	FFloatCurve& Curve = NewAsset->GetCurve();
	Curve.Reset();
	Curve.AddKey(0.0f, 0.0f);
	Curve.AddKey(1.0f, 1.0f);
	Curve.SortKeys();

	bool bSaved = FFloatCurveManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateCameraShake(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UCameraShakeAsset* NewAsset = UObjectManager::Get().CreateObject<UCameraShakeAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));
	NewAsset->Version = 1;
	NewAsset->ShakeType = ECameraShakeType::Sequence;

	bool bSaved = FCameraShakeManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateAnimGraph(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UAnimGraphAsset* NewAsset = UObjectManager::Get().CreateObject<UAnimGraphAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));
	NewAsset->InitializeDefault(); // SequencePlayer → OutputPose 기본 그래프.

	bool bSaved = FAnimGraphManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateParticleSystem(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UParticleSystem* NewAsset = UObjectManager::Get().CreateObject<UParticleSystem>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));
	NewAsset->InitializeDefaultSpriteSystem();

	bool bSaved = FParticleSystemManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}


bool FAssetFactory::CreateVectorField(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath, int32 SizeX, int32 SizeY, int32 SizeZ)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	if (SizeX <= 0) SizeX = 16;
	if (SizeY <= 0) SizeY = 16;
	if (SizeZ <= 0) SizeZ = 16;

	const FString EffectiveName = AssetName.empty() ? FString("NewVectorField") : AssetName;
	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, EffectiveName, L".uasset");

	TArray<FVector> ZeroVectors;
	ZeroVectors.resize(static_cast<size_t>(SizeX) * static_cast<size_t>(SizeY) * static_cast<size_t>(SizeZ), FVector::ZeroVector);

	UVectorFieldAsset* NewAsset = UObjectManager::Get().CreateObject<UVectorFieldAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));
	NewAsset->SetGridData(SizeX, SizeY, SizeZ, FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f), ZeroVectors);

	bool bSaved = FVectorFieldManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateClothAsset(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName.empty() ? FString("ClothAsset") : AssetName, L".uasset");

	TArray<FVector> Positions;
	TArray<FVector4> Colors;
	TArray<FVector2> UVs;
	TArray<uint32> Indices;

	UClothAsset* NewAsset = UObjectManager::Get().CreateObject<UClothAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));

	FClothAssetBuildOptions BuildOptions;
	BuildOptions.bBuildDefaultPinnedGrid32x32 = true;

	FString Error;
	if (!FClothAssetBuilder::BuildFromRawMesh(Positions, Colors, UVs, Indices, nullptr, *NewAsset, BuildOptions, &Error))
	{
		UObjectManager::Get().DestroyObject(NewAsset);
		return false;
	}

	if (!FClothAssetManager::Get().Save(NewAsset))
	{
		UObjectManager::Get().DestroyObject(NewAsset);
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}
