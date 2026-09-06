#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

// 타격/충돌 순간 카메라 셰이크를 1회 발동하는 instant notify.
//   - 시퀀스 타임라인의 임팩트 프레임에 배치하면 그 시점에 플레이어 카메라가 흔들린다.
//   - 기본은 Perlin 노이즈 셰이크(UPerlinNoiseCameraShake) — 불규칙·자연스러운 타격감.
//     bUsePerlinNoise=false 면 기존 sin 기반(UWaveOscillatorCameraShake) 사용.
//   - amplitude/frequency/duration 등은 Notify Properties 패널에서 편집 + .uasset round-trip.
//   - 플레이어 카메라 매니저가 없으면(에디터 비-PIE 등) early-out → 무영향.

#include "Source/Engine/Animation/Notify/AnimNotify_CameraShake.generated.h"

UCLASS()
class UAnimNotify_CameraShake : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_CameraShake() = default;
	~UAnimNotify_CameraShake() override = default;

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="Use Perlin Noise")
	bool bUsePerlinNoise = true;

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="Scale", Min=0.0f, Max=20.0f, Speed=0.05f)
	float Scale = 1.0f;

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="Duration", Min=0.0f, Max=10.0f, Speed=0.01f)
	float Duration = 0.4f;

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="Blend In Time", Min=0.0f, Max=5.0f, Speed=0.01f)
	float BlendInTime = 0.04f;

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="Blend Out Time", Min=0.0f, Max=5.0f, Speed=0.01f)
	float BlendOutTime = 0.12f;

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="Location Amplitude")
	FVector LocationAmplitude = FVector(2.5f, 2.5f, 1.5f);

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="Location Frequency")
	FVector LocationFrequency = FVector(10.0f, 12.0f, 8.0f);

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="Rotation Amplitude", Type=Rotator)
	FRotator RotationAmplitude = FRotator(0.6f, 0.9f, 1.0f);

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="Rotation Frequency", Type=Rotator)
	FRotator RotationFrequency = FRotator(9.0f, 11.0f, 7.0f);

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="FOV Amplitude", Min=0.0f, Max=1.0f, Speed=0.005f)
	float FOVAmplitude = 0.02f;

	UPROPERTY(Edit, Save, Category="CameraShake", DisplayName="FOV Frequency", Min=0.0f, Max=60.0f, Speed=0.1f)
	float FOVFrequency = 6.0f;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
