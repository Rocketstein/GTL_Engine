#pragma once

#include "Core/CoreTypes.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Render/PostProcess/FadeController.h"

class FPostProcessController
{
public:
	void Reset();
	void Update(float DeltaTime);

	FFadeController& GetFadeController() { return FadeController; }
	const FFadeController& GetFadeController() const { return FadeController; }

	FPostProcessSettings GetSettings() const;
	void SetSettings(const FPostProcessSettings& InSettings);
	FPostProcessSettings& GetMutableSettings() { return Settings; }
	const FPostProcessSettings& GetBaseSettings() const { return Settings; }

	const FVignettingSettings& GetVignettingSettings() const { return Settings.Vignetting; }
	void SetVignettingSettings(const FVignettingSettings& InSettings);
	void SetVignettingEnabled(bool bEnabled);
	void SetVignetting(float Intensity, float Radius, float Softness, const FVector& Color);
	void SetVignettingColor(const FVector& Color);

	const FGammaCorrectionSettings& GetGammaCorrectionSettings() const { return Settings.GammaCorrection; }
	void SetGammaCorrectionSettings(const FGammaCorrectionSettings& InSettings);
	void SetGammaCorrectionEnabled(bool bEnabled);
	void SetDisplayGamma(float DisplayGamma);

	const FLetterboxSettings& GetLetterboxSettings() const { return Settings.Letterbox; }
	void SetLetterboxSettings(const FLetterboxSettings& InSettings);
	void SetLetterboxEnabled(bool bEnabled);
	void SetLetterbox(float TargetAspectRatio, float Opacity, const FVector& Color);
	void SetLetterboxColor(const FVector& Color);

private:
	FPostProcessSettings Settings;
	FFadeController      FadeController;
};
