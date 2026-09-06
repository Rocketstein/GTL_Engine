#include "Camera/CameraShake.h"

IMPLEMENT_CLASS(UCameraShakeBase, UObject)

UCameraShakeBase::~UCameraShakeBase()
{
	if (RootShakePattern)
	{
		UObjectManager::Get().DestroyObject(RootShakePattern);
		RootShakePattern = nullptr;
	}
}

void UCameraShakeBase::UpdateShake(float DeltaTime, FVector& OutLoc, FRotator& OutRot)
{
	ElapsedTime += DeltaTime;

	const float ShakeAlpha = Duration > 0.0f ? ElapsedTime / Duration : 1.0f;
	const float ClampedAlpha = ShakeAlpha > 1.0f ? 1.0f : ShakeAlpha;
	const float FadeOutAlpha = 1.0f - ClampedAlpha;

	if (!RootShakePattern)
	{
		OutLoc = FVector::ZeroVector;
		OutRot = FRotator::ZeroRotator;
		return;
	}

	OutLoc = FVector(
		RootShakePattern->EvalTransitionX(ClampedAlpha),
		RootShakePattern->EvalTransitionY(ClampedAlpha),
		RootShakePattern->EvalTransitionZ(ClampedAlpha)) * Intensity * FadeOutAlpha;

	OutRot = FRotator(
		RootShakePattern->EvalRotationX(ClampedAlpha),
		RootShakePattern->EvalRotationY(ClampedAlpha),
		RootShakePattern->EvalRotationZ(ClampedAlpha)) * Intensity * FadeOutAlpha;
}
