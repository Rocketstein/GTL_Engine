#include "PlayerController.h"

#include "Component/CameraComponent.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/PawnActor.h"
#include "GameFramework/World.h"
#include "Input/InputManager.h"
#include "Viewport/GameViewportClient.h"

IMPLEMENT_CLASS(APlayerController, AActor)

APlayerController::APlayerController()
{
	bNeedsTick = true;
	bTickInEditor = false;
}

void APlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetupInputComponent();
}

void APlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	EnhancedInput.ProcessInput(&FInputManager::Get(), DeltaTime, /*bIgnoreGui=*/false);
}

void APlayerController::Possess(APawnActor* InPawn)
{
	if (PossessedPawn == InPawn) return;
	UnPossess();
	PossessedPawn = InPawn;

	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetOwner(this);
	}

	if (PossessedPawn)
	{
		UCameraComponent* PawnCamera = nullptr;
		for (UActorComponent* Comp : PossessedPawn->GetComponents())
		{
			if (UCameraComponent* Cam = Cast<UCameraComponent>(Comp))
			{
				PawnCamera = Cam;
				break;
			}
		}

		if (PawnCamera)
		{
			if (UWorld* World = GetWorld())
			{
				World->SetActiveCamera(PawnCamera);
			}

			if (GEngine)
			{
				if (UGameViewportClient* GameVC = GEngine->GetGameViewportClient())
				{
					GameVC->Possess(PawnCamera);
				}
			}
		}
	}
}

void APlayerController::UnPossess()
{
	PossessedPawn = nullptr;
}

void APlayerController::SetViewTarget(AActor* NewViewTarget)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTarget(NewViewTarget);
	}
}

void APlayerController::SetViewTargetWithBlend(
	AActor* NewViewTarget,
	float BlendTime,
	EViewTargetBlendFunction BlendFunction,
	float BlendExp,
	bool bLockOutgoing)
{
	if (!PlayerCameraManager)
	{
		return;
	}

	FViewTargetTransitionParams Params;
	Params.BlendTime = BlendTime;
	Params.BlendFunction = BlendFunction;
	Params.BlendExp = BlendExp;
	Params.bLockOutgoing = bLockOutgoing;

	PlayerCameraManager->SetViewTargetWithBlend(NewViewTarget, Params);
}

void APlayerController::AcquirePlayerCameraManager(APlayerCameraManager* InCameraManager)
{
	PlayerCameraManager = InCameraManager;
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetOwner(this);
	}
}

void APlayerController::PlayCameraModifier(const FString& ScriptPath, const TMap<FString, float>& Params)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->PlayCameraModifier(ScriptPath, Params);
	}
}
