#pragma once

#include "Object/Object.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Render/Types/VertexTypes.h"

#include "Source/Engine/Physics/Cloth/ClothAsset.generated.h"

class FArchive;
class UMaterial;

struct FClothFabricCookedData
{
	TArray<uint32> PhaseIndices;
	TArray<uint32> Sets;
	TArray<float> RestValues;
	TArray<float> StiffnessValues;
	TArray<uint32> ConstraintIndices;
	TArray<uint32> Anchors;
	TArray<float> TetherLengths;
	TArray<uint32> Triangles;

	bool IsValid(uint32 ParticleCount) const;
	void Serialize(FArchive& Ar);
};

UCLASS()
class UClothAsset : public UObject
{
public:
	GENERATED_BODY()

	void Serialize(FArchive& Ar) override;
	void PostEditProperty(const char* PropertyName) override;

	void SetSourcePath(const FString& InPath) { SourcePath = InPath; }
	const FString& GetSourcePath() const { return SourcePath; }

	void SetMaterial(UMaterial* InMaterial);
	UMaterial* GetMaterial() const { return Material; }
	UMaterial* ResolveMaterial();

	bool HasValidSimulationData() const;
	void BuildInitialParticles(TArray<FVector4>& OutParticles) const;

	const FClothFabricCookedData& GetFabricData() const { return FabricData; }
	const TArray<FVector>& GetRestPositions() const { return RestPositions; }
	const TArray<float>& GetInvMasses() const { return InvMasses; }
	const TArray<uint32>& GetIndices() const { return Indices; }
	const TArray<FVector2>& GetUVs() const { return UVs; }
	const TArray<float>& GetPinMask() const { return PinMask; }

	uint32 GetParticleCount() const { return static_cast<uint32>(RestPositions.size()); }
	uint32 GetIndexCount() const { return static_cast<uint32>(Indices.size()); }

public:
	FClothFabricCookedData FabricData;
	TArray<FVector> RestPositions;
	TArray<float> InvMasses;
	TArray<uint32> Indices;
	TArray<FVector2> UVs;
	TArray<float> PinMask;

	UPROPERTY(Edit, Save, Category="Cloth", DisplayName="Material", AssetType="Material")
	FSoftObjectPtr MaterialPath = "None";

private:
	FString SourcePath = "None";
	UMaterial* Material = nullptr;
};
