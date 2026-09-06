#include "ParticleEvent.h"

namespace
{
	bool IsNoneEventName(FName Name)
	{
		return !Name.IsValid() || Name == FName::None || Name.ToString().empty();
	}
}

bool FParticleEventData::Matches(EParticleEventType InType, FName InEventName) const
{
	const bool bTypeMatches = InType == EParticleEventType::Any || Type == InType;
	const bool bNameMatches = IsNoneEventName(InEventName) || EventName == InEventName;
	return bTypeMatches && bNameMatches;
}
