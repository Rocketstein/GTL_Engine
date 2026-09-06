#include "PlayerCameraManager.h"

#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShake.h"
#include "Camera/CameraShakePattern.h"
#include "Camera/LuaCameraModifier.h"
#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"
#include "Engine/Asset/AssetCurveUtils.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Core/Log.h"
#include "Engine/GameFramework/PawnActor.h"
#include "Engine/GameFramework/PlayerController.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

// Function : Build camera shake instance from serialized asset description
// input : Outer, Desc
// Outer : UObject owner for created shake and pattern objects
// Desc : asset description that defines duration, intensity, and pattern data
static UCameraShakeBase* BuildCameraShakeFromAssetDesc(UObject* Outer, const FCameraShakeModifierAssetDesc& Desc)
{
	UCameraShakeBase* Shake = UObjectManager::Get().CreateObject<UCameraShakeBase>(Outer);
	if (!Shake)
	{
		return nullptr;
	}

	UCameraShakePattern* RootPattern = nullptr;
	if (Desc.bUseCurves)
	{
		UCurveCameraShakePattern* CurvePattern = UObjectManager::Get().CreateObject<UCurveCameraShakePattern>(Shake);
		if (CurvePattern)
		{
			CurvePattern->SetTransitionCurveX(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationX, Desc.LocationAmplitude.X));
			CurvePattern->SetTransitionCurveY(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationY, Desc.LocationAmplitude.Y));
			CurvePattern->SetTransitionCurveZ(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationZ, Desc.LocationAmplitude.Z));
			CurvePattern->SetRotationCurveX(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationX, Desc.RotationAmplitude.Pitch));
			CurvePattern->SetRotationCurveY(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationY, Desc.RotationAmplitude.Yaw));
			CurvePattern->SetRotationCurveZ(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationZ, Desc.RotationAmplitude.Roll));
			RootPattern = CurvePattern;
		}
	}
	else
	{
		USinWaveCameraShakePattern* SinPattern = UObjectManager::Get().CreateObject<USinWaveCameraShakePattern>(Shake);
		if (SinPattern)
		{
			SinPattern->Frequency = Desc.Frequency;
			SinPattern->TransitionAmplitudeX = Desc.LocationAmplitude.X;
			SinPattern->TransitionAmplitudeY = Desc.LocationAmplitude.Y;
			SinPattern->TransitionAmplitudeZ = Desc.LocationAmplitude.Z;
			SinPattern->RotationAmplitudeX = Desc.RotationAmplitude.Pitch;
			SinPattern->RotationAmplitudeY = Desc.RotationAmplitude.Yaw;
			SinPattern->RotationAmplitudeZ = Desc.RotationAmplitude.Roll;
			RootPattern = SinPattern;
		}
	}

	if (!RootPattern)
	{
		UObjectManager::Get().DestroyObject(Shake);
		return nullptr;
	}

	Shake->Duration = Desc.Duration;
	Shake->Intensity = Desc.Intensity;
	Shake->SetRootShakePattern(RootPattern);

	return Shake;
}

void FViewTarget::SetNewTarget(AActor* NewTarget)
{
	if (!NewTarget)
	{
		return;
	}

	Target = NewTarget;
}

APawnActor* FViewTarget::GetTargetPawn() const
{
	if (!Target)
	{
		return nullptr;
	}

	return Cast<APawnActor>(Target);
}

bool FViewTarget::Equal(const FViewTarget& OtherTarget) const
{
	return Target != nullptr && Target == OtherTarget.Target;
}

void FViewTarget::CheckViewTarget(APlayerController* OwningController)
{
	if (!Target && OwningController)
	{
		Target = OwningController->GetPawn();
	}
}

void APlayerCameraManager::BeginPlay()
{
	AActor::BeginPlay();
}

