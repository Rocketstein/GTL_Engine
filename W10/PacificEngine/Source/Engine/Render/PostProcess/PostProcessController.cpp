#include "Render/PostProcess/PostProcessController.h"

#include "Math/MathUtils.h"

namespace
{
float Clamp01(float Value)
{
	return FMath::Clamp(Value, 0.0f, 1.0f);
}

FVignettingSettings SanitizeVignettingSettings(FVignettingSettings Settings)
{
	Settings.Intensity = FMath::Clamp(Settings.Intensity, 0.0f, 10.0f);
	Settings.Radius    = Clamp01(Settings.Radius);
	Settings.Softness  = FMath::Clamp(Settings.Softness, FMath::Epsilon, 1.0f);
	return Settings;
}

FGammaCorrectionSettings SanitizeGammaCorrectionSettings(FGammaCorrectionSettings Settings)
{
	Settings.DisplayGamma = Settings.DisplayGamma > FMath::Epsilon ? Settings.DisplayGamma : 2.2f;
	return Settings;
}

FLetterboxSettings SanitizeLetterboxSettings(FLetterboxSettings Settings)
{
	Settings.TargetAspectRatio = Settings.TargetAspectRatio > FMath::Epsilon ? Settings.TargetAspectRatio : 2.39f;
	Settings.Opacity           = Clamp01(Settings.Opacity);
	return Settings;
}

FFadeSettings SanitizeFadeSettings(FFadeSettings Settings)
{
	Settings.Alpha = Clamp01(Settings.Alpha);
	return Settings;
}
} // namespace

void FPostProcessController::Reset()
{
	Settings = FPostProcessSettings{};
	FadeController.Reset();
}

void FPostProcessController::Update(float DeltaTime)
{
	FadeController.Update(DeltaTime);
}

FPostProcessSettings FPostProcessController::GetSettings() const
{
	FPostProcessSettings Result = Settings;
	Result.Fade                 = FadeController.GetSettings();
	return Result;
}

void FPostProcessController::SetSettings(const FPostProcessSettings& InSettings)
{
	Settings                 = InSettings;
	Settings.Vignetting     = SanitizeVignettingSettings(Settings.Vignetting);
	Settings.GammaCorrection = SanitizeGammaCorrectionSettings(Settings.GammaCorrection);
	Settings.Letterbox      = SanitizeLetterboxSettings(Settings.Letterbox);
	Settings.Fade           = SanitizeFadeSettings(Settings.Fade);

	if (Settings.Fade.bEnabled)
	{
		FadeController.SetAlpha(Settings.Fade.Alpha, Settings.Fade.Color);
	}
	else
	{
		FadeController.Reset();
	}
}

void FPostProcessController::SetVignettingSettings(const FVignettingSettings& InSettings)
{
	Settings.Vignetting = SanitizeVignettingSettings(InSettings);
}

void FPostProcessController::SetVignettingEnabled(bool bEnabled)
{
	Settings.Vignetting.bEnabled = bEnabled;
}

void FPostProcessController::SetVignetting(float Intensity, float Radius, float Softness, const FVector& Color)
{
	Settings.Vignetting.bEnabled  = true;
	Settings.Vignetting.Intensity = Intensity;
	Settings.Vignetting.Radius    = Radius;
	Settings.Vignetting.Softness  = Softness;
	Settings.Vignetting.Color     = Color;
	Settings.Vignetting           = SanitizeVignettingSettings(Settings.Vignetting);
}

void FPostProcessController::SetVignettingColor(const FVector& Color)
{
	Settings.Vignetting.Color = Color;
}

void FPostProcessController::SetGammaCorrectionSettings(const FGammaCorrectionSettings& InSettings)
{
	Settings.GammaCorrection = SanitizeGammaCorrectionSettings(InSettings);
}

void FPostProcessController::SetGammaCorrectionEnabled(bool bEnabled)
{
	Settings.GammaCorrection.bEnabled = bEnabled;
}

void FPostProcessController::SetDisplayGamma(float DisplayGamma)
{
	Settings.GammaCorrection.bEnabled     = true;
	Settings.GammaCorrection.DisplayGamma = DisplayGamma > FMath::Epsilon ? DisplayGamma : 2.2f;
}

void FPostProcessController::SetLetterboxSettings(const FLetterboxSettings& InSettings)
{
	Settings.Letterbox = SanitizeLetterboxSettings(InSettings);
}

void FPostProcessController::SetLetterboxEnabled(bool bEnabled)
{
	Settings.Letterbox.bEnabled = bEnabled;
}

void FPostProcessController::SetLetterbox(float TargetAspectRatio, float Opacity, const FVector& Color)
{
	Settings.Letterbox.bEnabled          = true;
	Settings.Letterbox.TargetAspectRatio = TargetAspectRatio;
	Settings.Letterbox.Opacity           = Opacity;
	Settings.Letterbox.Color             = Color;
	Settings.Letterbox                   = SanitizeLetterboxSettings(Settings.Letterbox);
}

void FPostProcessController::SetLetterboxColor(const FVector& Color)
{
	Settings.Letterbox.Color = Color;
}
