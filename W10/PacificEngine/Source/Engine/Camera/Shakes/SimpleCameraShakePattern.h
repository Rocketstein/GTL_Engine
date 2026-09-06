#pragma once
#include "CameraShakePattern.h"



class USimpleCameraShakePattern : public UCameraShakePattern
{
public:
	DECLARE_CLASS(USimpleCameraShakePattern, UCameraShakePattern)

	/** 이 셰이크의 지속 시간(초). 0 이하이면 무한 지속을 의미한다. */
	float Duration = 1.f;

	/** 이 셰이크의 블렌드 인 시간. 0 이하이면 블렌드 인이 없음을 의미한다. */
	float BlendInTime = 0.2f;

	/** 이 셰이크의 블렌드 아웃 시간. 0 이하이면 블렌드 아웃이 없음을 의미한다. */
	float BlendOutTime = 0.2f;

protected:
    void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
    void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
    bool IsFinishedImpl() const override;
    void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
    void TeardownShakePatternImpl() override;

protected:
	FCameraShakeState State;
};
