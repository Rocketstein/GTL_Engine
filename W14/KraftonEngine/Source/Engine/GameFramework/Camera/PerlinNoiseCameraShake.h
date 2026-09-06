#pragma once

#include "GameFramework/Camera/CameraShakeBase.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

#include "Source/Engine/GameFramework/Camera/PerlinNoiseCameraShake.generated.h"

// ============================================================
// UPerlinNoiseCameraShake — Perlin(gradient) 노이즈 기반 데이터 셰이크.
//
// UWaveOscillatorCameraShake 와 같은 채널 구성(Location/Rotation/FOV, amplitude·frequency,
// Duration·BlendIn/Out envelope) 을 쓰되, sin 진동 대신 1D Perlin 노이즈를 샘플링한다.
// sin 은 주기가 일정해 "규칙적으로 떨리는" 느낌이 나는 반면, Perlin 노이즈는 부드럽지만
// 비주기적이라 자연스럽고 불규칙한 충격(타격감) 을 낸다. UE: UPerlinNoiseCameraShakePattern.
//
// 채널마다 노이즈 입력 오프셋(seed) 을 StartShake 때 랜덤화 → 같은 셰이크를 연속 호출해도
// 매번 다른 패턴. PlaySpace 변환은 매니저 측에서 처리하므로 raw additive 만 채운다.
//
// 사용:
//   PC->GetPlayerCameraManager()->StartCameraShake<UPerlinNoiseCameraShake>(1.0f);
// ============================================================
UCLASS()
class UPerlinNoiseCameraShake : public UCameraShakeBase
{
public:
	GENERATED_BODY()
	UPerlinNoiseCameraShake() = default;
	~UPerlinNoiseCameraShake() override = default;

	void StartShake(
		APlayerCameraManager* Camera,
		float InScale,
		ECameraShakePlaySpace InPlaySpace,
		FRotator InUserPlaySpaceRot) override;

	void UpdateAndApplyCameraShake(float DeltaTime, FCameraShakeUpdateResult& OutResult) override;

	void StopShake(bool bImmediately = true) override;

	// ─── 튜닝 파라미터 ─────────────────────────────────────────────
	// Duration / Blend
	float Duration      = 0.5f;
	float BlendInTime   = 0.05f;
	float BlendOutTime  = 0.10f;

	// Location 노이즈 (world-space units). Frequency = 초당 노이즈 공간 이동 속도.
	FVector LocAmplitude = FVector(2.5f, 2.5f, 1.5f);
	FVector LocFrequency = FVector(10.0f, 12.0f, 8.0f);

	// Rotation 노이즈 (degrees, additive Pitch/Yaw/Roll).
	FRotator RotAmplitude = FRotator(0.6f, 0.9f, 1.0f);
	FRotator RotFrequency = FRotator(9.0f, 11.0f, 7.0f);

	// FOV 노이즈 (radians, additive).
	float FOVAmplitude = 0.02f;
	float FOVFrequency = 6.0f;

private:
	float ElapsedTime = 0.0f;

	// 매 StartShake 호출 시 채널별 노이즈 입력 오프셋 랜덤화 → 같은 셰이크도 다른 패턴.
	FVector LocSeed  = FVector(0.0f, 0.0f, 0.0f);
	FRotator RotSeed = FRotator(0.0f, 0.0f, 0.0f);
	float FOVSeed    = 0.0f;
};