// Function : Destroy owned camera modifiers when manager leaves play
// input : none
// ModifierList : manager-owned modifier instances to release
void APlayerCameraManager::EndPlay()
{
	for (UCameraModifier* Modifier : ModifierList)
	{
		if (Modifier)
		{
			UObjectManager::Get().DestroyObject(Modifier);
		}
	}

	ModifierList.clear();
	CameraShakeModifier = nullptr;
	AActor::EndPlay();
}

void APlayerCameraManager::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);
	UpdateCamera(DeltaTime);
}

// Function : Bind camera manager to controller and initialize view target from pawn
// input : InPlayerController
// InPlayerController : owning controller that supplies the current pawn
void APlayerCameraManager::SetOwner(APlayerController* InPlayerController)
{
	Owner = InPlayerController;
	if (Owner && Owner->GetPawn())
	{
		ViewTarget.SetNewTarget(Owner->GetPawn());
	}
}

void APlayerCameraManager::AddCameraModifier(UCameraModifier* InModifier)
{
	if (!InModifier)
	{
		return;
	}

	for (UCameraModifier* ExistingModifier : ModifierList)
	{
		if (ExistingModifier == InModifier)
		{
			InModifier->EnableModifier();
			return;
		}
	}

	InModifier->AddedToCamera(this);
	InModifier->EnableModifier();
	ModifierList.push_back(InModifier);
	SortModifiersByPriority();
}

void APlayerCameraManager::PlayCameraModifier(const FString& ScriptPath, const TMap<FString, float>& Params)
{
	ULuaCameraModifier* Modifier = UObjectManager::Get().CreateObject<ULuaCameraModifier>(this);
	if (!Modifier)
	{
		return;
	}

	if (!Modifier->Initialize(ScriptPath, Params))
	{
		UObjectManager::Get().DestroyObject(Modifier);
		return;
	}

	AddCameraModifier(Modifier);
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	RemoveFinishedModifiers();

	for (UCameraModifier* CameraModifier : ModifierList)
	{
		if (!CameraModifier || CameraModifier->IsDisabled())
		{
			continue;
		}

		CameraModifier->UpdateAlpha(DeltaTime);
		if (CameraModifier->IsDisabled())
		{
			continue;
		}

		const bool bStopProcessing = CameraModifier->ModifyCamera(DeltaTime, InOutPOV);
		if (bStopProcessing)
		{
			break;
		}
	}

	RemoveFinishedModifiers();
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	// Reset Camera Snapshot.
	// UpdateCamera owns the final runtime POV for this frame.
	bHasValidCameraCachePOV = false;
	LastFrameCameraCache = CameraCache;

	ViewTarget.CheckViewTarget(Owner);

	FMinimalViewInfo NewPOV;

	if (bIsBlendingViewTarget)
	{
		FMinimalViewInfo FromPOV = BlendParams.bLockOutgoing
			? OutgoingViewTarget.POV
			: CalcViewTargetPOV(OutgoingViewTarget, DeltaTime);

		FMinimalViewInfo ToPOV = CalcViewTargetPOV(PendingViewTarget, DeltaTime);

		const float RawAlpha = BlendTotalTime > 0.0f
			? 1.0f - (BlendTimeToGo / BlendTotalTime)
			: 1.0f;

		const float Alpha = ApplyViewTargetBlendFunction(RawAlpha);
		NewPOV = BlendViewInfo(FromPOV, ToPOV, Alpha);

		BlendTimeToGo -= DeltaTime;

		if (BlendTimeToGo <= 0.0f)
		{
			ViewTarget = PendingViewTarget;
			ViewTarget.POV = ToPOV;
			NewPOV = ToPOV;

			PendingViewTarget = {};
			OutgoingViewTarget = {};
			BlendTimeToGo = 0.0f;
			BlendTotalTime = 0.0f;
			bIsBlendingViewTarget = false;
		}
	}
	else
	{
		NewPOV = CalcViewTargetPOV(ViewTarget, DeltaTime);
	}

	ApplyCameraModifiers(DeltaTime, NewPOV);

	if (bEnableFading && FadeTime > 0.0f)
	{
		FadeTimeRemaining = std::max(0.0f, FadeTimeRemaining - DeltaTime);
		const float Elapsed = FadeTime - FadeTimeRemaining;
		FadeAmount = FadeAlpha.X + (FadeAlpha.Y - FadeAlpha.X) * (Elapsed / FadeTime);
	}

	NewPOV.PostProcessSettings.FadeColor = FadeColor;
	NewPOV.PostProcessSettings.FadeAmount = FadeAmount;
	NewPOV.bConstrainAspectRatio = bEnableLetterBoxing;
	if (bEnableLetterBoxing)
	{
		NewPOV.LetterBoxingAspectW = LetterBoxingAspectW;
		NewPOV.LetterBoxingAspectH = LetterBoxingAspectH;
	}

	ViewTarget.POV = NewPOV;
	CameraCache.TimeStamp += DeltaTime;
	CameraCache.POV = NewPOV;
	bHasValidCameraCachePOV = true;
}

