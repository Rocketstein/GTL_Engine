#include "CameraModifier.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier, UObject)

UCameraModifier::UCameraModifier() {}

UCameraModifier::~UCameraModifier() {}

void UCameraModifier::AddedToCamera(APlayerCameraManager* InCameraManager)
{
	CameraOwner = InCameraManager;
}

bool UCameraModifier::IsFinished() const
{
	return !bPendingDisable && bDisabled;
}

void UCameraModifier::EnableModifier()
{
	bDisabled = false;
	bPendingDisable = false;
	if (Alpha <= 0.0f)
	{
		Alpha = (AlphaInTime <= 0.0f) ? 1.0f : 0.0f;
	}
}

void UCameraModifier::ToggleModifier()
{
	if (bDisabled || bPendingDisable)
	{
		EnableModifier();
	}
	else
	{
		DisableModifier();
	}
}

void UCameraModifier::DisableModifier(bool bImmediate)
{
	if (bImmediate)
	{
		bDisabled = true;
		bPendingDisable = false;
		Alpha = 0.0f;
		return;
	}

	bPendingDisable = true;
}

bool UCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	return false;
}

void UCameraModifier::UpdateAlpha(float DeltaTime)
{
	if (bDisabled)
	{
		return;
	}

	if (bPendingDisable)
	{
		if (AlphaOutTime <= 0.0f)
		{
			Alpha = 0.0f;
			bPendingDisable = false;
			bDisabled = true;
			return;
		}

		Alpha -= DeltaTime / AlphaOutTime;
		if (Alpha <= 0.0f)
		{
			Alpha = 0.0f;
			bPendingDisable = false;
			bDisabled = true;
		}
		return;
	}

	if (AlphaInTime <= 0.0f)
	{
		Alpha = 1.0f;
		return;
	}

	Alpha += DeltaTime / AlphaInTime;
	if (Alpha > 1.0f)
	{
		Alpha = 1.0f;
	}
}
