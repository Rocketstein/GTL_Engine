#include "Render/PostProcess/FadeController.h"

#include "Math/MathUtils.h"

void FFadeController::Reset()
{
	bPlaying      = false;
	ElapsedTime   = 0.0f;
	CurrentAlpha  = 0.0f;
	CurrentColor  = FVector(0.0f, 0.0f, 0.0f);
	ActiveRequest = {};
}

void FFadeController::SetAlpha(float Alpha, const FVector& Color)
{
	bPlaying      = false;
	ElapsedTime   = 0.0f;
	CurrentAlpha  = Clamp01(Alpha);
	CurrentColor  = Color;
	ActiveRequest = {};
}

void FFadeController::FadeIn(float Duration, const FVector& Color, EFadeCurve Curve)
{
	FadeTo(0.0f, Duration, Color, Curve);
}

void FFadeController::FadeOut(float Duration, const FVector& Color, EFadeCurve Curve)
{
	FadeTo(1.0f, Duration, Color, Curve);
}

void FFadeController::FadeTo(float TargetAlpha, float Duration, const FVector& Color, EFadeCurve Curve)
{
	FFadeRequest Request = {};
	Request.FromAlpha    = CurrentAlpha;
	Request.ToAlpha      = Clamp01(TargetAlpha);
	Request.Duration     = Duration;
	Request.Color        = Color;
	Request.Curve        = Curve;

	Play(Request);
}

void FFadeController::Play(const FFadeRequest& Request)
{
	ActiveRequest           = Request;
	ActiveRequest.FromAlpha = Clamp01(ActiveRequest.FromAlpha);
	ActiveRequest.ToAlpha   = Clamp01(ActiveRequest.ToAlpha);
	ActiveRequest.Duration  = ActiveRequest.Duration > 0.0f ? ActiveRequest.Duration : 0.0f;

	ElapsedTime  = 0.0f;
	CurrentColor = ActiveRequest.Color;

	if (ActiveRequest.Duration <= FMath::Epsilon)
	{
		CurrentAlpha = ActiveRequest.ToAlpha;
		bPlaying     = false;
		return;
	}

	CurrentAlpha = ActiveRequest.FromAlpha;
	bPlaying     = true;
}

void FFadeController::Stop()
{
	bPlaying    = false;
	ElapsedTime = 0.0f;
}

void FFadeController::Update(float DeltaTime)
{
	if (!bPlaying)
	{
		return;
	}

	if (DeltaTime > 0.0f)
	{
		ElapsedTime += DeltaTime;
	}

	const float NormalizedTime = Clamp01(ElapsedTime / ActiveRequest.Duration);
	const float Alpha          = EvaluateCurve(ActiveRequest.Curve, NormalizedTime);

	CurrentAlpha = FMath::Lerp(ActiveRequest.FromAlpha, ActiveRequest.ToAlpha, Alpha);
	CurrentColor = ActiveRequest.Color;

	if (NormalizedTime >= 1.0f)
	{
		CurrentAlpha = ActiveRequest.ToAlpha;
		bPlaying     = false;
	}
}

FFadeSettings FFadeController::GetSettings() const
{
	FFadeSettings Settings = {};
	Settings.bEnabled     = bPlaying || CurrentAlpha > FMath::Epsilon;
	Settings.Alpha        = Clamp01(CurrentAlpha);
	Settings.Color        = CurrentColor;

	return Settings;
}

float FFadeController::Clamp01(float Value)
{
	return FMath::Clamp(Value, 0.0f, 1.0f);
}

float FFadeController::EvaluateCurve(EFadeCurve Curve, float T)
{
	T = Clamp01(T);

	switch (Curve)
	{
	case EFadeCurve::EaseIn:
		return T * T;
	case EFadeCurve::EaseOut:
		return 1.0f - ((1.0f - T) * (1.0f - T));
	case EFadeCurve::EaseInOut:
		return T * T * (3.0f - 2.0f * T);
	case EFadeCurve::Linear:
	default:
		return T;
	}
}
