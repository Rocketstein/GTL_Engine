#pragma once

#include "CameraModifier.h"
#include "Camera/Shakes/CameraShakeTypes.h"

class UCameraShakeBase;
class UClass;

struct FAddCameraShakeParams
{
	float Scale = 1.f;
	ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;
	FRotator UserPlaySpaceRot = FRotator::ZeroRotator;
	TOptional<float> DurationOverride;
};

struct FActiveCameraShakeInfo
{
	UCameraShakeBase* ShakeInstance = nullptr;
};

class UCameraModifier_CameraShake : public UCameraModifier
{
public:
	DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)

	~UCameraModifier_CameraShake() override;

	UCameraShakeBase* AddCameraShake(UClass* ShakeClass, const FAddCameraShakeParams& Params = FAddCameraShakeParams());
	UCameraShakeBase* AddCameraShake(const char* ShakeClassName, const FAddCameraShakeParams& Params = FAddCameraShakeParams());
	UCameraShakeBase* AddCameraShake(UCameraShakeBase* ShakeInstance, const FAddCameraShakeParams& Params = FAddCameraShakeParams());
	UCameraShakeBase* AddCameraShakeFromAsset(const FString& Path, const FAddCameraShakeParams& Params = FAddCameraShakeParams());

	void RemoveCameraShake(UCameraShakeBase* ShakeInstance, bool bImmediately = true);
	void RemoveAllCameraShakesOfClass(UClass* ShakeClass, bool bImmediately = true);
	void RemoveAllCameraShakesOfClass(const char* ShakeClassName, bool bImmediately = true);
	void RemoveAllCameraShakes(bool bImmediately = true);

	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

private:
	UCameraShakeBase* FindActiveShakeByInstance(UCameraShakeBase* ShakeInstance) const;
	UCameraShakeBase* FindActiveShakeByClass(UClass* ShakeClass) const;
	UCameraShakeBase* FindActiveShakeByClassAndAsset(UClass* ShakeClass, const FString& SourceAssetPath) const;
	void DestroyActiveShakeAt(size_t Index);
	FCameraShakeBaseStartParams MakeStartParams(const FAddCameraShakeParams& Params) const;

private:
	TArray<FActiveCameraShakeInfo> ActiveShakes;
};
