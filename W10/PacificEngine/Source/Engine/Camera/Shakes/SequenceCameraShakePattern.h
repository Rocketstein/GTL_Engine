#pragma once

#include "SimpleCameraShakePattern.h"

#include <cstddef>

enum class ESequenceCameraShakeChannel : uint8
{
    X = 0,
    Y,
    Z,
    Pitch,
    Yaw,
    Roll,
    FOV,
    Count
};

constexpr size_t SequenceCameraShakeChannelCount = static_cast<size_t>(ESequenceCameraShakeChannel::Count);

struct FSequenceCameraShakeKey
{
    float Time = 0.f;
    float Value = 0.f;
    float ArriveTime = 0.f;
    float ArriveValue = 0.f;
    float LeaveTime = 0.f;
    float LeaveValue = 0.f;
};

using FSequenceCameraShakeCurve = TArray<FSequenceCameraShakeKey>;
using FSequenceCameraShakeCurves = TStaticArray<FSequenceCameraShakeCurve, SequenceCameraShakeChannelCount>;

const char* GetSequenceCameraShakeChannelName(ESequenceCameraShakeChannel Channel);

class USequenceCameraShakePattern : public USimpleCameraShakePattern
{
public:
    DECLARE_CLASS(USequenceCameraShakePattern, USimpleCameraShakePattern)

    float PlayRate = 1.f;
    FSequenceCameraShakeCurves Curves;

    static void NormalizeCurve(FSequenceCameraShakeCurve& Curve, float MaxDuration);
    static float EvaluateCurve(const FSequenceCameraShakeCurve& Curve, float Time);

private:
    void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
    void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult) override;

    void EvaluateAtTime(float Time, FCameraShakePatternUpdateResult& OutResult) const;
};
