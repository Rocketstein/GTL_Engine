#pragma once
#include "Object/Object.h"
#include "CameraShakeTypes.h"
#include "Math/Matrix.h"

struct FCameraShakePatternStartParams;
struct FMinimalViewInfo;
class UCameraShakePattern;

enum class ECameraShakeDurationType : uint8
{
	/** 카메라 셰이크가 고정된 지속 시간을 가진다. */
	Fixed,

	/** 카메라 셰이크가 명시적으로 중지될 때까지 무기한 재생된다. */
	Infinite,

	/** 카메라 셰이크가 커스텀/동적 지속 시간을 가진다. */
	Custom
};

struct FCameraShakeDuration
{
	/** Returns as infinite shake duration */
	static FCameraShakeDuration Infinite() { return FCameraShakeDuration{ 0.f, ECameraShakeDurationType::Infinite }; }
	/** Returns a custom shake duration */
	static FCameraShakeDuration Custom() { return FCameraShakeDuration{ 0.f, ECameraShakeDurationType::Custom }; }
	/** Returns a custom shake duration with a hint for how long the shake might be */
	static FCameraShakeDuration Custom(float DurationHint) { return FCameraShakeDuration{ DurationHint, ECameraShakeDurationType::Custom }; }

	FCameraShakeDuration() : Duration(0.f), Type(ECameraShakeDurationType::Fixed) {}
	FCameraShakeDuration(float InDuration, ECameraShakeDurationType InType = ECameraShakeDurationType::Fixed) : Duration(InDuration), Type(InType) {}

	/** 지속 시간 타입을 반환한다. */
	ECameraShakeDurationType GetDurationType() const { return Type; }
	/** 이 지속 시간이 고정 시간인지 여부를 반환한다. */
	bool IsFixed() const { return Type == ECameraShakeDurationType::Fixed; }
	/** 이 지속 시간이 무한인지 여부를 반환한다. */
	bool IsInfinite() const { return Type == ECameraShakeDurationType::Infinite; }
	/** 이 지속 시간이 커스텀인지 여부를 반환한다. */
	bool IsCustom() const { return Type == ECameraShakeDurationType::Custom; }
	/** 이 지속 시간이 커스텀이면서, 유효한 실제 지속 시간 힌트를 가지고 있는지 여부를 반환한다. */
	bool IsCustomWithHint() const { return IsCustom() && Duration > 0.f; }

	/** 지속 시간이 Fixed 또는 Custom일 때, 지속 시간 값을 반환한다. */
	float Get() const { check(IsFixed() || IsCustom()); return Duration; }
private:
	float Duration;
	ECameraShakeDurationType Type;
};


struct FCameraShakeInfo
{
	FCameraShakeDuration Duration;

	float BlendIn = 0.f;

	float BlendOut = 0.f;
};

/**
 * 셰이크 또는 셰이크 패턴의 일시적 상태.
 */
struct FCameraShakeState
{
	FCameraShakeState();

    /**
     * 셰이크 정보를 사용해 상태를 초기화하고 재생을 시작한다. 
     */
    void Start(const FCameraShakeInfo& InShakeInfo);

	/**
    * 셰이크 정보를 사용해 상태를 초기화하고 재생을 시작한다.
    */
	void Start(const FCameraShakeInfo& InShakeInfo, std::optional<float> InDurationOverride);

	/**
	* 셰이크 정보를 사용해 상태를 초기화하고 재생을 시작한다.
	*/
	void Start(const UCameraShakePattern* InShakePattern);

	/**
	* 셰이크 정보를 사용해 상태를 초기화하고 재생을 시작한다.
	*/
	void Start(const UCameraShakePattern* InShakePattern, const FCameraShakePatternStartParams& InParams);

    /**
     * DeltaTime을 사용해 상태를 업데이트한다.
     *
     * 이 상태가 관리 대상이 아니라면, 즉 고정된 지속 시간 정보가 없다면,
     * 아무 작업도 하지 않고 1을 반환한다.
     *
     * 이 상태가 관리 대상이라면, 이 클래스의 내부 상태를 업데이트하고
     * 셰이크가 언제 종료되었는지를 자동으로 계산한다.
     * 또한 블렌드 인/아웃이 있다면 그에 따른 블렌딩 가중치도 함께 계산한다.
     *
     * @param DeltaTime 마지막 업데이트 이후 경과한 시간.
     * @return 새로운 시간에 대해 계산된 블렌딩 가중치. 블렌딩이 없다면 기본값
     */
    float Update(float DeltaTime);

    /**
     * 상태를 주어진 절대 시간으로 스크럽한다.
     *
     * 이 상태가 관리 대상이 아니라면, 즉 고정된 지속 시간 정보가 없다면,
     * 아무 작업도 하지 않고 1을 반환한다.
     *
     * 이 상태가 관리 대상이라면, 이 클래스의 내부 상태를 업데이트하고
     * 셰이크가 언제 종료되었는지를 자동으로 계산한다.
     * 또한 블렌드 인/아웃이 있다면 그에 따른 블렌딩 가중치도 함께 계산한다.
     *
     * @param AbsoluteTime 스크럽할 대상 시간
     * @return 스크럽 시간에 대해 계산된 블렌딩 가중치. 블렌딩이 없다면 기본값
     */
    float Scrub(float AbsoluteTime);

