#include "Distributions/Distribution.h"

#include "Serialization/Archive.h"

void UDistribution::Serialize(FArchive& Ar)
{
	SerializeProperties(Ar, PF_Save);
}

void UDistribution::GetTimeRange(float& OutMinTime, float& OutMaxTime) const
{
	OutMinTime = 0.0f;
	OutMaxTime = 1.0f;
}
