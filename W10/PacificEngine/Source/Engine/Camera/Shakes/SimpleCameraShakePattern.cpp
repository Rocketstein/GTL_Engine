#include "SimpleCameraShakePattern.h"

#include "Object/ObjectFactory.h"

IMPLEMENT_ABSTRACT_CLASS(USimpleCameraShakePattern, UCameraShakePattern)

void USimpleCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	if (Duration > 0.f)
	{
		OutInfo.Duration = Duration;
	}
	else
	{
		OutInfo.Duration = FCameraShakeDuration::Infinite();
	}

	OutInfo.BlendIn = BlendInTime;
	OutInfo.BlendOut = BlendOutTime;
}

void USimpleCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	State.Start(this, Params);
}

bool USimpleCameraShakePattern::IsFinishedImpl() const
{
	return !State.IsPlaying();
}

void USimpleCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	State.Stop(Params.bImmediately);
}

void USimpleCameraShakePattern::TeardownShakePatternImpl()
{
	State = FCameraShakeState();
}