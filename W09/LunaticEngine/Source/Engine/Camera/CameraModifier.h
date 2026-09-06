#pragma once
#include "Camera/MinimalViewInfo.h"
#include "Core/CoreTypes.h"
#include "Object/Object.h"

class APlayerCameraManager;

class UCameraModifier : public UObject
{
public:
	DECLARE_CLASS(UCameraModifier, UObject)
	UCameraModifier();

	virtual void AddedToCamera(APlayerCameraManager* Camera);

	// Function : Modify final camera POV
	// input : DeltaTime, InOutPOV
	// DeltaTime : frame delta time used by modifier state
	// InOutPOV : final camera view data modified in place
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV);

	// Function : Update modifier blend alpha
	// input : DeltaTime
	// DeltaTime : frame delta time used for alpha in/out
	virtual void UpdateAlpha(float DeltaTime);

	virtual bool IsDisabled() const { return bDisabled; }
	virtual bool IsFinished() const;
	virtual void EnableModifier();
	virtual void ToggleModifier();
	virtual void DisableModifier(bool bImmediate = false);

	float GetAlpha() const { return Alpha; }
	bool IsPendingDisable() const { return bPendingDisable != 0; }
	void SetAlphaInTime(float InAlphaInTime) { AlphaInTime = InAlphaInTime > 0.0f ? InAlphaInTime : 0.0f; }
	void SetAlphaOutTime(float InAlphaOutTime) { AlphaOutTime = InAlphaOutTime > 0.0f ? InAlphaOutTime : 0.0f; }
	float GetAlphaInTime() const { return AlphaInTime; }
	float GetAlphaOutTime() const { return AlphaOutTime; }

protected:
	virtual ~UCameraModifier();

public:
	uint8 Priority = 0;
	float TransitionIntensity = 1.0f;
	float RotationIntensity = 1.0f;

protected:
	APlayerCameraManager* CameraOwner = nullptr;

	float AlphaInTime = 0.0f;
	float AlphaOutTime = 0.0f;
	float Alpha = 0.0f;
	uint32 bPendingDisable = false;
	uint32 bDisabled = false;
};
