#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Distributions/Distributions.generated.h"

class UObject;

UENUM()
enum class EDistributionParamMode : uint8
{
	Normal,
	Abs,
	Direct,
};

UENUM()
enum class ERawDistributionOperation : uint8
{
	Uninitialized,
	None,
	Random,
	Extreme,
};

USTRUCT()
struct FDistributionLookupTable
{
	GENERATED_BODY()

	UPROPERTY(Save, Category="Distribution", DisplayName="Operation", Enum=ERawDistributionOperation)
	ERawDistributionOperation Operation = ERawDistributionOperation::Uninitialized;

	UPROPERTY(Save, Category="Distribution", DisplayName="Entry Count")
	int32 EntryCount = 0;

	UPROPERTY(Save, Category="Distribution", DisplayName="Entry Stride")
	int32 EntryStride = 0;

	UPROPERTY(Save, Category="Distribution", DisplayName="Time Scale")
	float TimeScale = 1.0f;

	UPROPERTY(Save, Category="Distribution", DisplayName="Time Bias")
	float TimeBias = 0.0f;

	UPROPERTY(Save, Category="Distribution", DisplayName="Values", Type=Array)
	TArray<float> Values;

	void Reset();
	bool IsValid() const;
	float GetValue(float Time, int32 ComponentIndex) const;
};

struct FDistributionRandomStream
{
	uint32 Seed = 0x12345678u;

	explicit FDistributionRandomStream(uint32 InSeed = 0x12345678u)
		: Seed(InSeed)
	{
	}

	float GetFraction();
};

struct FRawDistribution
{
	FDistributionLookupTable LookupTable;

	void ResetLookupTable();
	bool HasLookupTable() const;
};

float DistributionRand(FDistributionRandomStream* RandomStream);
float DistributionLerp(float Min, float Max, float Alpha);
float DistributionMapValue(float Value, float MinInput, float MaxInput, float MinOutput, float MaxOutput);
FVector DistributionMapVector(const FVector& Value, const FVector& MinInput, const FVector& MaxInput, const FVector& MinOutput, const FVector& MaxOutput);
