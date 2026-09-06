#pragma once

class UWorld;

// U 붙이는 이유: 추후 BlueprintFunctionLibrary 상속받아야 할 수 있어서
class UGamePlayStatics
{
public:
	static void SetGlobalTimeDilation(UWorld* World, float TimeDilation);
	static float GetGlobalTimeDilation(const UWorld* World);

	static float GetWorldDeltaTime(const UWorld* World);
	static float GetWorldRawDeltaTime(const UWorld* World);
};

