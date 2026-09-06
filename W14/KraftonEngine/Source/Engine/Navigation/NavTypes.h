#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Navigation/NavTypes.generated.h"

UENUM()
enum class EPathFollowingStatus : uint8
{
	Idle = 0,
	Waiting = 1,
	Moving = 2,
	Paused = 3,
};

UENUM()
enum class EPathFollowingResult : uint8
{
	Success = 0,
	Blocked = 1,
	OffPath = 2,
	Aborted = 3,
	Invalid = 4,
};

UENUM()
enum class EPathFollowingRequestResult : uint8
{
	Failed = 0,
	AlreadyAtGoal = 1,
	RequestSuccessful = 2,
};

USTRUCT()
struct FNavAgentProperties
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Radius", Min=0.01f, Max=100.0f, Speed=0.05f)
	float Radius = 1.8f;

	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Height", Min=0.01f, Max=200.0f, Speed=0.1f)
	float Height = 6.0f;

	// Legacy alias used by older scenes and fallback runtime queries.  New path checks use
	// MaxClimbHeight / MaxDropHeight so different characters can step up and drop down
	// with separate limits.
	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Step Height", Min=0.0f, Max=50.0f, Speed=0.05f)
	float StepHeight = 0.6f;

	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Max Climb Height", Min=0.0f, Max=50.0f, Speed=0.01f)
	float MaxClimbHeight = 0.6f;

	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Max Drop Height", Min=0.0f, Max=50.0f, Speed=0.01f)
	float MaxDropHeight = 0.6f;

	float GetEffectiveMaxClimbHeight() const
	{
		return MaxClimbHeight > 0.0f ? MaxClimbHeight : StepHeight;
	}

	float GetEffectiveMaxDropHeight() const
	{
		return MaxDropHeight > 0.0f ? MaxDropHeight : StepHeight;
	}

	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Max Slope Degrees", Min=0.0f, Max=89.0f, Speed=1.0f)
	float MaxSlopeDegrees = 50.0f;
};

USTRUCT()
struct FNavPathPoint
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Location", Type=Vec3)
	FVector Location = FVector::ZeroVector;

	FNavPathPoint() = default;
	explicit FNavPathPoint(const FVector& InLocation) : Location(InLocation) {}
};

USTRUCT()
struct FNavigationPath
{
	GENERATED_BODY()

	TArray<FNavPathPoint> PathPoints;
	bool bIsPartial = false;
	bool bIsValid = false;

	void Reset()
	{
		PathPoints.clear();
		bIsPartial = false;
		bIsValid = false;
	}

	bool IsValid() const { return bIsValid && !PathPoints.empty(); }
	int32 Num() const { return static_cast<int32>(PathPoints.size()); }
	FVector GetPathPointLocation(int32 Index) const
	{
		if (Index < 0 || Index >= static_cast<int32>(PathPoints.size()))
		{
			return FVector::ZeroVector;
		}
		return PathPoints[static_cast<size_t>(Index)].Location;
	}
	FVector GetEndLocation() const
	{
		return PathPoints.empty() ? FVector::ZeroVector : PathPoints.back().Location;
	}
};
