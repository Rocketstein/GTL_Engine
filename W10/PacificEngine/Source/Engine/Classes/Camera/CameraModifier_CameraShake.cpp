#include "CameraModifier_CameraShake.h"

#include "Camera/Shakes/CameraShakeAssetManager.h"
#include "Camera/Shakes/CameraShakeBase.h"
#include "CameraManager.h"
#include "Object/ObjectFactory.h"

#include <cstddef>

IMPLEMENT_CLASS(UCameraModifier_CameraShake, UCameraModifier)

UCameraModifier_CameraShake::~UCameraModifier_CameraShake()
{
	RemoveAllCameraShakes(true);
}

UCameraShakeBase* UCameraModifier_CameraShake::AddCameraShake(UClass* ShakeClass, const FAddCameraShakeParams& Params)
{
	if (ShakeClass == nullptr)
	{
		return nullptr;
	}

	return AddCameraShake(ShakeClass->GetName(), Params);
}

UCameraShakeBase* UCameraModifier_CameraShake::AddCameraShake(const char* ShakeClassName, const FAddCameraShakeParams& Params)
{
	if (ShakeClassName == nullptr)
	{
		return nullptr;
	}

	UObject* NewObject = FObjectFactory::Get().Create(ShakeClassName, this);
	if (NewObject == nullptr)
	{
		return nullptr;
	}

	UCameraShakeBase* ShakeInstance = Cast<UCameraShakeBase>(NewObject);
	if (ShakeInstance == nullptr)
	{
		UObjectManager::Get().DestroyObject(NewObject);
		return nullptr;
	}

	UCameraShakeBase* Result = AddCameraShake(ShakeInstance, Params);
	if (Result != ShakeInstance)
	{
		UObjectManager::Get().DestroyObject(ShakeInstance);
	}

	return Result;
}

UCameraShakeBase* UCameraModifier_CameraShake::AddCameraShake(UCameraShakeBase* ShakeInstance, const FAddCameraShakeParams& Params)
{
	if (ShakeInstance == nullptr || ShakeInstance->GetRootShakePattern() == nullptr)
	{
		return nullptr;
	}

	UCameraShakeBase* ExistingShake = FindActiveShakeByInstance(ShakeInstance);
	if (ExistingShake == nullptr && ShakeInstance->bSingleInstance)
	{
		const FString& SourceAssetPath = ShakeInstance->GetSourceAssetPath();
		ExistingShake = SourceAssetPath.empty()
			? FindActiveShakeByClass(ShakeInstance->GetClass())
			: FindActiveShakeByClassAndAsset(ShakeInstance->GetClass(), SourceAssetPath);
	}

	if (ExistingShake)
	{
		ExistingShake->StartShake(MakeStartParams(Params));
		return ExistingShake;
	}

	ShakeInstance->SetOuter(this);
	ShakeInstance->StartShake(MakeStartParams(Params));

	FActiveCameraShakeInfo ActiveInfo;
	ActiveInfo.ShakeInstance = ShakeInstance;
	ActiveShakes.push_back(ActiveInfo);

	return ShakeInstance;
}

UCameraShakeBase* UCameraModifier_CameraShake::AddCameraShakeFromAsset(const FString& Path, const FAddCameraShakeParams& Params)
{
	UCameraShakeBase* ShakeInstance = FCameraShakeAssetManager::Get().CreateShakeInstance(Path, this);
	if (ShakeInstance == nullptr)
	{
		return nullptr;
	}

	UCameraShakeBase* Result = AddCameraShake(ShakeInstance, Params);
	if (Result != ShakeInstance)
	{
		UObjectManager::Get().DestroyObject(ShakeInstance);
	}

	return Result;
}

void UCameraModifier_CameraShake::RemoveCameraShake(UCameraShakeBase* ShakeInstance, bool bImmediately)
{
	if (ShakeInstance == nullptr)
	{
		return;
	}

	for (size_t Index = 0; Index < ActiveShakes.size(); ++Index)
	{
		if (ActiveShakes[Index].ShakeInstance == ShakeInstance)
		{
			if (bImmediately)
			{
				DestroyActiveShakeAt(Index);
			}
			else
			{
				ShakeInstance->StopShake(false);
			}
			return;
		}
	}
}

void UCameraModifier_CameraShake::RemoveAllCameraShakesOfClass(UClass* ShakeClass, bool bImmediately)
{
	if (ShakeClass == nullptr)
	{
		return;
	}

	for (size_t Index = ActiveShakes.size(); Index > 0; --Index)
	{
		UCameraShakeBase* ShakeInstance = ActiveShakes[Index - 1].ShakeInstance;
		if (ShakeInstance && ShakeInstance->GetClass()->IsA(ShakeClass))
		{
			if (bImmediately)
			{
				DestroyActiveShakeAt(Index - 1);
			}
			else
			{
				ShakeInstance->StopShake(false);
			}
		}
	}
}