    /**
     * 셰이크가 중지되었음을 표시한다.
     *
     * 즉시 중지해야 한다면 셰이크를 비활성 상태로 만든다.
     * 즉시 중지하지 않아도 된다면, 블렌드 아웃이 있는 경우 셰이크의 블렌드 아웃을 시작한다.
     *
     * 지속 시간 또는 블렌딩 정보가 없다면, 즉 셰이크 지속 시간이 "Custom"이라면,
     * 즉시 중지하지 않는 Stop은 아무 작업도 하지 않는다.
     * 이 경우 하위 클래스가 직접 처리해야 한다.
     */
    void Stop(bool bImmediately);

	/** 셰이크가 재생 중인지 여부를 반환한다. */
	bool IsPlaying() const { return bIsPlaying; }

	/** 셰이크가 블렌드 인 중인지 여부를 반환한다. */
	bool IsBlendingIn() const { return bIsBlendingIn; }

	/** 셰이크가 블렌드 아웃 중인지 여부를 반환한다. */
	bool IsBlendingOut() const { return bIsBlendingOut; }

	/** 현재 셰이크 실행에서 경과한 시간을 반환한다. */
	float GetElapsedTime() const { return ElapsedTime; }

	/** 현재 블렌드 인 진행 시간을 반환한다. IsBlendingIn()이 true일 때만 유효하다.  */
	float GetCurrentBlendInTime() const { return CurrentBlendInTime; }

	/** 현재 블렌드 아웃 진행 시간을 반환한다. IsBlendingOut()이 true 일 때만 유효하다. */
	float GetCurrentBlendOutTime() const { return CurrentBlendOutTime; }

	/** 현재 셰이크 정보를 반환한다. */
	const FCameraShakeInfo& GetShakeInfo() const { return ShakeInfo; }

	/** GetShakeInfo().Duration.IsFixed()를 가져오기 위한 헬퍼 함수 */
	bool HasDuration() const { return ShakeInfo.Duration.IsFixed(); }

	/** GetShakeInfO().Duration.Get()을 가져오기 위한 헬퍼 함수 */
	float GetDuration() const { return ShakeInfo.Duration.Get(); }

	/** GetShakeInfo().Duration.IsInfinite()를 가져오기 위한 헬퍼 함수 */
	bool IsInfinite() const { return ShakeInfo.Duration.IsInfinite(); }

private:
	void InitializePlaying();

private:
	FCameraShakeInfo ShakeInfo;

	float ElapsedTime;

	float CurrentBlendInTime;
	float CurrentBlendOutTime;

	bool bIsBlendingIn : 1; // << 이거 신기하네.
	bool bIsBlendingOut : 1;

	bool bIsPlaying : 1;

	// Cached values for blending information
	bool bHasBlendIn : 1;
	bool bHasBlendOut : 1;
};

class UCameraShakeBase : public UObject
{
public:
    DECLARE_CLASS(UCameraShakeBase, UObject)

    UCameraShakeBase() = default;
    ~UCameraShakeBase() override;

	void SetRootShakePattern(UCameraShakePattern* InPattern);
	UCameraShakePattern* GetRootShakePattern() const { return RootShakePattern; }

	void StartShake(const FCameraShakeBaseStartParams& Params);
	void UpdateAndApplyCameraShake(float DeltaTime, float DynamicScale, FMinimalViewInfo& InOutPOV);
	void StopShake(bool bImmediately = false);
	void TeardownShake();
	bool IsFinished() const;
	bool IsActive() const { return bIsActive; }

	void SetShakeScale(float InShakeScale) { ShakeScale = InShakeScale; }
	float GetShakeScale() const { return ShakeScale; }

	APlayerCameraManager* GetCameraManager() const { return CameraManager; }
	void SetSourceAssetPath(const FString& InSourceAssetPath) { SourceAssetPath = InSourceAssetPath; }
	const FString& GetSourceAssetPath() const { return SourceAssetPath; }

public:
	bool bSingleInstance = false;

private:
	void ApplyResult(const FCameraShakeApplyResultParams& Params, const FCameraShakePatternUpdateResult& Result, FMinimalViewInfo& InOutPOV) const;

private:
	UCameraShakePattern* RootShakePattern = nullptr;
	float ShakeScale = 1.f;
	bool bIsActive = false;
	APlayerCameraManager* CameraManager = nullptr;
	ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;
	FRotator UserPlaySpaceRot = FRotator::ZeroRotator;
	FMatrix UserPlaySpaceMatrix = FMatrix::Identity;
	FString SourceAssetPath;

};
