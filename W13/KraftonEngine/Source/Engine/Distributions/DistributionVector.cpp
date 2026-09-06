#include "Distributions/DistributionVector.h"

#include "Component/Primitive/ParticleSystemComponent.h"

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

	void CombineTimeRange(const FFloatCurve& Curve, float& InOutMinTime, float& InOutMaxTime)
	{
		float MinTime = 0.0f;
		float MaxTime = 1.0f;
		GetCurveTimeRange(Curve, MinTime, MaxTime);
		InOutMinTime = std::min(InOutMinTime, MinTime);
		InOutMaxTime = std::max(InOutMaxTime, MaxTime);
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

	float ApplyMirror(float MinValue, float MaxValue, EDistributionVectorMirrorFlags MirrorFlag, bool bWantMax)
	{
		if (MirrorFlag == EDistributionVectorMirrorFlags::Mirror)
		{
			const float Extent = std::max(std::fabs(MinValue), std::fabs(MaxValue));
			return bWantMax ? Extent : -Extent;
		}
		return bWantMax ? std::max(MinValue, MaxValue) : std::min(MinValue, MaxValue);
	}

	void ApplyMirrorFlags(const FVector& Min, const FVector& Max, FVector& OutMin, FVector& OutMax,
		EDistributionVectorMirrorFlags MirrorX,
		EDistributionVectorMirrorFlags MirrorY,
		EDistributionVectorMirrorFlags MirrorZ)
	{
		OutMin.X = ApplyMirror(Min.X, Max.X, MirrorX, false);
		OutMax.X = ApplyMirror(Min.X, Max.X, MirrorX, true);
		OutMin.Y = ApplyMirror(Min.Y, Max.Y, MirrorY, false);
		OutMax.Y = ApplyMirror(Min.Y, Max.Y, MirrorY, true);
		OutMin.Z = ApplyMirror(Min.Z, Max.Z, MirrorZ, false);
		OutMax.Z = ApplyMirror(Min.Z, Max.Z, MirrorZ, true);
	}

	void ApplyLockedAxes(FVector& Value, EDistributionVectorLockFlags LockedAxes)
	{
		switch (LockedAxes)
		{
		case EDistributionVectorLockFlags::XY:
			Value.Y = Value.X;
			break;
		case EDistributionVectorLockFlags::XZ:
			Value.Z = Value.X;
			break;
		case EDistributionVectorLockFlags::YZ:
			Value.Z = Value.Y;
			break;
		case EDistributionVectorLockFlags::XYZ:
			Value.Y = Value.X;
			Value.Z = Value.X;
			break;
		case EDistributionVectorLockFlags::None:
		default:
			break;
		}
	}

	FVector LerpVector(const FVector& Min, const FVector& Max, const FVector& Alpha)
	{
		return FVector(
			DistributionLerp(Min.X, Max.X, Alpha.X),
			DistributionLerp(Min.Y, Max.Y, Alpha.Y),
			DistributionLerp(Min.Z, Max.Z, Alpha.Z));
	}
}

FVector UDistributionVector::GetValue(float /*Time*/, UObject* /*Data*/, FDistributionRandomStream* /*RandomStream*/) const
{
	return FVector::ZeroVector;
}

void UDistributionVector::GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const
{
	const FVector Value = GetValue(Time, nullptr, nullptr);
	OutMin = Value;
	OutMax = Value;
}

void UDistributionVector::GetOutRange(FVector& OutMin, FVector& OutMax) const
{
	GetValueRange(0.0f, OutMin, OutMax);
}

FVector FRawDistributionVector::GetValue(float Time, UObject* Data, FDistributionRandomStream* RandomStream) const
{
	if (LookupTable.IsValid())
	{
		if ((LookupTable.Operation == ERawDistributionOperation::Random ||
			 LookupTable.Operation == ERawDistributionOperation::Extreme) &&
			LookupTable.EntryStride >= 6)
		{
			const FVector MinValue(
				LookupTable.GetValue(Time, 0),
				LookupTable.GetValue(Time, 1),
				LookupTable.GetValue(Time, 2));
			const FVector MaxValue(
				LookupTable.GetValue(Time, 3),
				LookupTable.GetValue(Time, 4),
				LookupTable.GetValue(Time, 5));

			if (LookupTable.Operation == ERawDistributionOperation::Extreme)
			{
				return DistributionRand(RandomStream) < 0.5f ? MinValue : MaxValue;
			}

			return LerpVector(MinValue, MaxValue, FVector(
				DistributionRand(RandomStream),
				DistributionRand(RandomStream),
				DistributionRand(RandomStream)));
		}

		if (LookupTable.EntryStride >= 3)
		{
			return FVector(
				LookupTable.GetValue(Time, 0),
				LookupTable.GetValue(Time, 1),
				LookupTable.GetValue(Time, 2));
		}
	}

	return Distribution ? Distribution->GetValue(Time, Data, RandomStream) : FVector::ZeroVector;
}

