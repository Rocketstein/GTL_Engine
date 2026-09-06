#pragma once
#include "SimpleCameraShakePattern.h"


struct FPerlinNoiseShaker
{
	float Amplitude;
	float Frequency;

	FPerlinNoiseShaker()
		: Amplitude(1.f)
		, Frequency(1.f)
	{
	}

	float Update(float DeltaTime, float AmplitudeMultiplier, float FrequencyMultiplier, float& InOutCurrentOffset) const;
};

class UPerlinNoiseCameraShakePattern : public USimpleCameraShakePattern
{
public:
	DECLARE_CLASS(UPerlinNoiseCameraShakePattern, USimpleCameraShakePattern)

	UPerlinNoiseCameraShakePattern();

	float LocationAmplitudeMultiplier = 1.f;
	float LocationFrequencyMultiplier = 1.f;
	FPerlinNoiseShaker X;
	FPerlinNoiseShaker Y;
	FPerlinNoiseShaker Z;
	float RotationAmplitudeMultiplier = 1.f;
	float RotationFrequencyMultiplier = 1.f;
	FPerlinNoiseShaker Pitch;
	FPerlinNoiseShaker Yaw;
	FPerlinNoiseShaker Roll;
	FPerlinNoiseShaker FOV;

private:
	void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult) override;

	void UpdatePerlinNoise(float DeltaTime, FCameraShakePatternUpdateResult& OutResult);

private:
	/** Initial perlin noise offset for location oscillation. */
	FVector InitialLocationOffset;
	/** Current perlin noise offset for location oscillation. */
	FVector CurrentLocationOffset;

	/** Initial perlin noise offset for rotation oscillation. */
	FVector InitialRotationOffset;
	/** Current perlin noise offset for rotation oscillation. */
	FVector CurrentRotationOffset;

	/** Initial perlin noise offset for FOV oscillation */
	float InitialFOVOffset;
	/** Current perlin noise offset for FOV oscillation */
	float CurrentFOVOffset;
};
