#pragma once

#include <chrono>

class FTimer
{
public:
	FTimer() = default;

	void Initialize();
	void Tick();
	float GetDeltaTime() const { return DeltaTime * TimeDilation; }
	float GetRawDeltaTime() const { return DeltaTime; }
	double GetTotalTime() const { return TotalTime; }
	float GetTimeDilation() const { return TimeDilation; }

	float GetFPS() const { return DeltaTime > 0.0f ? 1.0f / DeltaTime : 0.0f; }
	float GetDisplayFPS() const { return SmoothedFPS; }
	float GetFrameTimeMs() const { return DeltaTime * 1000.0f; }

	void SetTimeDilation(float Dilation) { TimeDilation = Dilation; }
	void SetMaxFPS(float InMaxFPS);
	float GetMaxFPS() const { return MaxFPS; }

	// 시뮬레이션에 전달되는 프레임 델타의 상한(초). 레벨 로드/메시 쿠킹/브레이크포인트/alt-tab
	// 같은 스톨이 거대한 dt 한 방으로 들어와 물리(스윕)가 지오메트리를 관통하는 것을 막는다.
	void SetMaxDeltaTime(float InMaxDeltaTime) { MaxDeltaTime = InMaxDeltaTime; }
	float GetMaxDeltaTime() const { return MaxDeltaTime; }

private:
	using Clock = std::chrono::high_resolution_clock;
	Clock::time_point LastTime = {};
	float DeltaTime = 0.0f;
	double TotalTime = 0.0;

	float MaxFPS = 0.0f;
	float TargetFrameTime = 0.0f;

	// 기본 0.1s(=10 FPS 바닥). 정상 프레임에는 영향 없고, 스톨 프레임만 잘라낸다.
	float MaxDeltaTime = 0.1f;

	float TimeDilation = 1.0f;
	float SmoothedFPS = 0.0f;
};
