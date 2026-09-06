#include "Physics/Cloth/ClothAsset.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cstring>

bool FClothFabricCookedData::IsValid(uint32 ParticleCount) const
{
	if (ParticleCount == 0 || PhaseIndices.empty() || Sets.empty() || RestValues.empty())
	{
		return false;
	}

	if (ConstraintIndices.size() != RestValues.size() * 2)
	{
		return false;
	}

	if (!StiffnessValues.empty() && StiffnessValues.size() != RestValues.size())
	{
		return false;
	}

	if (!TetherLengths.empty() && Anchors.size() != TetherLengths.size())
	{
		return false;
	}

	if (!TetherLengths.empty() && TetherLengths.size() != ParticleCount)
	{
		return false;
	}

	if (!Triangles.empty() && Triangles.size() % 3 != 0)
	{
		return false;
	}

	if (Sets.back() > RestValues.size())
	{
		return false;
	}

	for (uint32 Index : ConstraintIndices)
	{
		if (Index >= ParticleCount)
		{
			return false;
		}
	}

	for (uint32 Anchor : Anchors)
	{
		if (Anchor >= ParticleCount)
		{
			return false;
		}
	}

	for (float TetherLength : TetherLengths)
	{
		if (TetherLength < 0.0f)
		{
			return false;
		}
	}

	return true;
}

void FClothFabricCookedData::Serialize(FArchive& Ar)
{
	Ar << PhaseIndices;
	Ar << Sets;
	Ar << RestValues;
	Ar << StiffnessValues;
	Ar << ConstraintIndices;
	Ar << Anchors;
	Ar << TetherLengths;
	Ar << Triangles;
}

void UClothAsset::Serialize(FArchive& Ar)
{
	FString SavedSourcePath = Ar.IsSaving() ? SourcePath : FString();
	Ar << SavedSourcePath;
	if (Ar.IsLoading())
	{
		SourcePath = SavedSourcePath.empty() ? FString("None") : SavedSourcePath;
	}

	FString SavedMaterialPath = Ar.IsSaving() ? MaterialPath.ToString() : FString();
	Ar << SavedMaterialPath;
	if (Ar.IsLoading())
	{
		MaterialPath.SetPath(SavedMaterialPath.empty() ? FString("None") : SavedMaterialPath);
	}

	FabricData.Serialize(Ar);
	Ar << RestPositions;
	Ar << InvMasses;
	Ar << Indices;
	Ar << UVs;
	Ar << PinMask;

	if (Ar.IsLoading())
	{
		ResolveMaterial();
	}
}

void UClothAsset::PostEditProperty(const char* PropertyName)
{
	UObject::PostEditProperty(PropertyName);
	if (!PropertyName)
	{
		return;
	}

	if (std::strcmp(PropertyName, "MaterialPath") == 0 || std::strcmp(PropertyName, "Material") == 0)
	{
		ResolveMaterial();
	}
}

void UClothAsset::SetMaterial(UMaterial* InMaterial)
{
	Material = InMaterial;
	MaterialPath = Material ? Material->GetAssetPathFileName() : FString("None");
}

UMaterial* UClothAsset::ResolveMaterial()
{
	if (MaterialPath.IsNull() || MaterialPath.ToString() == "None")
	{
		Material = nullptr;
		return nullptr;
	}

	Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath.ToString());
	if (Material)
	{
		MaterialPath.SetCachedObject(Material);
	}
	return Material;
}

bool UClothAsset::HasValidSimulationData() const
{
	const uint32 ParticleCount = static_cast<uint32>(RestPositions.size());
	if (ParticleCount == 0 || InvMasses.size() != RestPositions.size())
	{
		return false;
	}

	if (!Indices.empty() && Indices.size() % 3 != 0)
	{
		return false;
	}

	if (!UVs.empty() && UVs.size() != RestPositions.size())
	{
		return false;
	}

	if (!PinMask.empty() && PinMask.size() != RestPositions.size())
	{
		return false;
	}

	for (uint32 Index : Indices)
	{
		if (Index >= ParticleCount)
		{
			return false;
		}
	}

	return FabricData.IsValid(ParticleCount);
}

void UClothAsset::BuildInitialParticles(TArray<FVector4>& OutParticles) const
{
	OutParticles.clear();
	OutParticles.reserve(RestPositions.size());

	for (uint32 Index = 0; Index < static_cast<uint32>(RestPositions.size()); ++Index)
	{
		float InvMass = Index < InvMasses.size() ? InvMasses[Index] : 1.0f;
		if (Index < PinMask.size() && PinMask[Index] > 0.0f)
		{
			InvMass = 0.0f;
		}

		const FVector& Position = RestPositions[Index];
		OutParticles.push_back(FVector4(Position.X, Position.Y, Position.Z, InvMass));
	}
}
