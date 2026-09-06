#include "CameraShakePattern.h"

#include <cmath>

IMPLEMENT_CLASS(UCameraShakePattern, UObject)
IMPLEMENT_CLASS(USinWaveCameraShakePattern, UCameraShakePattern)
IMPLEMENT_CLASS(UCurveCameraShakePattern, UCameraShakePattern)

float UCameraShakePattern::EvalTransitionX(float Alpha) const
{
	return 0.0f;
}

float UCameraShakePattern::EvalTransitionY(float Alpha) const
{
	return 0.0f;
}

float UCameraShakePattern::EvalTransitionZ(float Alpha) const
{
	return 0.0f;
}

float UCameraShakePattern::EvalRotationX(float Alpha) const
{
	return 0.0f;
}

float UCameraShakePattern::EvalRotationY(float Alpha) const
{
	return 0.0f;
}

float UCameraShakePattern::EvalRotationZ(float Alpha) const
{
	return 0.0f;
}

float USinWaveCameraShakePattern::EvalTransitionX(float Alpha) const
{
	return sinf(Alpha * Frequency) * TransitionAmplitudeX;
}

float USinWaveCameraShakePattern::EvalTransitionY(float Alpha) const
{
	return sinf(Alpha * Frequency) * TransitionAmplitudeY;
}

float USinWaveCameraShakePattern::EvalTransitionZ(float Alpha) const
{
	return sinf(Alpha * Frequency) * TransitionAmplitudeZ;
}

float USinWaveCameraShakePattern::EvalRotationX(float Alpha) const
{
	return sinf(Alpha * Frequency) * RotationAmplitudeX;
}

float USinWaveCameraShakePattern::EvalRotationY(float Alpha) const
{
	return sinf(Alpha * Frequency) * RotationAmplitudeY;
}

float USinWaveCameraShakePattern::EvalRotationZ(float Alpha) const
{
	return sinf(Alpha * Frequency) * RotationAmplitudeZ;
}

UCurveCameraShakePattern::~UCurveCameraShakePattern()
{
	if (TransitionCurveX)
	{
		UObjectManager::Get().DestroyObject(TransitionCurveX);
		TransitionCurveX = nullptr;
	}

	if (TransitionCurveY)
	{
		UObjectManager::Get().DestroyObject(TransitionCurveY);
		TransitionCurveY = nullptr;
	}

	if (TransitionCurveZ)
	{
		UObjectManager::Get().DestroyObject(TransitionCurveZ);
		TransitionCurveZ = nullptr;
	}

	if (RotationCurveX)
	{
		UObjectManager::Get().DestroyObject(RotationCurveX);
		RotationCurveX = nullptr;
	}

	if (RotationCurveY)
	{
		UObjectManager::Get().DestroyObject(RotationCurveY);
		RotationCurveY = nullptr;
	}

	if (RotationCurveZ)
	{
		UObjectManager::Get().DestroyObject(RotationCurveZ);
		RotationCurveZ = nullptr;
	}
}

float UCurveCameraShakePattern::EvalTransitionX(float Alpha) const
{
	return TransitionCurveX ? TransitionCurveX->Evaluate(Alpha) : 0.0f;
}

float UCurveCameraShakePattern::EvalTransitionY(float Alpha) const
{
	return TransitionCurveY ? TransitionCurveY->Evaluate(Alpha) : 0.0f;
}

float UCurveCameraShakePattern::EvalTransitionZ(float Alpha) const
{
	return TransitionCurveZ ? TransitionCurveZ->Evaluate(Alpha) : 0.0f;
}

float UCurveCameraShakePattern::EvalRotationX(float Alpha) const
{
	return RotationCurveX ? RotationCurveX->Evaluate(Alpha) : 0.0f;
}

float UCurveCameraShakePattern::EvalRotationY(float Alpha) const
{
	return RotationCurveY ? RotationCurveY->Evaluate(Alpha) : 0.0f;
}

float UCurveCameraShakePattern::EvalRotationZ(float Alpha) const
{
	return RotationCurveZ ? RotationCurveZ->Evaluate(Alpha) : 0.0f;
}
