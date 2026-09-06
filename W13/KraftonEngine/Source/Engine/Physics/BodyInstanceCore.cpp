#include "Physics/BodyInstanceCore.h"

FBodyInstanceCore::FBodyInstanceCore()
	: bSimulatePhysics(false)
	, bOverrideMass(false)
	, bEnableGravity(true)
	, bAutoWeld(false)
	, bStartAwake(true)
	, bGenerateWakeEvents(false)
	, bUpdateMassWhenScaleChanges(false)
{
}

bool FBodyInstanceCore::ShouldInstanceSimulatingPhysics() const
{
	return bSimulatePhysics && BodySetup != nullptr;
}
