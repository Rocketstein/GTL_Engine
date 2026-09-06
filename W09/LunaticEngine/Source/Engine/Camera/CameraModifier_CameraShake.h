#pragma once
#include "CameraModifier.h"
#include "CameraShake.h"

class UCameraModifier_CameraShake : public UCameraModifier
{
public:
	DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)

	void AddedToCamera(APlayerCameraManager* Camera) override;

	// Function : Apply active camera shake instances to final POV
	// input : DeltaTime, InOutPOV
	// DeltaTime : frame delta time used to advance shake instances
	// InOutPOV : final POV modified by accumulated shake offsets
	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	bool IsFinished() const override;

	// Function : Replace active shake list with one shake instance
	// input : InShake
	// InShake : camera shake instance owned by this modifier
	void SetCameraShake(UCameraShakeBase* InShake);

	// Function : Add camera shake instance to active shake list
	// input : InShake
	// InShake : camera shake instance evaluated every frame
	void AddCameraShake(UCameraShakeBase* InShake);
	UCameraShakeBase* GetCameraShake() const;
	const TArray<UCameraShakeBase*>& GetActiveShakes() const { return ActiveShakes; }

	// Function : Remove finished shake instances from active list
	// input : none
	// ActiveShakes : list of shake instances currently affecting POV
	void RemoveFinishedShakes();

	// Function : Destroy all active shake instances
	// input : none
	// ActiveShakes : list cleared when modifier is destroyed or reset
	void ClearCameraShakes();

protected:
	~UCameraModifier_CameraShake() override;

private:
	TArray<UCameraShakeBase*> ActiveShakes;
};
