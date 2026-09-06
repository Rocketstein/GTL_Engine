#include "CameraModifier_CameraShake.h"
#include "Object/ObjectFactory.h"

#include <algorithm>

IMPLEMENT_CLASS(UCameraModifier_CameraShake, UCameraModifier)

UCameraModifier_CameraShake::~UCameraModifier_CameraShake()
{
	ClearCameraShakes();
}

void UCameraModifier_CameraShake::AddedToCamera(APlayerCameraManager* Camera)
{
	UCameraModifier::AddedToCamera(Camera);
}

void UCameraModifier_CameraShake::SetCameraShake(UCameraShakeBase* InShake)
{
	ClearCameraShakes();
	AddCameraShake(InShake);
}

void UCameraModifier_CameraShake::AddCameraShake(UCameraShakeBase* InShake)
{
	if (!InShake)
	{
		return;
	}

	ActiveShakes.push_back(InShake);
	EnableModifier();
}

UCameraShakeBase* UCameraModifier_CameraShake::GetCameraShake() const
{
	return ActiveShakes.empty() ? nullptr : ActiveShakes.front();
}

void UCameraModifier_CameraShake::RemoveFinishedShakes()
{
	ActiveShakes.erase(
		std::remove_if(ActiveShakes.begin(), ActiveShakes.end(),
			[](UCameraShakeBase* Shake)
			{
				if (!Shake)
				{
					return true;
				}

				if (Shake->IsFinished())
				{
					UObjectManager::Get().DestroyObject(Shake);
					return true;
				}

				return false;
			}),
		ActiveShakes.end());
}

void UCameraModifier_CameraShake::ClearCameraShakes()
{
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		if (Shake)
		{
			UObjectManager::Get().DestroyObject(Shake);
		}
	}

	ActiveShakes.clear();
}

bool UCameraModifier_CameraShake::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	RemoveFinishedShakes();
	if (ActiveShakes.empty())
	{
		DisableModifier(true);
		return false;
	}

	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		if (!Shake)
		{
			continue;
		}

		FVector LocationOffset = FVector::ZeroVector;
		FRotator RotationOffset = FRotator::ZeroRotator;
		Shake->UpdateShake(DeltaTime, LocationOffset, RotationOffset);

		InOutPOV.Location += LocationOffset * TransitionIntensity * GetAlpha();
		InOutPOV.Rotation += RotationOffset * RotationIntensity * GetAlpha();
	}

	RemoveFinishedShakes();
	if (ActiveShakes.empty())
	{
		DisableModifier();
	}

	return false;
}

bool UCameraModifier_CameraShake::IsFinished() const
{
	return UCameraModifier::IsFinished();
}
