#include "Input/EnhancedInputManager.h"
#include "Input/InputModifier.h"
#include "Input/InputManager.h"
#include "Input/InputAction.h"
#include "Input/InputMappingContext.h"
#include "ImGui/imgui.h"

#include <algorithm>

// Function : Add mapping context to manager and sort by priority
// input : Priority, Context 
// Context : mapping context to add
// Priority : if there are multiple mapping context
// context with higher priority will be processed first
void FEnhancedInputManager::AddMappingContext(FInputMappingContext* Context, int32 Priority)
{
	MappingContexts.push_back({ Context, Priority });
	std::sort(MappingContexts.begin(), MappingContexts.end(),
		[](const FMappingContextEntry& A, const FMappingContextEntry& B)
	{
		return A.Priority > B.Priority;
	});
}

// Function : Remove mapping context from manager 
// input : Context
// context : mapping context to remove
void FEnhancedInputManager::RemoveMappingContext(FInputMappingContext* Context)
{
	MappingContexts.erase(
		std::remove_if(MappingContexts.begin(), MappingContexts.end(),
			[Context](const FMappingContextEntry& E) { return E.Context == Context; }),
		MappingContexts.end());
}

// Function : Clear all mapping context from manager
void FEnhancedInputManager::ClearAllMappingContexts()
{
	MappingContexts.clear();
}

// Function : Bind action to manager with trigger event and callback
// input : Action , TriggerEvent, callback
// Action : action to bind
// TriggerEvent : event to trigger callback
// callback : function to call when trigger event occurs
void FEnhancedInputManager::BindAction(FInputAction* Action, ETriggerEvent TriggerEvent, FInputActionCallback Callback)
{
	Bindings.push_back({ Action, TriggerEvent, std::move(Callback) });
}

// Function : Clear all bindings from manager
void FEnhancedInputManager::ClearBindings()
{
	Bindings.clear();
}

// Function : Get raw action value from input manager
FInputActionValue FEnhancedInputManager::GetRawActionValue(FInputManager* Input, int32 Key)
{
	if (Key == static_cast<int32>(EInputKey::MouseX))
		return FInputActionValue(Input->GetMouseDeltaX());
	if (Key == static_cast<int32>(EInputKey::MouseY))
		return FInputActionValue(Input->GetMouseDeltaY());
	if (Key == static_cast<int32>(EInputKey::MouseWheel))
		return FInputActionValue(Input->GetMouseWheelDelta());

	if (Key == static_cast<int32>(EInputKey::MouseDragL_X))
		return FInputActionValue(static_cast<float>(Input->GetDragDelta(FInputManager::MOUSE_LEFT).x));
	if (Key == static_cast<int32>(EInputKey::MouseDragL_Y))
		return FInputActionValue(static_cast<float>(Input->GetDragDelta(FInputManager::MOUSE_LEFT).y));
	if (Key == static_cast<int32>(EInputKey::MouseDragR_X))
		return FInputActionValue(static_cast<float>(Input->GetDragDelta(FInputManager::MOUSE_RIGHT).x));
	if (Key == static_cast<int32>(EInputKey::MouseDragR_Y))
		return FInputActionValue(static_cast<float>(Input->GetDragDelta(FInputManager::MOUSE_RIGHT).y));
	if (Key == static_cast<int32>(EInputKey::MouseDragM_X))
		return FInputActionValue(static_cast<float>(Input->GetDragDelta(FInputManager::MOUSE_MIDDLE).x));
	if (Key == static_cast<int32>(EInputKey::MouseDragM_Y))
		return FInputActionValue(static_cast<float>(Input->GetDragDelta(FInputManager::MOUSE_MIDDLE).y));

	if (Key >= 0 && Key < 256)
		return FInputActionValue(Input->IsKeyDown(Key) ? 1.0f : 0.0f);
	
	return FInputActionValue(0.0f);
}

