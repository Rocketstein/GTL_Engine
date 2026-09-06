#pragma once
class Time
{
public:
	static float DeltaTime;
	static float TimeSinceStart;
	static void UpdateTime(float deltaTime)
	{
		DeltaTime = deltaTime;
		TimeSinceStart += deltaTime;
	}
};

