#pragma once

#include "GameFramework/Actor/TriggerVolumeBase.h"
#include "Component/Camera/FPostProcessSettings.h"

#include "Source/Engine/GameFramework/Actor/PostProcessVolume.generated.h"

class UCameraComponent;
class APawn;

// ============================================================
// APostProcessVolume — TriggerBox 안에 들어온 possessed Pawn 의 활성 카메라에
// FocusSettings(DOF 등 PostProcess)를 입히고, 나가면 원래 설정으로 복원한다.
//
// 동작:
//   1) ATriggerVolumeBase 가 TriggerBox(Overlap-only) 셋업 + possessed Pawn 진입/이탈 감지.
//   2) 진입 시   OnPossessedPawnEntered → 활성 카메라(PC->CameraManager->ActiveCamera)의
//      PostProcessSettings 를 백업하고 FocusSettings 로 교체.
//   3) 이탈 시   OnPossessedPawnExited → 백업해 둔 설정으로 복원.
//
// 한계(간단형): 활성 카메라 1개에 직접 적용한다. 볼륨 안에서 카메라가 전환되면 새 카메라엔
//   적용되지 않고(이탈 시 적용했던 카메라만 복원), 여러 볼륨이 겹치면 마지막 진입이 덮어쓴다.
//   카메라 전환/다중 볼륨 blend 가 필요하면 렌더 단계(OverridePostProcess)에서 볼륨을 조회하는
//   방식으로 확장해야 한다.
// ============================================================
UCLASS()
class APostProcessVolume : public ATriggerVolumeBase
{
public:
	GENERATED_BODY()
	APostProcessVolume() = default;
	~APostProcessVolume() override = default;

	void OnPossessedPawnEntered(APawn* Pawn) override;
	void OnPossessedPawnExited(APawn* Pawn) override;

	const FPostProcessSettings& GetFocusSettings() const { return FocusSettings; }
	void SetFocusSettings(const FPostProcessSettings& InSettings) { FocusSettings = InSettings; }

protected:
	// 이 볼륨이 활성 카메라에 입힐 PostProcess(DOF 초점 등). 디자이너가 씬에서 지정/직렬화.
	// Aperture=0 이면 이 구역에서 DOF 를 끄는 용도로도 쓸 수 있다(DOFPass off 스위치).
	UPROPERTY(Edit, Save, Category="PostProcess", DisplayName="Focus Settings", Type=Struct)
	FPostProcessSettings FocusSettings = {};

private:
	// 진입 시 설정을 적용한 카메라와 그 이전 설정(복원용). 미적용 상태면 AppliedCamera=nullptr.
	// 런타임 상태라 직렬화하지 않는다(UPROPERTY 아님).
	UCameraComponent*    AppliedCamera = nullptr;
	FPostProcessSettings SavedSettings = {};

	// PC->PlayerCameraManager->GetActiveCamera() 해석. 없으면 nullptr.
	UCameraComponent* ResolveActiveCamera() const;
};
