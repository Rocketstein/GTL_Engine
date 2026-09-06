#include "Engine/Profiling/Time/Timer.h"

void FTimer::Initialize()
{
	LastTime = Clock::now();
}

void FTimer::Tick()
{
	if (TargetFrameTime > 0.0f)
	{
		Clock::time_point Now;
		float Elapsed;
		do
		{
			Now = Clock::now();
			Elapsed = std::chrono::duration<float>(Now - LastTime).count();
		} while (Elapsed < TargetFrameTime);
	}

	Clock::time_point CurrentTime = Clock::now();
	const float MeasuredDeltaTime = std::chrono::duration<float>(CurrentTime - LastTime).count();
	LastTime = CurrentTime;

	// 스톨(레벨 로드/메시 쿠킹/브레이크포인트 등)로 생긴 거대한 dt 는 상한으로 잘라
	// 물리 스윕이 한 프레임에 지오메트리를 건너뛰는 것을 방지한다. FPS 표시는 실측값 사용.
	DeltaTime = (MaxDeltaTime > 0.0f && MeasuredDeltaTime > MaxDeltaTime)
		? MaxDeltaTime
		: MeasuredDeltaTime;
	TotalTime += DeltaTime;

	// EMA로 FPS 보간 (UE 방식) — 실제 측정 dt 기준
	const float CurrentFPS = MeasuredDeltaTime > 0.0f ? 1.0f / MeasuredDeltaTime : 0.0f;
	constexpr float Smoothing = 0.2f; // 낮을수록 더 부드러움
	SmoothedFPS = (SmoothedFPS == 0.0f)
		? CurrentFPS
		: SmoothedFPS + (CurrentFPS - SmoothedFPS) * Smoothing;
}

void FTimer::SetMaxFPS(float InMaxFPS)
{
	MaxFPS = InMaxFPS;
	TargetFrameTime = MaxFPS > 0.0f ? 1.0f / MaxFPS : 0.0f;
}
