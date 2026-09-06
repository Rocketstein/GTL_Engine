#pragma once
#include "Core/CoreTypes.h"
#include "Input/inputTrigger.h"
#include <functional>

class FInputManager;

struct FInputMappingContext;
struct FInputAction;

using FInputActionCallback = std::function<void(const FInputActionValue&)>;

// class : Enhanced Input Manager
// Fuction : handle mapping context and trigger check and callback execute
// Mapping context is container of mapping of action and key and trigger and modifier
class FEnhancedInputManager
{
public:

	void AddMappingContext(FInputMappingContext* Context, int32 Priority = 0);
	void RemoveMappingContext(FInputMappingContext* Context);
	void ClearAllMappingContexts();

	void BindAction(FInputAction* Action, ETriggerEvent TriggerEvent, FInputActionCallback Callback);
	void ClearBindings();

	void ProcessInput(FInputManager* RawInput, float DeltaTime, bool bIgnoreGui = false);
private:
	FInputActionValue GetRawActionValue(FInputManager* Input, int32 Key);
	struct FMappingContextEntry
	{
		FInputMappingContext* Context;
		int32 Priority;
	};

	struct FBindingEntry
	{
		FInputAction* Action;
		ETriggerEvent TriggerEvent;
		FInputActionCallback Callback;
	};
	TArray<FMappingContextEntry> MappingContexts;
	TArray<FBindingEntry> Bindings;
	TMap<FInputAction*, ETriggerState> ActionStates;
};