void UCameraModifier_CameraShake::RemoveAllCameraShakesOfClass(const char* ShakeClassName, bool bImmediately)
{
	UClass* ShakeClass = UClass::FindClassByName(ShakeClassName ? FString(ShakeClassName) : FString());
	RemoveAllCameraShakesOfClass(ShakeClass, bImmediately);
}

void UCameraModifier_CameraShake::RemoveAllCameraShakes(bool bImmediately)
{
	for (size_t Index = ActiveShakes.size(); Index > 0; --Index)
	{
		UCameraShakeBase* ShakeInstance = ActiveShakes[Index - 1].ShakeInstance;
		if (ShakeInstance == nullptr)
		{
			ActiveShakes.erase(ActiveShakes.begin() + static_cast<ptrdiff_t>(Index - 1));
			continue;
		}

		if (bImmediately)
		{
			DestroyActiveShakeAt(Index - 1);
		}
		else
		{
			ShakeInstance->StopShake(false);
		}
	}
}

bool UCameraModifier_CameraShake::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	UpdateAlpha(DeltaTime);

	for (size_t Index = 0; Index < ActiveShakes.size();)
	{
		UCameraShakeBase* ShakeInstance = ActiveShakes[Index].ShakeInstance;
		if (ShakeInstance == nullptr)
		{
			ActiveShakes.erase(ActiveShakes.begin() + static_cast<ptrdiff_t>(Index));
			continue;
		}

		ShakeInstance->UpdateAndApplyCameraShake(DeltaTime, Alpha, InOutPOV);
		if (ShakeInstance->IsFinished())
		{
			DestroyActiveShakeAt(Index);
			continue;
		}

		++Index;
	}

	if (bPendingDisable && Alpha <= 0.f)
	{
		DisableModifier(true);
	}

	return false;
}

UCameraShakeBase* UCameraModifier_CameraShake::FindActiveShakeByInstance(UCameraShakeBase* ShakeInstance) const
{
	for (const FActiveCameraShakeInfo& ActiveShake : ActiveShakes)
	{
		if (ActiveShake.ShakeInstance == ShakeInstance)
		{
			return ActiveShake.ShakeInstance;
		}
	}

	return nullptr;
}

UCameraShakeBase* UCameraModifier_CameraShake::FindActiveShakeByClass(UClass* ShakeClass) const
{
	if (ShakeClass == nullptr)
	{
		return nullptr;
	}

	for (const FActiveCameraShakeInfo& ActiveShake : ActiveShakes)
	{
		UCameraShakeBase* ShakeInstance = ActiveShake.ShakeInstance;
		if (ShakeInstance && ShakeInstance->GetClass() == ShakeClass)
		{
			return ShakeInstance;
		}
	}

	return nullptr;
}

UCameraShakeBase* UCameraModifier_CameraShake::FindActiveShakeByClassAndAsset(UClass* ShakeClass, const FString& SourceAssetPath) const
{
	if (ShakeClass == nullptr || SourceAssetPath.empty())
	{
		return nullptr;
	}

	for (const FActiveCameraShakeInfo& ActiveShake : ActiveShakes)
	{
		UCameraShakeBase* ShakeInstance = ActiveShake.ShakeInstance;
		if (ShakeInstance && ShakeInstance->GetClass() == ShakeClass && ShakeInstance->GetSourceAssetPath() == SourceAssetPath)
		{
			return ShakeInstance;
		}
	}

	return nullptr;
}

void UCameraModifier_CameraShake::DestroyActiveShakeAt(size_t Index)
{
	if (Index >= ActiveShakes.size())
	{
		return;
	}

	UCameraShakeBase* ShakeInstance = ActiveShakes[Index].ShakeInstance;
	if (ShakeInstance)
	{
		ShakeInstance->StopShake(true);
		ShakeInstance->TeardownShake();
		UObjectManager::Get().DestroyObject(ShakeInstance);
	}

	ActiveShakes.erase(ActiveShakes.begin() + static_cast<ptrdiff_t>(Index));
}

FCameraShakeBaseStartParams UCameraModifier_CameraShake::MakeStartParams(const FAddCameraShakeParams& Params) const
{
	FCameraShakeBaseStartParams StartParams;
	StartParams.CameraManager = CameraOwner;
	StartParams.ShakeScale = Params.Scale;
	StartParams.PlaySpace = Params.PlaySpace;
	StartParams.UserPlaySpaceRot = Params.UserPlaySpaceRot;
	StartParams.bOverrideDuration = Params.DurationOverride.has_value();
	StartParams.DurationOverride = Params.DurationOverride.value_or(0.f);
	return StartParams;
}
