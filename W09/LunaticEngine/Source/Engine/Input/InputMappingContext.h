#pragma once
#include "Core/CoreTypes.h"

class FInputModifier;
class FInputTrigger;
struct FInputAction;
// Struct : ActionKey Mapping
// Fuction : Store Mapping information of action, key , Trigger, Modifier 
struct FActionKeyMapping
{
	FInputAction* Action = nullptr;
	int32 Key = 0;
	TArray<FInputTrigger*> Triggers;  
	TArray<FInputModifier*> Modifiers;
};
// Struct : Input Mapping Context
// Fuction : Store Mapping of Action and Key, Trigger, Modifier
struct FInputMappingContext
{
	FString ContextName;
	TArray<FActionKeyMapping> Mappings;

	FActionKeyMapping& AddMapping(FInputAction* Action, int32 Key);

};