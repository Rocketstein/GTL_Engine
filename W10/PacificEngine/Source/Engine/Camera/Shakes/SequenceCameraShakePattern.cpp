#include "SequenceCameraShakePattern.h"

#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float MinKeyTimeDelta = 0.001f;

float Cubic(float P0, float P1, float P2, float P3, float T)
{
    const float OneMinusT = 1.f - T;
    return OneMinusT * OneMinusT * OneMinusT * P0
        + 3.f * OneMinusT * OneMinusT * T * P1
        + 3.f * OneMinusT * T * T * P2
        + T * T * T * P3;
}

float ClampFinite(float Value, float DefaultValue)
{
    return std::isfinite(Value) ? Value : DefaultValue;
}

float ClampHandleTime(float Value, float MinValue, float MaxValue)
{
    if (MaxValue < MinValue)
    {
        return MinValue;
    }
    return FMath::Clamp(Value, MinValue, MaxValue);
}
} // namespace

const char* GetSequenceCameraShakeChannelName(ESequenceCameraShakeChannel Channel)
{
    switch (Channel)
    {
    case ESequenceCameraShakeChannel::X:
        return "X";
    case ESequenceCameraShakeChannel::Y:
        return "Y";
    case ESequenceCameraShakeChannel::Z:
        return "Z";
    case ESequenceCameraShakeChannel::Pitch:
        return "Pitch";
    case ESequenceCameraShakeChannel::Yaw:
        return "Yaw";
    case ESequenceCameraShakeChannel::Roll:
        return "Roll";
    case ESequenceCameraShakeChannel::FOV:
        return "FOV";
    default:
        return "Unknown";
    }
}

void USequenceCameraShakePattern::NormalizeCurve(FSequenceCameraShakeCurve& Curve, float MaxDuration)
{
    for (FSequenceCameraShakeKey& Key : Curve)
    {
        Key.Time = ClampFinite(Key.Time, 0.f);
        Key.Value = ClampFinite(Key.Value, 0.f);
        Key.ArriveTime = ClampFinite(Key.ArriveTime, 0.f);
        Key.ArriveValue = ClampFinite(Key.ArriveValue, 0.f);
        Key.LeaveTime = ClampFinite(Key.LeaveTime, 0.f);
        Key.LeaveValue = ClampFinite(Key.LeaveValue, 0.f);

        Key.Time = (std::max)(Key.Time, 0.f);
        if (MaxDuration > 0.f)
        {
            Key.Time = (std::min)(Key.Time, MaxDuration);
        }
    }

    std::sort(Curve.begin(), Curve.end(), [](const FSequenceCameraShakeKey& A, const FSequenceCameraShakeKey& B)
              {
                  return A.Time < B.Time;
              });

    for (size_t Index = 1; Index < Curve.size(); ++Index)
    {
        const float MinTime = Curve[Index - 1].Time + MinKeyTimeDelta;
        if (Curve[Index].Time < MinTime)
        {
            Curve[Index].Time = MaxDuration > 0.f ? (std::min)(MinTime, MaxDuration) : MinTime;
        }
    }

    for (size_t Index = 0; Index < Curve.size(); ++Index)
    {
        FSequenceCameraShakeKey& Key = Curve[Index];

        const float PrevSpan = Index > 0 ? (std::max)(Key.Time - Curve[Index - 1].Time, 0.f) : 0.f;
        const float NextSpan = Index + 1 < Curve.size() ? (std::max)(Curve[Index + 1].Time - Key.Time, 0.f) : 0.f;

        Key.ArriveTime = ClampHandleTime(Key.ArriveTime, -PrevSpan, 0.f);
        Key.LeaveTime = ClampHandleTime(Key.LeaveTime, 0.f, NextSpan);
    }
}

float USequenceCameraShakePattern::EvaluateCurve(const FSequenceCameraShakeCurve& Curve, float Time)
{
    if (Curve.empty())
    {
        return 0.f;
    }

    if (Curve.size() == 1 || Time <= Curve.front().Time)
    {
        return Curve.front().Value;
    }

    if (Time >= Curve.back().Time)
    {
        return Curve.back().Value;
    }

    for (size_t Index = 0; Index + 1 < Curve.size(); ++Index)
    {
        const FSequenceCameraShakeKey& Start = Curve[Index];
        const FSequenceCameraShakeKey& End = Curve[Index + 1];
        if (Time > End.Time)
        {
            continue;
        }

        const float SegmentDuration = End.Time - Start.Time;
        if (SegmentDuration <= FMath::Epsilon)
        {
            return End.Value;
        }

        const float X0 = Start.Time;
        const float X3 = End.Time;
        const float X1 = FMath::Clamp(Start.Time + Start.LeaveTime, X0, X3);
        const float X2 = FMath::Clamp(End.Time + End.ArriveTime, X0, X3);
        const float Y0 = Start.Value;
        const float Y1 = Start.Value + Start.LeaveValue;
        const float Y2 = End.Value + End.ArriveValue;
        const float Y3 = End.Value;

        float Low = 0.f;
        float High = 1.f;
        float T = FMath::Clamp((Time - Start.Time) / SegmentDuration, 0.f, 1.f);
        for (int Iteration = 0; Iteration < 18; ++Iteration)
        {
            T = (Low + High) * 0.5f;
            const float X = Cubic(X0, X1, X2, X3, T);
            if (X < Time)
            {
                Low = T;
            }
            else
            {
                High = T;
            }
        }

        return Cubic(Y0, Y1, Y2, Y3, (Low + High) * 0.5f);
    }

    return Curve.back().Value;
}

void USequenceCameraShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
    const float BlendWeight = State.Update(Params.DeltaTime);
    const float SampleTime = State.GetElapsedTime() * (std::max)(PlayRate, 0.f);

    EvaluateAtTime(SampleTime, OutResult);
    OutResult.ApplyScale(BlendWeight);
}

void USequenceCameraShakePattern::ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
    const float BlendWeight = State.Scrub(Params.AbsoluteTime);
    const float SampleTime = Params.AbsoluteTime * (std::max)(PlayRate, 0.f);

    EvaluateAtTime(SampleTime, OutResult);
    OutResult.ApplyScale(BlendWeight);
}

void USequenceCameraShakePattern::EvaluateAtTime(float Time, FCameraShakePatternUpdateResult& OutResult) const
{
    OutResult.Location.X = EvaluateCurve(Curves[static_cast<size_t>(ESequenceCameraShakeChannel::X)], Time);
    OutResult.Location.Y = EvaluateCurve(Curves[static_cast<size_t>(ESequenceCameraShakeChannel::Y)], Time);
    OutResult.Location.Z = EvaluateCurve(Curves[static_cast<size_t>(ESequenceCameraShakeChannel::Z)], Time);

    OutResult.Rotation.Pitch = EvaluateCurve(Curves[static_cast<size_t>(ESequenceCameraShakeChannel::Pitch)], Time);
    OutResult.Rotation.Yaw = EvaluateCurve(Curves[static_cast<size_t>(ESequenceCameraShakeChannel::Yaw)], Time);
    OutResult.Rotation.Roll = EvaluateCurve(Curves[static_cast<size_t>(ESequenceCameraShakeChannel::Roll)], Time);

    OutResult.FOV = EvaluateCurve(Curves[static_cast<size_t>(ESequenceCameraShakeChannel::FOV)], Time);
}

IMPLEMENT_CLASS(USequenceCameraShakePattern, USimpleCameraShakePattern)
