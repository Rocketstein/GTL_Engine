#include "Distributions/DistributionFloat.h"

#include "Component/Primitive/ParticleSystemComponent.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
	void GetCurveTimeRange(const FFloatCurve& Curve, float& OutMinTime, float& OutMaxTime)
	{
		if (Curve.Keys.empty())
		{
			OutMinTime = 0.0f;
			OutMaxTime = 1.0f;
			return;
		}

		OutMinTime = Curve.Keys.front().Time;
		OutMaxTime = Curve.Keys.front().Time;
		for (const FCurveKey& Key : Curve.Keys)
		{
			OutMinTime = std::min(OutMinTime, Key.Time);
			OutMaxTime = std::max(OutMaxTime, Key.Time);
		}
	}

	void GetCurveValueRange(const FFloatCurve& Curve, float& OutMin, float& OutMax)
	{
		if (Curve.Keys.empty())
		{
			OutMin = Curve.DefaultValue;
			OutMax = Curve.DefaultValue;
			return;
		}

		OutMin = Curve.Keys.front().Value;
		OutMax = Curve.Keys.front().Value;
		for (const FCurveKey& Key : Curve.Keys)
		{
			OutMin = std::min(OutMin, Key.Value);
			OutMax = std::max(OutMax, Key.Value);
		}
	}

	void CombineRange(float& InOutMin, float& InOutMax, float OtherMin, float OtherMax)
	{
		InOutMin = std::min(InOutMin, OtherMin);
		InOutMax = std::max(InOutMax, OtherMax);
	}
}

float UDistributionFloat::GetValue(float /*Time*/, UObject* /*Data*/, FDistributionRandomStream* /*RandomStream*/) const
{
	return 0.0f;
}

void UDistributionFloat::GetValueRange(float Time, float& OutMin, float& OutMax) const
{
	const float Value = GetValue(Time, nullptr, nullptr);
	OutMin = Value;
	OutMax = Value;
}

void UDistributionFloat::GetOutRange(float& OutMin, float& OutMax) const
{
	GetValueRange(0.0f, OutMin, OutMax);
}

float FRawDistributionFloat::GetValue(float Time, UObject* Data, FDistributionRandomStream* RandomStream) const
{
	if (LookupTable.IsValid())
	{
		if (LookupTable.Operation == ERawDistributionOperation::Random && LookupTable.EntryStride >= 2)
		{
			const float MinValue = LookupTable.GetValue(Time, 0);
			const float MaxValue = LookupTable.GetValue(Time, 1);
			return DistributionLerp(MinValue, MaxValue, DistributionRand(RandomStream));
		}

		if (LookupTable.Operation == ERawDistributionOperation::Extreme && LookupTable.EntryStride >= 2)
		{
			return DistributionRand(RandomStream) < 0.5f
				? LookupTable.GetValue(Time, 0)
				: LookupTable.GetValue(Time, 1);
		}

		return LookupTable.GetValue(Time, 0);
	}

	return Distribution ? Distribution->GetValue(Time, Data, RandomStream) : 0.0f;
}

void FRawDistributionFloat::GetOutRange(float& OutMin, float& OutMax) const
{
	if (Distribution)
	{
		Distribution->GetOutRange(OutMin, OutMax);
		return;
	}

	OutMin = 0.0f;
	OutMax = 0.0f;
}

bool FRawDistributionFloat::IsUniform() const
{
	return Distribution && Distribution->IsUniform();
}

void FRawDistributionFloat::Initialize()
{
	ResetLookupTable();
	if (!Distribution || !Distribution->CanBeBaked())
	{
		return;
	}

	float MinTime = 0.0f;
	float MaxTime = 1.0f;
	Distribution->GetTimeRange(MinTime, MaxTime);
	const bool bHasTimeRange = std::fabs(MaxTime - MinTime) > 1.e-6f;
	const int32 SampleCount = bHasTimeRange ? 32 : 1;
	const bool bUniform = Distribution->IsUniform();

	LookupTable.Operation = bUniform ? ERawDistributionOperation::Random : ERawDistributionOperation::None;
	LookupTable.EntryCount = SampleCount;
	LookupTable.EntryStride = bUniform ? 2 : 1;
	LookupTable.TimeScale = bHasTimeRange ? static_cast<float>(SampleCount - 1) / (MaxTime - MinTime) : 1.0f;
	LookupTable.TimeBias = bHasTimeRange ? -MinTime * LookupTable.TimeScale : 0.0f;
	LookupTable.Values.resize(static_cast<size_t>(LookupTable.EntryCount * LookupTable.EntryStride));

	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		const float Alpha = SampleCount > 1 ? static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1) : 0.0f;
		const float Time = bHasTimeRange ? DistributionLerp(MinTime, MaxTime, Alpha) : MinTime;
		const int32 Offset = SampleIndex * LookupTable.EntryStride;

		if (bUniform)
		{
			float RangeMin = 0.0f;
			float RangeMax = 0.0f;
			Distribution->GetValueRange(Time, RangeMin, RangeMax);
			LookupTable.Values[Offset] = RangeMin;
			LookupTable.Values[Offset + 1] = RangeMax;
		}
		else
		{
			LookupTable.Values[Offset] = Distribution->GetValue(Time, nullptr, nullptr);
		}
	}
}

float UDistributionFloatConstant::GetValue(float /*Time*/, UObject* /*Data*/, FDistributionRandomStream* /*RandomStream*/) const
{
	return Constant;
}