void APlayerCameraManager::SortModifiersByPriority()
{
	std::sort(ModifierList.begin(), ModifierList.end(),
		[](const UCameraModifier* A, const UCameraModifier* B)
		{
			if (!A)
			{
				return false;
			}
			if (!B)
			{
				return true;
			}
			return A->Priority > B->Priority;
		});
}

void APlayerCameraManager::RemoveFinishedModifiers()
{
	ModifierList.erase(
		std::remove_if(ModifierList.begin(), ModifierList.end(),
			[this](UCameraModifier* Modifier)
			{
				if (!Modifier)
				{
					return true;
				}

				if (Modifier->IsFinished())
				{
					if (Modifier == CameraShakeModifier)
					{
						CameraShakeModifier = nullptr;
					}
					UObjectManager::Get().DestroyObject(Modifier);
					return true;
				}

				return false;
			}),
		ModifierList.end());
}

void APlayerCameraManager::StartCameraShake()
{
	if (CameraShakeModifier)
	{
		CameraShakeModifier->EnableModifier();
	}
}

void APlayerCameraManager::EndCameraShake()
{
	if (CameraShakeModifier)
	{
		CameraShakeModifier->DisableModifier();
	}
}

void APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color)
{
	// Initialize fade state immediately from call parameters.
	FadeColor = Color;
	FadeAlpha = FVector2(FromAlpha, ToAlpha);
	FadeTime = std::max(0.0f, Duration);
	FadeTimeRemaining = FadeTime;
	FadeAmount = FadeTime > 0.0f ? FromAlpha : ToAlpha;
	bEnableFading = true;

	ViewTarget.POV.PostProcessSettings.FadeColor = FadeColor;
	ViewTarget.POV.PostProcessSettings.FadeAmount = FadeAmount;
	if (bHasValidCameraCachePOV)
	{
		CameraCache.POV.PostProcessSettings.FadeColor = FadeColor;
		CameraCache.POV.PostProcessSettings.FadeAmount = FadeAmount;
	}
}

void APlayerCameraManager::EndCameraFade()
{
	// Remove fade immediately without interpolation.
	bEnableFading = false;
	FadeAmount = 0.0f;
	FadeTimeRemaining = 0.0f;
	ViewTarget.POV.PostProcessSettings.FadeColor = FadeColor;
	ViewTarget.POV.PostProcessSettings.FadeAmount = FadeAmount;
	if (bHasValidCameraCachePOV)
	{
		CameraCache.POV.PostProcessSettings.FadeColor = FadeColor;
		CameraCache.POV.PostProcessSettings.FadeAmount = FadeAmount;
	}
}

