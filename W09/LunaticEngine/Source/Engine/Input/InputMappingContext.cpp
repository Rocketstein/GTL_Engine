#include "Input/InputMappingContext.h"
#include "Input/InputAction.h"
// AddMapping : Add Mapping of Action and Key to Mapping Context
FActionKeyMapping& FInputMappingContext::AddMapping(FInputAction* Action, int32 Key)
{
	Mappings.push_back({});
	FActionKeyMapping& Mapping = Mappings.back();
	Mapping.Action = Action;
	Mapping.Key = Key;
	return Mapping;
}
