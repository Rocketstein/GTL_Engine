#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "Component/ActorComponent.h"
#include "Component/Camera/CameraComponent.h"

void APlayerController::BeginPlay()
{
	Super::BeginPlay();
	// E.2/3: PC 가 PlayerCameraManager 의 owner — UE 패턴.
	// PC 가 GameMode->StartMatch 에서 spawn 되는 시점엔 다른 액터들이 이미 BeginPlay 완료.
	// 그 사이에 BeginPlay 한 카메라 컴포넌트들은 PC 가 없어 등록 못 했으므로 여기서 catch up.
	if (UWorld* World = GetWorld())
	{
		PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		if (PlayerCameraManager)
		{
			// 한 액터가 여러 카메라 컴포넌트(예: CarPawn 의 First/Third Person)를 가질 수
			// 있으므로 GetComponents 전체를 순회. GetComponentByClass 는 첫 번째만 반환하므로 부족.
			for (AActor* Actor : World->GetActors())
			{
				if (!Actor) continue;
				for (UActorComponent* Comp : Actor->GetComponents())
				{
					if (UCameraComponent* Cam = Cast<UCameraComponent>(Comp))
					{
						PlayerCameraManager->RegisterCamera(Cam);
					}
				}
			}
			PlayerCameraManager->AutoPossessDefaultCamera();
		}
	}
}

void APlayerController::SetViewTargetWithBlend(
	AActor* NewViewTarget,
	float BlendTime,
	EViewTargetBlendFunction BlendFunc,
	float BlendExp,
	bool bLockOutgoing)
{
	APlayerCameraManager* CM = GetPlayerCameraManager();
	if (!CM) return;

	FViewTargetTransitionParams Params;
	Params.BlendTime = BlendTime;
	Params.BlendFunction = BlendFunc;
	Params.BlendExp = BlendExp;
	Params.bLockOutgoing = bLockOutgoing;

	CM->SetViewTarget(NewViewTarget, Params);
}

void APlayerController::Possess(APawn* Pawn)
{
	APawn* OldPawn = GetPossessedPawn();
	AController::Possess(Pawn);
	NotifyPossessedPawnChanged(OldPawn, GetPossessedPawn());
}

void APlayerController::UnPossess()
{
	APawn* OldPawn = GetPossessedPawn();
	AController::UnPossess();
	NotifyPossessedPawnChanged(OldPawn, GetPossessedPawn());
}

void APlayerController::NotifyPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (OldPawn == NewPawn)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->NotifyPlayerPawnChanged(this, OldPawn, NewPawn);
	}
}

void APlayerController::ProcessPlayerInput(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	// 입력이 disable 된 동안엔 Pawn 으로 전달하지 않는다 (사망 등). World 는 계속 Tick.
	if (!bInputEnabled)
	{
		return;
	}

	APawn* Pawn = GetPawn();
	if (!Pawn)
	{
		return;
	}

	Pawn->ProcessPlayerInput(Snapshot, DeltaTime);
}