void APlayerCameraManager::LoadCameraModifierStackAsset(const std::filesystem::path& AssetPath)
{
	FString Error;
	UAssetData* LoadedAsset = FAssetFileSerializer::LoadAssetFromFile(AssetPath, &Error);
	if (!LoadedAsset)
	{
		UE_LOG_CATEGORY(PlayerCameraManager, Error, "Failed to load camera modifier asset: %s", Error.c_str());
		return;
	}

	UCameraModifierStackAssetData* StackAsset = Cast<UCameraModifierStackAssetData>(LoadedAsset);
	if (!StackAsset)
	{
		UE_LOG_CATEGORY(PlayerCameraManager, Error, "Asset type mismatch: %s", AssetPath.string().c_str());
		UObjectManager::Get().DestroyObject(LoadedAsset);
		return;
	}

	for (const FCameraShakeModifierAssetDesc& Desc : StackAsset->CameraShakes)
	{
		if (Desc.Common.bStartDisabled)
		{
			continue;
		}

		UCameraModifier_CameraShake* Modifier = EnsureCameraShakeModifier();
		if (!Modifier)
		{
			UE_LOG_CATEGORY(PlayerCameraManager, Error, "Failed to create camera shake modifier from asset entry: %s", Desc.Name.c_str());
			continue;
		}

		UCameraShakeBase* Shake = BuildCameraShakeFromAssetDesc(Modifier, Desc);
		if (!Shake)
		{
			UE_LOG_CATEGORY(PlayerCameraManager, Error, "Failed to create camera shake from asset entry: %s", Desc.Name.c_str());
			continue;
		}

		Modifier->Priority = Desc.Common.Priority > Modifier->Priority ? Desc.Common.Priority : Modifier->Priority;
		Modifier->SetAlphaInTime(Desc.Common.AlphaInTime);
		Modifier->SetAlphaOutTime(Desc.Common.AlphaOutTime);
		Modifier->AddCameraShake(Shake);
		SortModifiersByPriority();
	}

	UObjectManager::Get().DestroyObject(LoadedAsset);
}

void APlayerCameraManager::SetViewTarget(AActor* NewTarget)
{
	if (!NewTarget)
	{
		return;
	}

	ViewTarget.SetNewTarget(NewTarget);
	PendingViewTarget = {};
	OutgoingViewTarget = {};
	BlendTimeToGo = 0.0f;
	BlendTotalTime = 0.0f;
	bIsBlendingViewTarget = false;
}


void APlayerCameraManager::SetViewTargetWithBlend(AActor* NewTarget, const FViewTargetTransitionParams& Params)
{
	if (!NewTarget)
	{
		return;
	}

	if (Params.BlendTime <= 0.0f || !ViewTarget.Target)
	{
		SetViewTarget(NewTarget);
		return;
	}

	BlendParams = Params;
	BlendTotalTime = Params.BlendTime;
	BlendTimeToGo = Params.BlendTime;
	bIsBlendingViewTarget = true;

	OutgoingViewTarget = ViewTarget;
	if (BlendParams.bLockOutgoing)
	{
		OutgoingViewTarget.POV = CameraCache.POV;
	}

	PendingViewTarget.SetNewTarget(NewTarget);
}


UCameraComponent* APlayerCameraManager::FindCameraComponent(AActor* Target)
{
	if (!Target)
	{
		return nullptr;
	}

	return Target->GetComponentByClass<UCameraComponent>();
}

FMinimalViewInfo APlayerCameraManager::BuildFallbackCameraView(AActor* Target) const
{
	FMinimalViewInfo ViewInfo;
	if (Target)
	{
		ViewInfo.Location = Target->GetActorLocation();
		ViewInfo.Rotation = Target->GetActorRotation();
	}
	else
	{
		ViewInfo.Location = FVector::ZeroVector;
		ViewInfo.Rotation = FRotator::ZeroRotator;
	}

	return ViewInfo;
}

UCameraModifier_CameraShake* APlayerCameraManager::EnsureCameraShakeModifier()
{
	if (CameraShakeModifier)
	{
		return CameraShakeModifier;
	}

	CameraShakeModifier = UObjectManager::Get().CreateObject<UCameraModifier_CameraShake>(this);
	if (!CameraShakeModifier)
	{
		return nullptr;
	}

	AddCameraModifier(CameraShakeModifier);
	return CameraShakeModifier;
}

