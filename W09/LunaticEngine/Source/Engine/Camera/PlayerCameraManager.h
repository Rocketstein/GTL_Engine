#pragma once
#include "GameFramework/AActor.h"
#include "CameraModifier.h"
#include "Core/EngineTypes.h"
#include "Core/CollisionEventTypes.h"

#include <filesystem>

class USciptComponent;
class APawnActor;
class APlayerController;
class UCameraComponent;
class UCameraModifier_CameraShake;

struct FViewTarget
{
public:
	// Function : Set actor used as current camera view target
	// input : InTarget
	// InTarget : actor that provides camera POV through CalcCamera
	void SetNewTarget(AActor* InTarget);
	APawnActor* GetTargetPawn() const;
	bool Equal(const FViewTarget& OtherTarget) const;

	// Function : Ensure view target follows owning controller pawn
	// input : OwningController
	// OwningController : controller that owns the current player pawn
	void CheckViewTarget(APlayerController* OwningController);

public:
	AActor* Target = nullptr;
	FMinimalViewInfo POV;
};

enum class EViewTargetBlendFunction
{
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut,
};

struct FViewTargetTransitionParams
{
	float BlendTime = 0.f;
	EViewTargetBlendFunction BlendFunction = EViewTargetBlendFunction::Linear;
	float BlendExp = 2.0f;
	bool bLockOutgoing = false;
};

struct FCameraCacheEntry
{
	float TimeStamp = 0.f;
	FMinimalViewInfo POV;
};


class APlayerCameraManager : public AActor
{
public:
	DECLARE_CLASS(APlayerCameraManager, AActor)

	void BeginPlay() override;
	void EndPlay() override;

	// PlayerCameraManager keeps the modifier list alive and evaluates it every tick.
	void Tick(float DeltaTime) override;

	// Function : Bind camera manager to player controller
	// input : InPlayerController
	// InPlayerController : controller that owns the pawn used as default view target
	void SetOwner(APlayerController* InPlayerController);

	// Function : Add camera modifier to manager and sort by priority
	// input : InModifier
	// InModifier : modifier instance that will edit final camera POV
	void AddCameraModifier(UCameraModifier* InModifier);

	// Function : Create and play a Lua camera modifier through PlayerCameraManager
	// input : ScriptPath, Params
	// ScriptPath : Lua modifier script path under Scripts
	// Params : numeric parameters passed to Lua modifier Begin(params)
	void PlayCameraModifier(const FString& ScriptPath, const TMap<FString, float>& Params = {});

	// Function : Apply active camera modifiers to the current POV
	// input : DeltaTime, InOutPOV
	// DeltaTime : frame delta time
	// InOutPOV : camera POV modified by active modifiers in priority order
	void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);

	// Function : Update view target POV and camera cache for this frame
	// input : DeltaTime
	// DeltaTime : frame delta time used by CalcCamera and modifiers
	void UpdateCamera(float DeltaTime);

	// Function : Sort active camera modifiers by priority
	// input : none
	// Priority : modifier with higher priority is processed first
	void SortModifiersByPriority();

	// Function : Remove camera modifiers that finished their lifecycle
	// input : none
	// ModifierList : active modifiers owned by PlayerCameraManager
	void RemoveFinishedModifiers();

	// Function : Enable camera shake modifier owned by PlayerCameraManager
	// input : none
	// CameraShakeModifier : modifier that owns active camera shake instances
	void StartCameraShake();

	// Function : Disable camera shake modifier owned by PlayerCameraManager
	// input : none
	// CameraShakeModifier : modifier that owns active camera shake instances
	void EndCameraShake();

	// Function : Start camera fade applied to final POV post process settings
	// input : FromAlpha, ToAlpha, Duration, Color
	// FromAlpha : starting fade opacity
	// ToAlpha : target fade opacity
	// Duration : fade blend time in seconds
	// Color : fade color written to final POV
	void StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color);

	// Function : Stop camera fade immediately
	// input : none
	// FadeAmount : reset to zero when fade ends
	void EndCameraFade();

	// Function : Load camera shake data asset and add shakes to camera shake modifier
	// input : AssetPath
	// AssetPath : camera modifier stack asset path
	void LoadCameraModifierStackAsset(const std::filesystem::path& AssetPath);

	const FMinimalViewInfo& GetCameraCachePOV() const { return CameraCache.POV; }
	bool HasValidCameraCachePOV() const { return bHasValidCameraCachePOV; }

	void StartLetterBoxing(float LBAspectW, float LBAspectH);
	void EndLetterBoxing();

	void SetViewTarget(AActor* NewTarget);
	void SetViewTargetWithBlend(AActor* NewTarget, const FViewTargetTransitionParams& Params);
public:
	FViewTarget ViewTarget;
	FName CameraStyle;

	bool bEnableFading = false;
	FLinearColor FadeColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black
	float FadeAmount = 0.0f;
	FVector2 FadeAlpha; // X : start opacity, Y : target opacity
	float FadeTime = 0.0f;
	float FadeTimeRemaining = 0.0f;
	bool bEnableLetterBoxing = false;
	float LetterBoxingAspectW = 16.0f;
	float LetterBoxingAspectH = 9.0f;

	FCameraCacheEntry CameraCache;
	FCameraCacheEntry LastFrameCameraCache;

	FViewTarget PendingViewTarget;
	FViewTarget OutgoingViewTarget;

	FViewTargetTransitionParams BlendParams;
	float BlendTimeToGo = 0.0f;
	float BlendTotalTime = 0.0f;
	bool bIsBlendingViewTarget = false;

private:
	// Function : Find camera component on target actor
	// input : Target
	// Target : actor searched for UCameraComponent
	UCameraComponent* FindCameraComponent(AActor* Target);

	// Function : Build default POV when no view target can provide one
	// input : Target
	// Target : optional actor used for fallback location and rotation
	FMinimalViewInfo BuildFallbackCameraView(AActor* Target) const;

	// Function : Get or create the camera shake modifier owned by this manager
	// input : none
	// CameraShakeModifier : single modifier that owns active camera shake instances
	UCameraModifier_CameraShake* EnsureCameraShakeModifier();


	FMinimalViewInfo CalcViewTargetPOV(FViewTarget& InViewTarget, float DeltaTime);
	FMinimalViewInfo BlendViewInfo(const FMinimalViewInfo& FromPOV, const FMinimalViewInfo& ToPOV, float Alpha) const;
	float ApplyViewTargetBlendFunction(float Alpha) const;


private:
	APlayerController* Owner = nullptr;
	TArray<UCameraModifier*> ModifierList;
	UCameraModifier_CameraShake* CameraShakeModifier = nullptr;
	bool bHasValidCameraCachePOV = false;
};
