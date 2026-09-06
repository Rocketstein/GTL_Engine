#include "Distributions/Distributions.h"

#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

void FDistributionLookupTable::Reset()
{
	Operation = ERawDistributionOperation::Uninitialized;
	EntryCount = 0;
	EntryStride = 0;
	TimeScale = 1.0f;
	TimeBias = 0.0f;
	Values.clear();
}

bool FDistributionLookupTable::IsValid() const
{
	return Operation != ERawDistributionOperation::Uninitialized
		&& EntryCount > 0
		&& EntryStride > 0
		&& static_cast<int32>(Values.size()) >= EntryCount * EntryStride;
}

float FDistributionLookupTable::GetValue(float Time, int32 ComponentIndex) const
{
	if (!IsValid() || ComponentIndex < 0 || ComponentIndex >= EntryStride)
	{
		return 0.0f;
	}

	const float SamplePosition = FMath::Clamp(Time * TimeScale + TimeBias, 0.0f, static_cast<float>(EntryCount - 1));
	const int32 IndexA = static_cast<int32>(std::floor(SamplePosition));
	const int32 IndexB = std::min(IndexA + 1, EntryCount - 1);
	const float Alpha = SamplePosition - static_cast<float>(IndexA);

	const int32 OffsetA = IndexA * EntryStride + ComponentIndex;
	const int32 OffsetB = IndexB * EntryStride + ComponentIndex;
	return DistributionLerp(Values[OffsetA], Values[OffsetB], Alpha);
}

float FDistributionRandomStream::GetFraction()
{
	Seed = Seed * 196314165u + 907633515u;
	return static_cast<float>((Seed >> 8) & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

void FRawDistribution::ResetLookupTable()
{
	LookupTable.Reset();
}

bool FRawDistribution::HasLookupTable() const
{
	return LookupTable.IsValid();
}

float DistributionRand(FDistributionRandomStream* RandomStream)
{
	if (RandomStream)
	{
		return RandomStream->GetFraction();
	}
	return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

float DistributionLerp(float Min, float Max, float Alpha)
{
	return Min + (Max - Min) * Alpha;
}

float DistributionMapValue(float Value, float MinInput, float MaxInput, float MinOutput, float MaxOutput)
{
	const float InputRange = MaxInput - MinInput;
	if (std::fabs(InputRange) <= 1.e-6f)
	{
		return MinOutput;
	}

	const float Alpha = FMath::Clamp((Value - MinInput) / InputRange, 0.0f, 1.0f);
	return DistributionLerp(MinOutput, MaxOutput, Alpha);
}

FVector DistributionMapVector(const FVector& Value, const FVector& MinInput, const FVector& MaxInput, const FVector& MinOutput, const FVector& MaxOutput)
{
	return FVector(
		DistributionMapValue(Value.X, MinInput.X, MaxInput.X, MinOutput.X, MaxOutput.X),
		DistributionMapValue(Value.Y, MinInput.Y, MaxInput.Y, MinOutput.Y, MaxOutput.Y),
		DistributionMapValue(Value.Z, MinInput.Z, MaxInput.Z, MinOutput.Z, MaxOutput.Z));
}
