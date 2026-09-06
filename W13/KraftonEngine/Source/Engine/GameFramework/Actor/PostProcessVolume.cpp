#include "GameFramework/Actor/PostProcessVolume.h"

#include "GameFramework/World.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "Component/Camera/CameraComponent.h"
#include "Object/Object.h"   // IsAliveObject

UCameraComponent* APostProcessVolume::ResolveActiveCamera() const
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* CM = PC ? PC->GetPlayerCameraManager() : nullptr;
	return CM ? CM->GetActiveCamera() : nullptr;
}

void APostProcessVolume::OnPossessedPawnEntered(APawn* /*Pawn*/)
{
	UCameraComponent* Cam = ResolveActiveCamera();
	if (!Cam) return;

	// 이미 같은 카메라에 적용 중(중복 진입)이면 재백업하지 않는다 — 원본 설정 보존.
	if (AppliedCamera == Cam) return;

	// 다른 카메라에 적용돼 있던 상태(드문 경우)면 먼저 복원해 두고 새로 적용.
	if (AppliedCamera && IsAliveObject(AppliedCamera))
	{
		AppliedCamera->SetPostProcessSettings(SavedSettings);
	}

	SavedSettings = Cam->GetPostProcessSettings();   // 복원용 백업
	AppliedCamera = Cam;
	Cam->SetPostProcessSettings(FocusSettings);
}

void APostProcessVolume::OnPossessedPawnExited(APawn* /*Pawn*/)
{
	if (!AppliedCamera) return;

	// 적용했던 카메라가 그 사이 파괴됐으면 복원 생략(댕글링 방지).
	if (IsAliveObject(AppliedCamera))
	{
		AppliedCamera->SetPostProcessSettings(SavedSettings);
	}
	AppliedCamera = nullptr;
}
