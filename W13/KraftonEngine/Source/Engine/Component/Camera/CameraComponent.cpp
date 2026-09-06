#include "Component/Camera/CameraComponent.h"
#include "Object/Reflection/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "Render/Types/MinimalViewInfo.h"
#include <cmath>

UCameraComponent::UCameraComponent()
{
	// DOF 는 기본 OFF — FPostProcessSettings 의 구조체 기본 Aperture(0.5)는 "DOF 켤 때의 적당한
	// 조리개"일 뿐, 카메라가 기본적으로 블러되면 안 된다. Aperture=0 이 DOFPass off 스위치이므로
	// 카메라 기본값을 0 으로 둔다. DOF 는 PostProcessVolume 진입이나 명시적 저작으로만 켜진다.
	PostProcessSettings.Aperture = 0.0f;
}

void UCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// E.2/3: PC 가 BeginPlay 시점엔 아직 spawn 전 → PlayerCameraManager nullptr.
	// PC 의 BeginPlay 에서 World 의 모든 카메라 컴포넌트를 catch up 등록하므로 안전.
	if (UWorld* World = GetOwner()->GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
			{
				CM->RegisterCamera(this);
			}
		}
	}
}

void UCameraComponent::EndPlay()
{
	Super::EndPlay();
	if (UWorld* World = GetOwner()->GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
			{
				CM->UnregisterCamera(this);
			}
		}
	}
}

void UCameraComponent::LookAt(const FVector& Target)
{
	FVector Position = GetWorldLocation();
	FVector Diff = (Target - Position).Normalized();

	constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

	FRotator LookRotation = GetRelativeRotation();
	LookRotation.Pitch = -asinf(Diff.Z) * Rad2Deg;

	if (fabsf(Diff.Z) < 0.999f) {
		LookRotation.Yaw = atan2f(Diff.Y, Diff.X) * Rad2Deg;
	}

	SetRelativeRotation(LookRotation);
}

void UCameraComponent::OnResize(int32 Width, int32 Height)
{
	CameraState.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
}

void UCameraComponent::SetCameraState(const FCameraState& NewState)
{
	CameraState = NewState;
}

void UCameraComponent::GetCameraView(float /*DeltaTime*/, FMinimalViewInfo& OutPOV) const
{
	UpdateWorldMatrix();
	OutPOV.Location    = GetWorldLocation();
	OutPOV.Rotation    = GetWorldMatrix().ToRotator();
	OutPOV.FOV         = CameraState.FOV;
	OutPOV.AspectRatio = CameraState.AspectRatio;
	OutPOV.OrthoWidth  = CameraState.OrthoWidth;
	OutPOV.NearClip    = CameraState.NearZ;
	OutPOV.FarClip     = CameraState.FarZ;
	OutPOV.bIsOrtho    = CameraState.bIsOrthogonal;
}