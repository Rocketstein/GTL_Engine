#include "GamePlayStatics.h"
#include "GameFramework/World.h"

void UGamePlayStatics::SetGlobalTimeDilation(UWorld* World, float TimeDilation)
{
	if (!World)
	{
		return;
	}
	World->SetGlobalTimeDilation(TimeDilation);
}

float UGamePlayStatics::GetGlobalTimeDilation(const UWorld* World)
{
	return World ? World->GetGlobalTimeDilation() : 1.0f;
}

float UGamePlayStatics::GetWorldDeltaTime(const UWorld* World)
{
	return World ? World->GetDeltaTime() : 0.f;
}

float UGamePlayStatics::GetWorldRawDeltaTime(const UWorld* World)
{
	return World ? World->GetRawDeltaTime() : 0.f;
}
