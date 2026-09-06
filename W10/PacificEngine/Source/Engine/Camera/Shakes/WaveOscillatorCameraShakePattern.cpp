#include "WaveOscillatorCameraShakePattern.h"

#include "Object/ObjectFactory.h"

float FWaveOscillator::Initialize(float& OutInitializeOffset) const
{
	OutInitializeOffset = (InitialOffsetType == EInitialWaveOscillatorOffsetType::Random)
		? FMath::FRand() * (2.f * FMath::Pi)
		: 0.f;
	return Amplitude * sinf(OutInitializeOffset);
}

float FWaveOscillator::Update(float DeltaTime, float AmplitudeMultiplier, float FrequencyMultiplier, float& InOutCurrentOffset) const
{
	const float TotalAmplitude = Amplitude * AmplitudeMultiplier;
	if (TotalAmplitude != 0.f)
	{
		InOutCurrentOffset += DeltaTime * Frequency * FrequencyMultiplier * (2.f * FMath::Pi);
		return TotalAmplitude * sinf(InOutCurrentOffset);
	}
	return 0.f;
}

UWaveOscillatorCameraShakePattern::UWaveOscillatorCameraShakePattern()
{
	RotationAmplitudeMultiplier = 0.f;
	FOV.Amplitude = 0.f;
}

void UWaveOscillatorCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
    Super::StartShakePatternImpl(Params);

	if (!Params.bIsRestarting)
	{
		X.Initialize(InitialLocationOffset.X);
		Y.Initialize(InitialLocationOffset.Y);
		Z.Initialize(InitialLocationOffset.Z);

		CurrentLocationOffset = InitialLocationOffset;

		Pitch.Initialize(InitialRotationOffset.X);
		Yaw.Initialize(InitialRotationOffset.Y);
		Roll.Initialize(InitialRotationOffset.Z);

		CurrentRotationOffset = InitialRotationOffset;

		FOV.Initialize(InitialFOVOffset);

		CurrentFOVOffset = InitialFOVOffset;
	}
}

void UWaveOscillatorCameraShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	UpdateOscillators(Params.DeltaTime, OutResult);

	const float BlendWeight = State.Update(Params.DeltaTime);
	OutResult.ApplyScale(BlendWeight);
}

void UWaveOscillatorCameraShakePattern::ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	// Scrubbing is like going back to our initial state and updating directly to the scrub time.
	CurrentLocationOffset = InitialLocationOffset;
	CurrentRotationOffset = InitialRotationOffset;
	CurrentFOVOffset = InitialFOVOffset;

	UpdateOscillators(Params.AbsoluteTime, OutResult);

	const float BlendWeight = State.Scrub(Params.AbsoluteTime);
	OutResult.ApplyScale(BlendWeight);
}

void UWaveOscillatorCameraShakePattern::UpdateOscillators(float DeltaTime, FCameraShakePatternUpdateResult& OutResult)
{
	OutResult.Location.X = X.Update(DeltaTime, LocationAmplitudeMultiplier, LocationFrequencyMultiplier, CurrentLocationOffset.X);
	OutResult.Location.Y = Y.Update(DeltaTime, LocationAmplitudeMultiplier, LocationFrequencyMultiplier, CurrentLocationOffset.Y);
	OutResult.Location.Z = Z.Update(DeltaTime, LocationAmplitudeMultiplier, LocationFrequencyMultiplier, CurrentLocationOffset.Z);

	OutResult.Rotation.Pitch = Pitch.Update(DeltaTime, RotationAmplitudeMultiplier, RotationFrequencyMultiplier, CurrentRotationOffset.X);
	OutResult.Rotation.Yaw = Yaw.Update(DeltaTime, RotationAmplitudeMultiplier, RotationFrequencyMultiplier, CurrentRotationOffset.Y);
	OutResult.Rotation.Roll = Roll.Update(DeltaTime, RotationAmplitudeMultiplier, RotationFrequencyMultiplier, CurrentRotationOffset.Z);

	OutResult.FOV = FOV.Update(DeltaTime, 1.f, 1.f, CurrentFOVOffset);
}

IMPLEMENT_CLASS(UWaveOscillatorCameraShakePattern, USimpleCameraShakePattern)