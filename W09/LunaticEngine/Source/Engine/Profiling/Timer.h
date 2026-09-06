#pragma once

#include <chrono>

class FTimer
{
public:
	FTimer() = default;

	void Initialize();
	void Tick();
	float GetDeltaTime() const { return DeltaTime; }
	double GetTotalTime() const { return TotalTime; }
	float GetFPS() const { return DeltaTime > 0.0f ? 1.0f / DeltaTime : 0.0f; }
	float GetDisplayFPS() const { return SmoothedFPS; }
	float GetFrameTimeMs() const { return DeltaTime * 1000.0f; }
	float GetTimeSinceLastTick() const;
	void SetMaxFPS(float InMaxFPS);
	float GetMaxFPS() const { return MaxFPS; }

private:
	// instead of using std::chrono::steady_clock
	using Clock = std::chrono::steady_clock;
	Clock::time_point LastTime = {};
	float DeltaTime = 0.0f;
	double TotalTime = 0.0;

	float MaxFPS = 0.0f;
	float TargetFrameTime = 0.0f;

	float SmoothedFPS = 0.0f;
};
