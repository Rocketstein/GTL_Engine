#include "Camera/Shakes/CameraShakePattern.h"

#include "Object/ObjectFactory.h"
#include "Camera/Shakes/CameraShakeBase.h"

FCameraShakePatternUpdateParams FCameraShakePatternScrubParams::ToUpdateParams() const
{
	//FCameraShakePatternUpdateParams UpdateParams(POV);
	FCameraShakePatternUpdateParams UpdateParams;
	UpdateParams.DeltaTime = AbsoluteTime;
	UpdateParams.ShakeScale = ShakeScale;
	UpdateParams.DynamicScale = DynamicScale;
	return UpdateParams;
}

void FCameraShakePatternUpdateResult::ApplyScale(float InScale)
{
	Location *= InScale;
	Rotation = Rotation * InScale;
	FOV *= InScale;
}

IMPLEMENT_CLASS(UCameraShakePattern, UObject)

void UCameraShakePattern::GetShakePatternInfo(FCameraShakeInfo& OutInfo) const
{
	GetShakePatternInfoImpl(OutInfo);
}

void UCameraShakePattern::StartShakePattern(const FCameraShakePatternStartParams& Params)
{
	StartShakePatternImpl(Params);
}

void UCameraShakePattern::UpdateShakePattern(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	UpdateShakePatternImpl(Params, OutResult);
}

void UCameraShakePattern::ScrubShakePattern(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	ScrubShakePatternImpl(Params, OutResult);
}

bool UCameraShakePattern::IsFinished() const
{
	return IsFinishedImpl();
}

void UCameraShakePattern::StopShakePattern(const FCameraShakePatternStopParams& Params)
{
	StopShakePatternImpl(Params);
}

void UCameraShakePattern::TeardownShakePattern()
{
	TeardownShakePatternImpl();
}

UCameraShakeBase* UCameraShakePattern::GetShakeInstance() const
{
	return GetTypedOuter<UCameraShakeBase>();
}