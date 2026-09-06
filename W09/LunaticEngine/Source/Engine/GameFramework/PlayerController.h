#pragma once

#include "GameFramework/AActor.h"
#include "Input/EnhancedInputManager.h"
#include "Engine/Camera/PlayerCameraManager.h"

class APawnActor;
class APlayerCameraManager;
struct FInputMappingContext;
struct FInputAction;

// 플레이어 입력을 받아 Pawn을 조종하는 액터.
// UE의 APlayerController 대응 — 자체 FEnhancedInputManager로 매핑 컨텍스트/액션 바인딩을 관리한다.
class APlayerController : public AActor
{
public:
	DECLARE_CLASS(APlayerController, AActor)

	APlayerController();

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	void Possess(APawnActor* InPawn);
	void UnPossess();
	APawnActor* GetPawn() const { return PossessedPawn; }

	// Function : Change current camera view target immediately
	// input : NewViewTarget
	// NewViewTarget : actor that provides camera POV through CalcCamera
	void SetViewTarget(AActor* NewViewTarget);

	// Function : Change current camera view target using PlayerCameraManager transition blend
	// input : NewViewTarget, BlendTime, BlendFunction, BlendExp, bLockOutgoing
	// NewViewTarget : actor that provides target camera POV through CalcCamera
	// BlendTime : transition duration in seconds
	// BlendFunction : curve used to remap transition alpha
	// BlendExp : exponent used by ease blend functions
	// bLockOutgoing : if true, outgoing POV is frozen at transition start
	void SetViewTargetWithBlend(
		AActor* NewViewTarget,
		float BlendTime = 0.0f,
		EViewTargetBlendFunction BlendFunction = EViewTargetBlendFunction::Linear,
		float BlendExp = 2.0f,
		bool bLockOutgoing = false);

	// 서브클래스가 BeginPlay 시점에 매핑 컨텍스트/액션 바인딩을 등록하기 위한 훅 (UE의 SetupPlayerInputComponent 대응).
	virtual void SetupInputComponent() {}

	// Mapping/binding helpers are called by subclasses from SetupInputComponent.
	// Function : Add mapping context to manager and sort by priority
	// input : Context, Priority
	// Context : mapping context to add
	// Priority : if there are multiple mapping context, context with higher priority will be processed first
	void AddMappingContext(FInputMappingContext* Context, int32 Priority = 0)
	{
		EnhancedInput.AddMappingContext(Context, Priority);
	}

	void RemoveMappingContext(FInputMappingContext* Context)
	{
		EnhancedInput.RemoveMappingContext(Context);
	}

	void BindAction(FInputAction* Action, ETriggerEvent TriggerEvent, FInputActionCallback Callback)
	{
		EnhancedInput.BindAction(Action, TriggerEvent, std::move(Callback));
	}

	// Function : Assign PlayerCameraManager to this controller
	// input : InCameraManager
	// InCameraManager : camera manager responsible for final player camera POV
	void AcquirePlayerCameraManager(APlayerCameraManager* InCameraManager);

	// Function : Request camera modifier playback on the owned PlayerCameraManager
	// input : ScriptPath, Params
	// ScriptPath : Lua modifier script path under Scripts
	// Params : numeric parameters passed to Lua modifier Begin(params)
	void PlayCameraModifier(const FString& ScriptPath, const TMap<FString, float>& Params = {});

protected:
	APawnActor* PossessedPawn = nullptr;
	APlayerCameraManager* PlayerCameraManager = nullptr;
	FEnhancedInputManager EnhancedInput;
};
