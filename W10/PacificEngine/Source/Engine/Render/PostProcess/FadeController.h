#pragma once

#include "Core/CoreTypes.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"

enum class EFadeCurve : uint8
{
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut,
};

struct FFadeRequest
{
	float      FromAlpha = 0.0f;
	float      ToAlpha   = 0.0f;
	float      Duration  = 0.0f;
	FVector    Color     = FVector(0.0f, 0.0f, 0.0f);
	EFadeCurve Curve     = EFadeCurve::EaseInOut;
};

class FFadeController
{
public:
	void Reset();
	void SetAlpha(float Alpha, const FVector& Color = FVector(0.0f, 0.0f, 0.0f));

	void FadeIn(float Duration, const FVector& Color = FVector(0.0f, 0.0f, 0.0f), EFadeCurve Curve = EFadeCurve::EaseInOut);
	void FadeOut(float Duration, const FVector& Color = FVector(0.0f, 0.0f, 0.0f), EFadeCurve Curve = EFadeCurve::EaseInOut);
	void FadeTo(float TargetAlpha, float Duration, const FVector& Color = FVector(0.0f, 0.0f, 0.0f), EFadeCurve Curve = EFadeCurve::EaseInOut);
	void Play(const FFadeRequest& Request);
	void Stop();

	void Update(float DeltaTime);

	bool IsPlaying() const { return bPlaying; }
	float GetAlpha() const { return CurrentAlpha; }
	float GetElapsedTime() const { return ElapsedTime; }
	float GetDuration() const { return ActiveRequest.Duration; }
	const FVector& GetColor() const { return CurrentColor; }

	FFadeSettings GetSettings() const;

private:
	static float Clamp01(float Value);
	static float EvaluateCurve(EFadeCurve Curve, float T);

	bool         bPlaying      = false;
	float        ElapsedTime   = 0.0f;
	float        CurrentAlpha  = 0.0f;
	FVector      CurrentColor  = FVector(0.0f, 0.0f, 0.0f);
	FFadeRequest ActiveRequest = {};
};
