#include "Camera/Shakes/CameraShakeBase.h"

#include "Camera/Shakes/CameraShakePattern.h"
#include "Engine/Classes/Camera/CameraManager.h"
#include "Object/ObjectFactory.h"

FCameraShakeState::FCameraShakeState()
	: ElapsedTime(0.f)
	, CurrentBlendInTime(0.f)
	, CurrentBlendOutTime(0.f)
	, bIsBlendingIn(false)
	, bIsBlendingOut(false)
	, bIsPlaying(false)
	, bHasBlendIn(false)
	, bHasBlendOut(false)
{
}

void FCameraShakeState::Start(const FCameraShakeInfo& InShakeInfo)
{
	Start(InShakeInfo, TOptional<float>());
}

void FCameraShakeState::Start(const FCameraShakeInfo& InShakeInfo, std::optional<float> InDurationOverride)
{
	const FCameraShakeState PrevState(*this);

	ShakeInfo = InShakeInfo;
	bHasBlendIn = ShakeInfo.BlendIn > 0.f;
	bHasBlendOut = ShakeInfo.BlendOut > 0.f;

	if (InDurationOverride.has_value())
	{
		ShakeInfo.Duration = FCameraShakeDuration(InDurationOverride.value());
	}

	InitializePlaying();

	if (PrevState.bIsPlaying)
	{
		// 단일 인스턴스 셰이크가 다시 시작되는 중이다...
		// 블렌드 아웃을 블렌드 인으로 되돌려야 하는지 확인한다.
	    if (bHasBlendIn && PrevState.bHasBlendOut && PrevState.bIsBlendingOut)
	    {
			// 이미 블렌드 아웃이 시작된 상태였다.
			// 현재 블렌드 아웃 가중치와 동일한 수준의 가중치가 되도록,
			// 블렌드 인의 중간 지점부터 시작한다.
			CurrentBlendInTime = ShakeInfo.BlendIn * (1.f - PrevState.CurrentBlendOutTime / PrevState.ShakeInfo.BlendOut);
	    }
		else if (bHasBlendIn && !PrevState.bHasBlendOut)
		{
			// 이전  셰이크에는 블렌드 아웃이 없었으므로,
			// 여전히 100% 강도로 재생 중이었다.
			// 그런데 새 블렌드 인의 시작점인 0%로 갑자기 떨어지면 안 되므로,
			// 블렌드 인을 건너뛴다.
			bIsBlendingIn = false;
		}
	}
}

void FCameraShakeState::Start(const UCameraShakePattern* InShakePattern)
{
	check(InShakePattern);
	FCameraShakeInfo Info;
	InShakePattern->GetShakePatternInfo(Info);
	Start(Info);
}

void FCameraShakeState::Start(const UCameraShakePattern* InShakePattern, const FCameraShakePatternStartParams& InParams)
{
	check(InShakePattern);
	FCameraShakeInfo Info;
	InShakePattern->GetShakePatternInfo(Info);
	TOptional<float> DurationOverride;
	if (InParams.bOverrideDuration)
	{
		DurationOverride = InParams.DurationOverride;
	}
	Start(Info, DurationOverride);
}

