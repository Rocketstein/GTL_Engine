#pragma once
#include "SimpleCameraShakePattern.h"

enum class EInitialWaveOscillatorOffsetType : uint8
{
	/** Start with random offset (default). */
    Random,
	/** Start with zero offset. */
	Zero
};

struct FWaveOscillator
{
	float Amplitude;

	float Frequency;

	EInitialWaveOscillatorOffsetType InitialOffsetType;

	FWaveOscillator()
	    : Amplitude(1.f)
	    , Frequency(1.f)
        , InitialOffsetType(EInitialWaveOscillatorOffsetType::Random)
    {}

	float Initialize(float& OutInitializeOffset) const;

	float Update(float DeltaTime, float AmplitudeMultiplier, float FrequencyMultiplier, float& InOutCurrentOffset) const;
};

class UWaveOscillatorCameraShakePattern : public USimpleCameraShakePattern
{
public:
	DECLARE_CLASS(UWaveOscillatorCameraShakePattern, USimpleCameraShakePattern)

	UWaveOscillatorCameraShakePattern();

public:
	float LocationAmplitudeMultiplier = 1.f;
	float LocationFrequencyMultiplier = 1.f;

	FWaveOscillator X;
	FWaveOscillator Y;
	FWaveOscillator Z;

	float RotationAmplitudeMultiplier = 1.f;
	float RotationFrequencyMultiplier = 1.f;

	FWaveOscillator Pitch;
	FWaveOscillator Yaw;
	FWaveOscillator Roll;
	FWaveOscillator FOV;

private:
	void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult) override;

	void UpdateOscillators(float DeltaTime, FCameraShakePatternUpdateResult& OutResult);

private:
	FVector InitialLocationOffset;
	FVector CurrentLocationOffset;

	FVector InitialRotationOffset;
	FVector CurrentRotationOffset;

	float InitialFOVOffset;
	float CurrentFOVOffset;
};