void APlayerCameraManager::StartLetterBoxing(float LBAspectW, float LBAspectH) {
	if (LBAspectW <= 0.0f || LBAspectH <= 0.0f)
	{
		return;
	}

	bEnableLetterBoxing = true;
	LetterBoxingAspectW = LBAspectW;
	LetterBoxingAspectH = LBAspectH;

	ViewTarget.POV.bConstrainAspectRatio = true;
	ViewTarget.POV.LetterBoxingAspectW = LBAspectW;
	ViewTarget.POV.LetterBoxingAspectH = LBAspectH;

	if (bHasValidCameraCachePOV)
	{
		CameraCache.POV.bConstrainAspectRatio = true;
		CameraCache.POV.LetterBoxingAspectW = LBAspectW;
		CameraCache.POV.LetterBoxingAspectH = LBAspectH;
	}
}

void APlayerCameraManager::EndLetterBoxing() {
	bEnableLetterBoxing = false;
	ViewTarget.POV.bConstrainAspectRatio = false;

	if (bHasValidCameraCachePOV)
	{
		CameraCache.POV.bConstrainAspectRatio = false;
	}
}

FMinimalViewInfo APlayerCameraManager::CalcViewTargetPOV(FViewTarget& InViewTarget, float DeltaTime)
{
	if (InViewTarget.Target)
	{
		InViewTarget.Target->CalcCamera(DeltaTime, InViewTarget.POV);
		return InViewTarget.POV;
	}

	InViewTarget.POV = BuildFallbackCameraView(nullptr);
	return InViewTarget.POV;
}


FMinimalViewInfo APlayerCameraManager::BlendViewInfo(const FMinimalViewInfo& FromPOV, const FMinimalViewInfo& ToPOV, float Alpha) const
{
	FMinimalViewInfo Result = ToPOV;

	Result.Location = FromPOV.Location + (ToPOV.Location - FromPOV.Location) * Alpha;
	Result.Rotation.Pitch = FromPOV.Rotation.Pitch + (ToPOV.Rotation.Pitch - FromPOV.Rotation.Pitch) * Alpha;
	Result.Rotation.Yaw = FromPOV.Rotation.Yaw + (ToPOV.Rotation.Yaw - FromPOV.Rotation.Yaw) * Alpha;
	Result.Rotation.Roll = FromPOV.Rotation.Roll + (ToPOV.Rotation.Roll - FromPOV.Rotation.Roll) * Alpha;
	Result.FOV = FromPOV.FOV + (ToPOV.FOV - FromPOV.FOV) * Alpha;
	Result.OrthoWidth = FromPOV.OrthoWidth + (ToPOV.OrthoWidth - FromPOV.OrthoWidth) * Alpha;
	Result.PostProcessBlendWeight =
		FromPOV.PostProcessBlendWeight + (ToPOV.PostProcessBlendWeight - FromPOV.PostProcessBlendWeight) * Alpha;

	return Result;
}

float APlayerCameraManager::ApplyViewTargetBlendFunction(float Alpha) const
{
	Alpha = (std::clamp)(Alpha, 0.0f, 1.0f);

	switch (BlendParams.BlendFunction)
	{
	case EViewTargetBlendFunction::EaseIn:
		return std::pow(Alpha, BlendParams.BlendExp);
	case EViewTargetBlendFunction::EaseOut:
		return 1.0f - std::pow(1.0f - Alpha, BlendParams.BlendExp);
	case EViewTargetBlendFunction::EaseInOut:
		return Alpha < 0.5f
			? 0.5f * std::pow(Alpha * 2.0f, BlendParams.BlendExp)
			: 1.0f - 0.5f * std::pow((1.0f - Alpha) * 2.0f, BlendParams.BlendExp);
	case EViewTargetBlendFunction::Linear:
	default:
		return Alpha;
	}
}