float FCameraShakeState::Update(float DeltaTime)
{
	float BlendingWeight = 1.f;

	ElapsedTime += DeltaTime;
	if (bIsBlendingIn)
	{
		CurrentBlendInTime += DeltaTime;
	}
	if (bIsBlendingOut)
	{
		CurrentBlendOutTime += DeltaTime;
	}

	if (HasDuration())
	{
		const float ShakeDuration = ShakeInfo.Duration.Get();
		if (ElapsedTime < 0.f || ElapsedTime >= ShakeDuration)
		{
			bIsPlaying = false;
			return 0.f;
		}

		const float DurationRemaining = ShakeDuration - ElapsedTime;
		if (bHasBlendOut && !bIsBlendingOut && DurationRemaining < ShakeInfo.BlendOut)
		{
			bIsBlendingOut = true;
			CurrentBlendOutTime = ShakeInfo.BlendOut - DurationRemaining;
		}
	}

	// Compute blend-in and blend-out weight.
	if (bHasBlendIn && bIsBlendingIn)
	{
		if (CurrentBlendInTime < ShakeInfo.BlendIn)
		{
			BlendingWeight *= (CurrentBlendInTime / ShakeInfo.BlendIn);
		}
		else
		{
			// Finished blending in!
			bIsBlendingIn = false;
			CurrentBlendInTime = ShakeInfo.BlendIn;
		}
	}
	if (bHasBlendOut && bIsBlendingOut)
	{
		if (CurrentBlendOutTime < ShakeInfo.BlendOut)
		{
			BlendingWeight *= (1.f - CurrentBlendOutTime / ShakeInfo.BlendOut);
		}
		else
		{
			// Finished blending out!
			bIsBlendingOut = false;
			CurrentBlendOutTime = ShakeInfo.BlendOut;
			// We also end the shake itself. In most cases we would have hit the similar case
			// above already, but if we have an infinite shake we have no duration to reach the end
			// of so we only finish here.
			bIsPlaying = false;
			return 0.f;
		}
	}
	return BlendingWeight;
}

float FCameraShakeState::Scrub(float AbsoluteTime)
{
	// Reset the state to active, at the beginning, and update from there.
	InitializePlaying();
	return Update(AbsoluteTime);
}

void FCameraShakeState::Stop(bool bImmediately)
{
	// For stopping immediately, we don't do anything besides render the shake inactive.
	if (bImmediately || !bHasBlendOut)
	{
		bIsPlaying = false;
	}
	// For stopping with a "graceful" blend-out:
	// - If we are already blending out, let's keep doing that and not change anything.
	// - If we are not, let's start blending out.
	else if (bHasBlendOut && !bIsBlendingOut)
	{
		bIsBlendingOut = true;
		CurrentBlendOutTime = 0.f;
	}
}

void FCameraShakeState::InitializePlaying()
{
	ElapsedTime = 0.f;
	CurrentBlendInTime = 0.f;
	CurrentBlendOutTime = 0.f;
	bIsBlendingIn = bHasBlendIn;
	bIsBlendingOut = false;
	bIsPlaying = true;
}

IMPLEMENT_CLASS(UCameraShakeBase, UObject)

UCameraShakeBase::~UCameraShakeBase()
{
	TeardownShake();

	if (RootShakePattern && RootShakePattern->GetOuter() == this)
	{
		UObjectManager::Get().DestroyObject(RootShakePattern);
	}
	RootShakePattern = nullptr;
}

void UCameraShakeBase::SetRootShakePattern(UCameraShakePattern* InPattern)
{
	if (RootShakePattern == InPattern)
	{
		return;
	}

	if (RootShakePattern && RootShakePattern->GetOuter() == this)
	{
		UObjectManager::Get().DestroyObject(RootShakePattern);
	}

	RootShakePattern = InPattern;
	if (RootShakePattern)
	{
		RootShakePattern->SetOuter(this);
	}
}

void UCameraShakeBase::StartShake(const FCameraShakeBaseStartParams& Params)
{
	if (RootShakePattern == nullptr)
	{
		bIsActive = false;
		return;
	}

	const bool bWasActive = bIsActive;

	CameraManager = Params.CameraManager;
	ShakeScale = Params.ShakeScale;
	PlaySpace = Params.PlaySpace;
	UserPlaySpaceRot = Params.UserPlaySpaceRot;
	UserPlaySpaceMatrix = UserPlaySpaceRot.ToMatrix();
	bIsActive = true;

	FCameraShakePatternStartParams PatternParams;
	PatternParams.bIsRestarting = bWasActive;
	PatternParams.bOverrideDuration = Params.bOverrideDuration;
	PatternParams.DurationOverride = Params.DurationOverride;

	RootShakePattern->StartShakePattern(PatternParams);
}

