#include "Navigation/NavigationData.h"

ANavigationData::ANavigationData()
{
	bNeedsTick = false;
}

bool ANavigationData::RebuildNavigationData()
{
	bNavigationDataBuilt = true;
	return true;
}

void ANavigationData::ClearNavigationData()
{
	bNavigationDataBuilt = false;
}

bool ANavigationData::ProjectPointToNavigation(const FVector&, FVector&, const FNavAgentProperties&) const
{
	return false;
}

bool ANavigationData::FindPath(const FVector&, const FVector&, const FNavAgentProperties&, FNavigationPath& OutPath) const
{
	OutPath.Reset();
	return false;
}

bool ANavigationData::NavigationRaycast(const FVector&, const FVector&, const FNavAgentProperties&) const
{
	return true;
}

bool ANavigationData::GetRandomReachablePointInRadius(const FVector&, float, FVector&, const FNavAgentProperties&) const
{
	return false;
}

void ANavigationData::DebugDrawNavigationData(float) const
{
}