void UDistributionFloatConstant::GetValueRange(float /*Time*/, float& OutMin, float& OutMax) const
{
	OutMin = Constant;
	OutMax = Constant;
}

void UDistributionFloatConstant::GetOutRange(float& OutMin, float& OutMax) const
{
	OutMin = Constant;
	OutMax = Constant;
}

float UDistributionFloatUniform::GetValue(float /*Time*/, UObject* /*Data*/, FDistributionRandomStream* RandomStream) const
{
	float RangeMin = 0.0f;
	float RangeMax = 0.0f;
	GetValueRange(0.0f, RangeMin, RangeMax);
	if (bUseExtremes)
	{
		return DistributionRand(RandomStream) < 0.5f ? RangeMin : RangeMax;
	}
	return DistributionLerp(RangeMin, RangeMax, DistributionRand(RandomStream));
}

void UDistributionFloatUniform::GetValueRange(float /*Time*/, float& OutMin, float& OutMax) const
{
	OutMin = std::min(Min, Max);
	OutMax = std::max(Min, Max);
}

void UDistributionFloatUniform::GetOutRange(float& OutMin, float& OutMax) const
{
	GetValueRange(0.0f, OutMin, OutMax);
}

float UDistributionFloatConstantCurve::GetValue(float Time, UObject* /*Data*/, FDistributionRandomStream* /*RandomStream*/) const
{
	return ConstantCurve.Evaluate(Time);
}

void UDistributionFloatConstantCurve::GetValueRange(float Time, float& OutMin, float& OutMax) const
{
	const float Value = ConstantCurve.Evaluate(Time);
	OutMin = Value;
	OutMax = Value;
}

void UDistributionFloatConstantCurve::GetOutRange(float& OutMin, float& OutMax) const
{
	GetCurveValueRange(ConstantCurve, OutMin, OutMax);
}

void UDistributionFloatConstantCurve::GetTimeRange(float& OutMinTime, float& OutMaxTime) const
{
	GetCurveTimeRange(ConstantCurve, OutMinTime, OutMaxTime);
}

float UDistributionFloatUniformCurve::GetValue(float Time, UObject* /*Data*/, FDistributionRandomStream* RandomStream) const
{
	float RangeMin = 0.0f;
	float RangeMax = 0.0f;
	GetValueRange(Time, RangeMin, RangeMax);
	if (bUseExtremes)
	{
		return DistributionRand(RandomStream) < 0.5f ? RangeMin : RangeMax;
	}
	return DistributionLerp(RangeMin, RangeMax, DistributionRand(RandomStream));
}

void UDistributionFloatUniformCurve::GetValueRange(float Time, float& OutMin, float& OutMax) const
{
	const float MinValue = MinCurve.Evaluate(Time);
	const float MaxValue = MaxCurve.Evaluate(Time);
	OutMin = std::min(MinValue, MaxValue);
	OutMax = std::max(MinValue, MaxValue);
}

void UDistributionFloatUniformCurve::GetOutRange(float& OutMin, float& OutMax) const
{
	GetCurveValueRange(MinCurve, OutMin, OutMax);
	float OtherMin = 0.0f;
	float OtherMax = 0.0f;
	GetCurveValueRange(MaxCurve, OtherMin, OtherMax);
	CombineRange(OutMin, OutMax, OtherMin, OtherMax);
}

void UDistributionFloatUniformCurve::GetTimeRange(float& OutMinTime, float& OutMaxTime) const
{
	GetCurveTimeRange(MinCurve, OutMinTime, OutMaxTime);
	float OtherMinTime = 0.0f;
	float OtherMaxTime = 1.0f;
	GetCurveTimeRange(MaxCurve, OtherMinTime, OtherMaxTime);
	OutMinTime = std::min(OutMinTime, OtherMinTime);
	OutMaxTime = std::max(OutMaxTime, OtherMaxTime);
}

float UDistributionFloatParameterBase::GetValue(float /*Time*/, UObject* Data, FDistributionRandomStream* /*RandomStream*/) const
{
	float ParamValue = Constant;
	if (!GetParamValue(Data, ParameterName, ParamValue))
	{
		return Constant;
	}

	switch (ParamMode)
	{
	case EDistributionParamMode::Direct:
		return ParamValue;
	case EDistributionParamMode::Abs:
		ParamValue = std::fabs(ParamValue);
		break;
	case EDistributionParamMode::Normal:
	default:
		break;
	}

	return DistributionMapValue(ParamValue, MinInput, MaxInput, MinOutput, MaxOutput);
}

void UDistributionFloatParameterBase::GetValueRange(float Time, float& OutMin, float& OutMax) const
{
	const float Value = GetValue(Time, nullptr, nullptr);
	OutMin = Value;
	OutMax = Value;
}

void UDistributionFloatParameterBase::GetOutRange(float& OutMin, float& OutMax) const
{
	OutMin = std::min(MinOutput, MaxOutput);
	OutMax = std::max(MinOutput, MaxOutput);
}

bool UDistributionFloatParameterBase::GetParamValue(UObject* /*Data*/, FName /*ParamName*/, float& /*OutFloat*/) const
{
	return false;
}

bool UDistributionFloatParticleParameter::GetParamValue(UObject* Data, FName ParamName, float& OutFloat) const
{
	if (const UParticleSystemComponent* Component = Cast<UParticleSystemComponent>(Data))
	{
		return Component->GetFloatParameter(ParamName, OutFloat);
	}
	return false;
}