void UCameraShakeBase::UpdateAndApplyCameraShake(float DeltaTime, float DynamicScale, FMinimalViewInfo& InOutPOV)
{
	if (!bIsActive || RootShakePattern == nullptr)
	{
		return;
	}

	if (RootShakePattern->IsFinished())
	{
		bIsActive = false;
		return;
	}

	FCameraShakePatternUpdateParams UpdateParams;
	UpdateParams.DeltaTime = DeltaTime;
	UpdateParams.ShakeScale = ShakeScale;
	UpdateParams.DynamicScale = DynamicScale;

	FCameraShakePatternUpdateResult Result;
	RootShakePattern->UpdateShakePattern(UpdateParams, Result);

	FCameraShakeApplyResultParams ApplyParams;
	ApplyParams.ShakeScale = ShakeScale;
	ApplyParams.DynamicScale = DynamicScale;
	ApplyParams.PlaySpace = PlaySpace;
	ApplyParams.CameraRotation = InOutPOV.Rotation;
	ApplyParams.UserPlaySpaceRot = UserPlaySpaceRot;

	ApplyResult(ApplyParams, Result, InOutPOV);

	if (RootShakePattern->IsFinished())
	{
		bIsActive = false;
	}
}

void UCameraShakeBase::StopShake(bool bImmediately)
{
	if (RootShakePattern == nullptr)
	{
		bIsActive = false;
		return;
	}

	FCameraShakePatternStopParams StopParams;
	StopParams.bImmediately = bImmediately;
	RootShakePattern->StopShakePattern(StopParams);

	if (bImmediately)
	{
		bIsActive = false;
	}
}

void UCameraShakeBase::TeardownShake()
{
	if (RootShakePattern)
	{
		RootShakePattern->TeardownShakePattern();
	}

	bIsActive = false;
	CameraManager = nullptr;
}

bool UCameraShakeBase::IsFinished() const
{
	return !bIsActive || RootShakePattern == nullptr || RootShakePattern->IsFinished();
}

void UCameraShakeBase::ApplyResult(const FCameraShakeApplyResultParams& Params, const FCameraShakePatternUpdateResult& Result, FMinimalViewInfo& InOutPOV) const
{
	FCameraShakePatternUpdateResult ScaledResult = Result;

	if (!HasCameraShakePatternUpdateResultFlag(ScaledResult.Flags, ECameraShakePatternUpdateResultFlags::SkipAutoScale))
	{
		ScaledResult.ApplyScale(Params.GetTotalScale());
	}

	if (!HasCameraShakePatternUpdateResultFlag(ScaledResult.Flags, ECameraShakePatternUpdateResultFlags::SkipAutoPlaySpace))
	{
		switch (Params.PlaySpace)
		{
		case ECameraShakePlaySpace::CameraLocal:
			ScaledResult.Location = Params.CameraRotation.ToMatrix().TransformVector(ScaledResult.Location);
			break;
		case ECameraShakePlaySpace::UserDefined:
			ScaledResult.Location = UserPlaySpaceMatrix.TransformVector(ScaledResult.Location);
			break;
		case ECameraShakePlaySpace::World:
		default:
			break;
		}
	}

	if (HasCameraShakePatternUpdateResultFlag(ScaledResult.Flags, ECameraShakePatternUpdateResultFlags::ApplyAsAbsolute))
	{
		InOutPOV.Location = ScaledResult.Location;
		InOutPOV.Rotation = ScaledResult.Rotation;
		InOutPOV.FOV = ScaledResult.FOV;
	}
	else
	{
		InOutPOV.Location += ScaledResult.Location;
		InOutPOV.Rotation += ScaledResult.Rotation;
		InOutPOV.FOV += ScaledResult.FOV;
	}
}
