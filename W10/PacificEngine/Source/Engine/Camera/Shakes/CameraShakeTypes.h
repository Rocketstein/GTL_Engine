#pragma once

#include "Core/CoreTypes.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

#include <algorithm>

class APlayerCameraManager;

enum class ECameraShakePlaySpace : uint8
{
	CameraLocal,
	World,
	UserDefined
};

struct FCameraShakeBaseStartParams
{
	APlayerCameraManager* CameraManager = nullptr;

	float ShakeScale = 1.f;

	ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;

	FRotator UserPlaySpaceRot = FRotator::ZeroRotator;

	bool bOverrideDuration = false;

	float DurationOverride = 0.f;
};

struct FCameraShakeApplyResultParams
{
	float ShakeScale = 1.f;

	float DynamicScale = 1.f;

	ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;

	FRotator CameraRotation = FRotator::ZeroRotator;

	FRotator UserPlaySpaceRot = FRotator::ZeroRotator;

	float GetTotalScale() const
	{
		return (std::max)(ShakeScale * DynamicScale, 0.f);
	}
};

enum class ECameraShakePatternUpdateResultFlags : uint8
{
	ApplyAsAbsolute = 1 << 0,
	SkipAutoScale = 1 << 1,
	SkipAutoPlaySpace = 1 << 2,

	Default = 0
};

inline bool HasCameraShakePatternUpdateResultFlag(
	ECameraShakePatternUpdateResultFlags Flags,
	ECameraShakePatternUpdateResultFlags FlagToCheck)
{
	return (static_cast<uint8>(Flags) & static_cast<uint8>(FlagToCheck)) != 0;
}

struct FCameraShakePatternUpdateResult
{
	FCameraShakePatternUpdateResult()
		: Location(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, FOV(0.f)
		, Flags(ECameraShakePatternUpdateResultFlags::Default)
	{
	}

	FVector Location;

	FRotator Rotation;

	float FOV;

	ECameraShakePatternUpdateResultFlags Flags;

	void ApplyScale(float InScale);
};
