#pragma once
#include <algorithm>

#include "CameraShakeBase.h"
#include "CameraShakeTypes.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/Object.h"

class UCameraShakeBase;


struct FCameraShakePatternStartParams
{
	/** 카메라 셰이크가 재생 중인 상태에서 다시 시작되는지 여부 */
	bool bIsRestarting = false;

	/** 카메라 셰이크의 지속 시간이 재정의되었는지 여부, DurationOverride 참고 */
	bool bOverrideDuration = false;

	/** 카메라 셰이크의 지속 시간을 선택적으로 재정의하는 값 */
	float DurationOverride = 0.f;
};

struct FCameraShakePatternUpdateParams
{
	FCameraShakePatternUpdateParams()
	{
	}

	//FCameraShakePatternUpdateParams(const FMinimalViewInfo& InPOV) : POV(InPOV) {}

	/** 마지막 업데이트 이후 경과한 시간 */
	float DeltaTime = 0.f;

	/** 이 셰이크에 적용되는 기본 스케일 */
	float ShakeScale = 1.f;

	/** 다음 업데이트를 위해 Camera Manager로부터 전달되는 동적 스케일 */
	float DynamicScale = 1.f;

	/** 이 Camera Shake가 수정해야 하는 현재 카메라 뷰 */
	//FMinimalViewInfo POV;

	/** 현재 업데이트 동안 Camera Shake에 적용할 최종 스케일. Shake * DynamicScale과 같다. */
	float GetTotalScale() const
	{
		return (std::max)(ShakeScale * DynamicScale, 0.f);
	}
};

struct FCameraShakePatternScrubParams
{
	FCameraShakePatternScrubParams() {}

	//FCameraShakePatternScrubParams(const FMinimalViewInfo& InPOV) : POV(InPOV) {}

	/** 이 값을 업데이트 파라미터 구조체로 변환한다. 이때 DeltaTime은 0부터 AbsoluteTime까지 경과한 시간으로 설정된다. */
	FCameraShakePatternUpdateParams ToUpdateParams() const;

	/** 스크럽할 대상 시간 */
	float AbsoluteTime = 0.f;

	/** 이 셰이크에 적용되는 기본 스케일 */
	float ShakeScale = 1.f;

	/** 다음 업데이트를 위해 Camera Manager로부터 전달되는 동적 스케일 */
	float DynamicScale = 1.f;

	/** 이 Camera Shake가 수정해야 하는 현재 카메라 뷰 */
	// FMinimalViewInfo POV;

	/** 현재 업데이트에서 Camera Shake에 적용할 최종스케일. ShakeScale * DynamicScale과 같다. */
	float GetTotalScale() const
	{
		return (std::max)(ShakeScale * DynamicScale, 0.f);
	}
};

struct FCameraShakePatternStopParams
{
	bool bImmediately = false;
};

class UCameraShakePattern : public UObject
{
public:
    DECLARE_CLASS(UCameraShakePattern, UObject)

    UCameraShakePattern()           = default;
    ~UCameraShakePattern() override = default;

	/** 이 셰이크 패턴에 대한 정보를 가져온다. */
	void GetShakePatternInfo(FCameraShakeInfo& OutInfo) const;
	/** 셰이크 패턴이 시작될 때 호출된다.*/
	void StartShakePattern(const FCameraShakePatternStartParams& Params);
	/** 셰이크 패턴을 업데이트한다. 생성된 오프셋을 주어진 결과에 더해야 한다. */
	void UpdateShakePattern(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult);
    /** 셰이크 패턴을 주어진 시간으로 스크럽하고, 생성된 오프셋을 주어진 결과에 적용한다. */
	void ScrubShakePattern(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult);
	/** 셰이크 패턴이 중료되었는지 여부를 반환한다. */
	bool IsFinished() const;
	/** 셰이크 패턴이 수동으로 중지될 때 호출된다. */
	void StopShakePattern(const FCameraShakePatternStopParams& Params);
	/** 셰이크 패턴이 폐기될 때 호출한다. 자연스럽게 끝난 뒤이거나, 수동으로 중지된 뒤일 수 있다. */
	void TeardownShakePattern();

protected:
	/** Gets the shake pattern's parent shake */
    UCameraShakeBase* GetShakeInstance() const;

	template<typename InstanceType>
	InstanceType* GetShakeInstance() const { return Cast<InstanceType>(GetShakeInstance()); }

private:
	// UCameraShakePattern interface
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const {}
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) {}
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) {}
	virtual void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult) {}
	virtual bool IsFinishedImpl() const { return true; }
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) {}
	virtual void TeardownShakePatternImpl() {}
};
