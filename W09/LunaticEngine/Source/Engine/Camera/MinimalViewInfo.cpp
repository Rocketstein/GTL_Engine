#include "MinimalViewInfo.h"

float FPostProcessSettings::GetScalar(FName Name, float DefaultValue) const
{
	return GetParameterValue(ScalarParameter, Name, DefaultValue);
}

void FPostProcessSettings::SetScalar(FName Name, float Value)
{
	SetParameterValue(ScalarParameter, Name, Value);
}

FVector FPostProcessSettings::GetVector(FName Name, FVector DefaultValue) const
{
	return GetParameterValue(VectorParameter, Name, DefaultValue);
}

void FPostProcessSettings::SetVector(FName Name, FVector Value)
{
	SetParameterValue(VectorParameter, Name, Value);
}

FLinearColor FPostProcessSettings::GetColor(FName Name, FLinearColor DefaultValue) const
{
	return GetParameterValue(ColorParameter, Name, DefaultValue);
}

void FPostProcessSettings::SetColor(FName Name, FLinearColor Value)
{
	SetParameterValue(ColorParameter, Name, Value);
}

FPostProcessSettings::FMaterialEntry& FPostProcessSettings::FindOrAddMaterial(const FString& MaterialPath)
{
	for (FMaterialEntry& Entry : Materials)
	{
		if (Entry.MaterialPath == MaterialPath)
		{
			return Entry;
		}
	}

	FMaterialEntry Entry;
	Entry.MaterialPath = MaterialPath;
	Materials.push_back(Entry);
	return Materials.back();
}

void FPostProcessSettings::AddMaterial(const FString& MaterialPath, float BlendWeight)
{
	FMaterialEntry& Entry = FindOrAddMaterial(MaterialPath);
	Entry.BlendWeight = BlendWeight;
}

void FPostProcessSettings::SetMaterialScalar(const FString& MaterialPath, FName Name, float Value)
{
	FMaterialEntry& Entry = FindOrAddMaterial(MaterialPath);
	SetParameterValue(Entry.Parameters.ScalarParameter, Name, Value);
}

float FPostProcessSettings::GetMaterialScalar(const FString& MaterialPath, FName Name, float DefaultValue) const
{
	for (const FMaterialEntry& Entry : Materials)
	{
		if (Entry.MaterialPath == MaterialPath)
		{
			return GetParameterValue(Entry.Parameters.ScalarParameter, Name, DefaultValue);
		}
	}

	return DefaultValue;
}

