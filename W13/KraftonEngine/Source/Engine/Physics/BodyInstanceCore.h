#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Physics/BodyInstanceCore.generated.h"

class UBodySetupCore;

USTRUCT()
struct FBodyInstanceCore
{
	GENERATED_BODY()

	// BodySetupCore pointer that this instance is initialized from.
	UBodySetupCore* BodySetup = nullptr;

	FBodyInstanceCore();
	virtual ~FBodyInstanceCore() = default;

	/*
	 * If true, this body will use simulation. If false, it will be fixed
	 * (kinematic) and move where it is told.
	 */
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Simulate Physics")
	bool bSimulatePhysics;

	// If true, mass will not be automatically computed and must be set directly.
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Override Mass")
	bool bOverrideMass;

	// If object should have the force of gravity applied.
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Enable Gravity")
	bool bEnableGravity;

	// If true and attached to a parent, this body can be welded into the parent body.
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Auto Weld")
	bool bAutoWeld;

	// If object should start awake, or if it should initially be sleeping.
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Start Awake")
	bool bStartAwake;

	// Should wake/sleep events fire when this object wakes up or sleeps.
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Generate Wake Events")
	bool bGenerateWakeEvents;

	// If true, mass should be updated when scale changes.
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Update Mass When Scale Changes")
	bool bUpdateMassWhenScaleChanges;

	bool ShouldInstanceSimulatingPhysics() const;
};