// Function : Process input from raw input manager 
// and update action state and trigger events
// input : raw input , delta time
// raw input : input manager to get raw input from
// delta time : time since last frame,
// used for trigger state update
void FEnhancedInputManager::ProcessInput(FInputManager* RawInput, float DeltaTime, bool bIgnoreGui)
{
	bool bGuiWantsKeyboard = !bIgnoreGui && RawInput->IsGuiUsingKeyboard();
	bool bGuiWantsMouse = !bIgnoreGui && RawInput->IsGuiUsingMouse();

	TMap<FInputAction*, FInputActionValue> ActionValues;
	TMap<FInputAction*, ETriggerState> NewTriggerStates;
	
	for (const FMappingContextEntry& ContextEntry : MappingContexts)
	{
		if (!ContextEntry.Context) continue;

		for (FActionKeyMapping& Mapping : ContextEntry.Context->Mappings)
		{
			if (!Mapping.Action) continue;

			// ImGui blocking
			bool bIsMouseKey = (Mapping.Key >= static_cast<int32>(EInputKey::MouseX) && Mapping.Key <= static_cast<int32>(EInputKey::MouseDragM_Y))
				|| (Mapping.Key == VK_LBUTTON || Mapping.Key == VK_RBUTTON || Mapping.Key == VK_MBUTTON || Mapping.Key == VK_XBUTTON1 || Mapping.Key == VK_XBUTTON2);
			
			if (bIsMouseKey && bGuiWantsMouse) continue;
			if (!bIsMouseKey && bGuiWantsKeyboard) continue;

			FInputActionValue Value = GetRawActionValue(RawInput, Mapping.Key);

			for (auto& Modifier : Mapping.Modifiers)
			{
				Value = Modifier->ModifyRaw(Value);
			}

			ETriggerState MappingTriggerState = ETriggerState::None;
			if (Mapping.Triggers.empty())
			{
				MappingTriggerState = Value.IsNonZero() ? ETriggerState::Triggered : ETriggerState::None;
			}
			else
			{
				bool bAllTriggered = true;
				bool bAnyOngoing = false;
				for (auto& Trigger : Mapping.Triggers)
				{
					ETriggerState State = Trigger->UpdateState(Value, DeltaTime);
					if (State != ETriggerState::Triggered) bAllTriggered = false;
					if (State == ETriggerState::Ongoing) bAnyOngoing = true;
				}

				if (bAllTriggered) MappingTriggerState = ETriggerState::Triggered;
				else if (bAnyOngoing) MappingTriggerState = ETriggerState::Ongoing;
			}

			if (ActionValues.find(Mapping.Action) == ActionValues.end())
				ActionValues[Mapping.Action] = FInputActionValue();

			ActionValues[Mapping.Action] = ActionValues[Mapping.Action] + Value;

			ETriggerState& CurrentBest = NewTriggerStates[Mapping.Action];
			if (static_cast<uint32>(MappingTriggerState) > static_cast<uint32>(CurrentBest))
				CurrentBest = MappingTriggerState;
		}
	}

	// State Transitions -> ETriggerEvent -> Callbacks
	for (auto& [Action, NewState] : NewTriggerStates)
	{
		ETriggerState PrevState = ActionStates[Action];
		ETriggerEvent Event = ETriggerEvent::None;

		if (NewState == ETriggerState::Triggered)
		{
			Event = (PrevState == ETriggerState::None) ? (ETriggerEvent::Started | ETriggerEvent::Triggered) : ETriggerEvent::Triggered;
		}
		else if (NewState == ETriggerState::Ongoing)
		{
			Event = (PrevState == ETriggerState::None) ? (ETriggerEvent::Started | ETriggerEvent::Ongoing) : ETriggerEvent::Ongoing;
		}
		else // None
		{
			if (PrevState == ETriggerState::Triggered) Event = ETriggerEvent::Completed;
			else if (PrevState == ETriggerState::Ongoing) Event = ETriggerEvent::Canceled;
		}

		ActionStates[Action] = NewState;

		if (Event != ETriggerEvent::None)
		{
			FInputActionValue& ActionValue = ActionValues[Action];
			for (const FBindingEntry& Binding : Bindings)
			{
				if (Binding.Action == Action && (Event & Binding.TriggerEvent))
				{
					Binding.Callback(ActionValue);
				}
			}
		}
	}

	// Clean up actions not mapped this frame
	for (auto It = ActionStates.begin(); It != ActionStates.end();)
	{
		if (NewTriggerStates.find(It->first) == NewTriggerStates.end())
		{
			if (It->second != ETriggerState::None)
			{
				ETriggerEvent Event = (It->second == ETriggerState::Triggered) ? ETriggerEvent::Completed : ETriggerEvent::Canceled;
				FInputActionValue ZeroValue;
				for (const FBindingEntry& Binding : Bindings)
				{
					if (Binding.Action == It->first && (Event & Binding.TriggerEvent))
					{
						Binding.Callback(ZeroValue);
					}
				}
			}
			It = ActionStates.erase(It);
		}
		else
		{
			++It;
		}
	}
}
