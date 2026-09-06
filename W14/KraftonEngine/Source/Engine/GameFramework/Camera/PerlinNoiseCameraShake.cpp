#include "GameFramework/Camera/PerlinNoiseCameraShake.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace
{
	// ── 1D Perlin(gradient) 노이즈 ────────────────────────────────────────────
	// 정수 격자점마다 ±1 기울기를 부여하고 smoothstep(fade) 으로 보간한다. 출력은 대략
	// [-1, 1]. sin 과 달리 주기가 없어 "규칙적인 떨림" 이 사라진다.
	float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
	float Lerp(float a, float b, float t) { return a + t * (b - a); }

	// 정수 좌표 → 해시(격자점 기울기 부호 결정용). 결정적이라 같은 좌표는 항상 같은 값.
	uint32_t IntHash(int32_t x)
	{
		uint32_t h = static_cast<uint32_t>(x);
		h ^= h >> 16; h *= 0x7feb352dU;
		h ^= h >> 15; h *= 0x846ca68bU;
		h ^= h >> 16;
		return h;
	}

	// 1D 기울기: ±1 → lerp 결과를 [-1,1] 로 정규화하기 쉽다.
	float Grad(int32_t ix, float fx)
	{
		return (IntHash(ix) & 1u) ? fx : -fx;
	}

	float Perlin1D(float x)
	{
		const int32_t i0 = static_cast<int32_t>(std::floor(x));
		const float   f  = x - static_cast<float>(i0);
		const float   u  = Fade(f);
		const float   g0 = Grad(i0,     f);
		const float   g1 = Grad(i0 + 1, f - 1.0f);
		return Lerp(g0, g1, u) * 2.0f;   // 1D Perlin 최대 진폭 ≈ 0.5 → ×2 로 [-1,1] 정규화
	}

	// 2-옥타브 fBm — 큰 흔들림 위에 미세 진동을 얹어 더 거친 타격감. 진폭 합으로 정규화해
	// 출력 범위를 [-1,1] 로 유지.
	float Noise1D(float x)
	{
		const float n0 = Perlin1D(x);
		const float n1 = Perlin1D(x * 2.0f + 31.7f);   // 옥타브2: 2배 빠르게, 위상 분리
		return (n0 + 0.5f * n1) / 1.5f;
	}

	constexpr float kSeedRange = 1000.0f;

	// rand() 0..RAND_MAX → [0, kSeedRange) 노이즈 입력 오프셋. 같은 셰이크라도 매번 다른 구간 샘플.
	float RandSeed()
	{
		return (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * kSeedRange;
	}
}

void UPerlinNoiseCameraShake::StartShake(
	APlayerCameraManager* Camera,
	float InScale,
	ECameraShakePlaySpace InPlaySpace,
	FRotator InUserPlaySpaceRot)
{
	Super::StartShake(Camera, InScale, InPlaySpace, InUserPlaySpaceRot);

	ElapsedTime = 0.0f;

	// 채널별 노이즈 입력 오프셋 랜덤화 → 채널 간 상관 제거 + 호출마다 다른 패턴.
	LocSeed = FVector(RandSeed(), RandSeed(), RandSeed());
	RotSeed = FRotator(RandSeed(), RandSeed(), RandSeed());
	FOVSeed = RandSeed();
}

void UPerlinNoiseCameraShake::UpdateAndApplyCameraShake(float DeltaTime, FCameraShakeUpdateResult& OutResult)
{
	if (bFinished || Duration <= 0.0f) return;

	ElapsedTime += DeltaTime;
	if (ElapsedTime >= Duration)
	{
		bFinished = true;
		return;
	}

	// Blend envelope — Duration 양 끝에서 amplitude 가 0 으로 부드럽게 ramp.
	float Envelope = 1.0f;
	if (BlendInTime > 0.0f && ElapsedTime < BlendInTime)
	{
		Envelope = ElapsedTime / BlendInTime;
	}
	else if (BlendOutTime > 0.0f && ElapsedTime > Duration - BlendOutTime)
	{
		Envelope = (Duration - ElapsedTime) / BlendOutTime;
	}

	const float t = ElapsedTime;
	const float Gain = Scale * Envelope;

	// Location (world-space — PlaySpace 변환은 매니저 책임. 현재는 raw 가산).
	OutResult.Location.X += Noise1D(t * LocFrequency.X + LocSeed.X) * LocAmplitude.X * Gain;
	OutResult.Location.Y += Noise1D(t * LocFrequency.Y + LocSeed.Y) * LocAmplitude.Y * Gain;
	OutResult.Location.Z += Noise1D(t * LocFrequency.Z + LocSeed.Z) * LocAmplitude.Z * Gain;

	// Rotation (degrees, additive Pitch/Yaw/Roll).
	OutResult.Rotation.Pitch += Noise1D(t * RotFrequency.Pitch + RotSeed.Pitch) * RotAmplitude.Pitch * Gain;
	OutResult.Rotation.Yaw   += Noise1D(t * RotFrequency.Yaw   + RotSeed.Yaw)   * RotAmplitude.Yaw   * Gain;
	OutResult.Rotation.Roll  += Noise1D(t * RotFrequency.Roll  + RotSeed.Roll)  * RotAmplitude.Roll  * Gain;

	// FOV (radians, additive).
	OutResult.FOV += Noise1D(t * FOVFrequency + FOVSeed) * FOVAmplitude * Gain;
}

void UPerlinNoiseCameraShake::StopShake(bool bImmediately)
{
	if (bImmediately)
	{
		bFinished = true;
		return;
	}

	// Soft stop — BlendOut 만 남기고 Duration 을 ElapsedTime 까지 단축.
	if (BlendOutTime > 0.0f && ElapsedTime + BlendOutTime < Duration)
	{
		Duration = ElapsedTime + BlendOutTime;
	}
	else
	{
		bFinished = true;
	}
}