void FRawDistributionVector::GetOutRange(FVector& OutMin, FVector& OutMax) const
{
	if (Distribution)
	{
		Distribution->GetOutRange(OutMin, OutMax);
		return;
	}

	OutMin = FVector::ZeroVector;
	OutMax = FVector::ZeroVector;
}

bool FRawDistributionVector::IsUniform() const
{
	return Distribution && Distribution->IsUniform();
}

void FRawDistributionVector::Initialize()
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
	LookupTable.EntryStride = bUniform ? 6 : 3;
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
			FVector RangeMin;
			FVector RangeMax;
			Distribution->GetValueRange(Time, RangeMin, RangeMax);
			LookupTable.Values[Offset + 0] = RangeMin.X;
			LookupTable.Values[Offset + 1] = RangeMin.Y;
			LookupTable.Values[Offset + 2] = RangeMin.Z;
			LookupTable.Values[Offset + 3] = RangeMax.X;
			LookupTable.Values[Offset + 4] = RangeMax.Y;
			LookupTable.Values[Offset + 5] = RangeMax.Z;
		}
		else
		{
			const FVector Value = Distribution->GetValue(Time, nullptr, nullptr);
			LookupTable.Values[Offset + 0] = Value.X;
			LookupTable.Values[Offset + 1] = Value.Y;
			LookupTable.Values[Offset + 2] = Value.Z;
		}
	}
}

FVector UDistributionVectorConstant::GetValue(float /*Time*/, UObject* /*Data*/, FDistributionRandomStream* /*RandomStream*/) const
{
	return Constant;
}

void UDistributionVectorConstant::GetValueRange(float /*Time*/, FVector& OutMin, FVector& OutMax) const
{
	OutMin = Constant;
	OutMax = Constant;
}

void UDistributionVectorConstant::GetOutRange(FVector& OutMin, FVector& OutMax) const
{
	OutMin = Constant;
	OutMax = Constant;
}

FVector UDistributionVectorUniform::GetValue(float /*Time*/, UObject* /*Data*/, FDistributionRandomStream* RandomStream) const
{
	FVector RangeMin;
	FVector RangeMax;
	GetValueRange(0.0f, RangeMin, RangeMax);

	FVector Value = bUseExtremes
		? (DistributionRand(RandomStream) < 0.5f ? RangeMin : RangeMax)
		: LerpVector(RangeMin, RangeMax, FVector(
			DistributionRand(RandomStream),
			DistributionRand(RandomStream),
			DistributionRand(RandomStream)));
	ApplyLockedAxes(Value, LockedAxes);
	return Value;
}

void UDistributionVectorUniform::GetValueRange(float /*Time*/, FVector& OutMin, FVector& OutMax) const
{
	ApplyMirrorFlags(Min, Max, OutMin, OutMax, MirrorX, MirrorY, MirrorZ);
	ApplyLockedAxes(OutMin, LockedAxes);
	ApplyLockedAxes(OutMax, LockedAxes);
}

void UDistributionVectorUniform::GetOutRange(FVector& OutMin, FVector& OutMax) const
{
	GetValueRange(0.0f, OutMin, OutMax);
}

FVector UDistributionVectorConstantCurve::GetValue(float Time, UObject* /*Data*/, FDistributionRandomStream* /*RandomStream*/) const
{
	return FVector(XCurve.Evaluate(Time), YCurve.Evaluate(Time), ZCurve.Evaluate(Time));
}

void UDistributionVectorConstantCurve::GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const
{
	const FVector Value = GetValue(Time, nullptr, nullptr);
	OutMin = Value;
	OutMax = Value;
}

void UDistributionVectorConstantCurve::GetOutRange(FVector& OutMin, FVector& OutMax) const
{
	GetCurveValueRange(XCurve, OutMin.X, OutMax.X);
	GetCurveValueRange(YCurve, OutMin.Y, OutMax.Y);
	GetCurveValueRange(ZCurve, OutMin.Z, OutMax.Z);
}

void UDistributionVectorConstantCurve::GetTimeRange(float& OutMinTime, float& OutMaxTime) const
{
	GetCurveTimeRange(XCurve, OutMinTime, OutMaxTime);
	CombineTimeRange(YCurve, OutMinTime, OutMaxTime);
	CombineTimeRange(ZCurve, OutMinTime, OutMaxTime);
}

FVector UDistributionVectorUniformCurve::GetValue(float Time, UObject* /*Data*/, FDistributionRandomStream* RandomStream) const
{
	FVector RangeMin;
	FVector RangeMax;
	GetValueRange(Time, RangeMin, RangeMax);

	FVector Value = bUseExtremes
		? (DistributionRand(RandomStream) < 0.5f ? RangeMin : RangeMax)
		: LerpVector(RangeMin, RangeMax, FVector(
			DistributionRand(RandomStream),
			DistributionRand(RandomStream),
			DistributionRand(RandomStream)));
	ApplyLockedAxes(Value, LockedAxes);
	return Value;
}

void UDistributionVectorUniformCurve::GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const
{
	OutMin = FVector(MinXCurve.Evaluate(Time), MinYCurve.Evaluate(Time), MinZCurve.Evaluate(Time));
	OutMax = FVector(MaxXCurve.Evaluate(Time), MaxYCurve.Evaluate(Time), MaxZCurve.Evaluate(Time));
	if (OutMin.X > OutMax.X) std::swap(OutMin.X, OutMax.X);
	if (OutMin.Y > OutMax.Y) std::swap(OutMin.Y, OutMax.Y);
	if (OutMin.Z > OutMax.Z) std::swap(OutMin.Z, OutMax.Z);
	ApplyLockedAxes(OutMin, LockedAxes);
	ApplyLockedAxes(OutMax, LockedAxes);
}

void UDistributionVectorUniformCurve::GetOutRange(FVector& OutMin, FVector& OutMax) const
{
	GetCurveValueRange(MinXCurve, OutMin.X, OutMax.X);
	GetCurveValueRange(MinYCurve, OutMin.Y, OutMax.Y);
	GetCurveValueRange(MinZCurve, OutMin.Z, OutMax.Z);

	float MinValue = 0.0f;
	float MaxValue = 0.0f;
	GetCurveValueRange(MaxXCurve, MinValue, MaxValue);
	OutMin.X = std::min(OutMin.X, MinValue);
	OutMax.X = std::max(OutMax.X, MaxValue);
	GetCurveValueRange(MaxYCurve, MinValue, MaxValue);
	OutMin.Y = std::min(OutMin.Y, MinValue);
	OutMax.Y = std::max(OutMax.Y, MaxValue);
	GetCurveValueRange(MaxZCurve, MinValue, MaxValue);
	OutMin.Z = std::min(OutMin.Z, MinValue);
	OutMax.Z = std::max(OutMax.Z, MaxValue);
}

void UDistributionVectorUniformCurve::GetTimeRange(float& OutMinTime, float& OutMaxTime) const
{
	GetCurveTimeRange(MinXCurve, OutMinTime, OutMaxTime);
	CombineTimeRange(MinYCurve, OutMinTime, OutMaxTime);
	CombineTimeRange(MinZCurve, OutMinTime, OutMaxTime);
	CombineTimeRange(MaxXCurve, OutMinTime, OutMaxTime);
	CombineTimeRange(MaxYCurve, OutMinTime, OutMaxTime);
	CombineTimeRange(MaxZCurve, OutMinTime, OutMaxTime);
}

FVector UDistributionVectorParameterBase::GetValue(float /*Time*/, UObject* Data, FDistributionRandomStream* /*RandomStream*/) const
{
	FVector ParamValue = Constant;
	if (!GetParamValue(Data, ParameterName, ParamValue))
	{
		return Constant;
	}

	switch (ParamMode)
	{
	case EDistributionParamMode::Direct:
		return ParamValue;
	case EDistributionParamMode::Abs:
		ParamValue.X = std::fabs(ParamValue.X);
		ParamValue.Y = std::fabs(ParamValue.Y);
		ParamValue.Z = std::fabs(ParamValue.Z);
		break;
	case EDistributionParamMode::Normal:
	default:
		break;
	}

	return DistributionMapVector(ParamValue, MinInput, MaxInput, MinOutput, MaxOutput);
}

void UDistributionVectorParameterBase::GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const
{
	const FVector Value = GetValue(Time, nullptr, nullptr);
	OutMin = Value;
	OutMax = Value;
}

void UDistributionVectorParameterBase::GetOutRange(FVector& OutMin, FVector& OutMax) const
{
	OutMin = FVector(
		std::min(MinOutput.X, MaxOutput.X),
		std::min(MinOutput.Y, MaxOutput.Y),
		std::min(MinOutput.Z, MaxOutput.Z));
	OutMax = FVector(
		std::max(MinOutput.X, MaxOutput.X),
		std::max(MinOutput.Y, MaxOutput.Y),
		std::max(MinOutput.Z, MaxOutput.Z));
}

bool UDistributionVectorParameterBase::GetParamValue(UObject* /*Data*/, FName /*ParamName*/, FVector& /*OutVector*/) const
{
	return false;
}

bool UDistributionVectorParticleParameter::GetParamValue(UObject* Data, FName ParamName, FVector& OutVector) const
{
	if (const UParticleSystemComponent* Component = Cast<UParticleSystemComponent>(Data))
	{
		return Component->GetVectorParameter(ParamName, OutVector);
	}
	return false;
}
