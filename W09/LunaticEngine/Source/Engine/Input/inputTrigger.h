#pragma once
#include "Input/InputAction.h"
// Trigger State : None, Ongoing, Triggered
enum class ETriggerState : uint32
{
	None,
	Ongoing,
	Triggered,
};
// Trigger Event : Started, Ongoing, Triggered, Completed, Canceled
enum class ETriggerEvent : uint32
{
	None = 0,
	Started = 1 << 0,
	Ongoing = 1 << 1,
	Triggered = 1 << 2,
	Completed = 1 << 3,
	Canceled = 1 << 4,
};

// bit flag operator for ETriggerEvent
inline ETriggerEvent operator|(ETriggerEvent A, ETriggerEvent B)
{
	return static_cast<ETriggerEvent>(static_cast<uint32>(A) | static_cast<uint32>(B));
}
inline bool operator&(ETriggerEvent A, ETriggerEvent B)
{
	return (static_cast<uint32>(A) & static_cast<uint32>(B)) != 0;
}

class FInputTrigger
{
public:
	virtual ~FInputTrigger() = default;
	virtual ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) = 0;
	virtual void Reset() { LastState = ETriggerState::None; }

	ETriggerState LastState = ETriggerState::None;
};
class FTriggerDown : public FInputTrigger//눌린동안
{
public:

	// Fuction : Value is NonZero -> Triggered, else None
	ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) override
	{
		ETriggerState State = Value.IsNonZero() ? ETriggerState::Triggered : ETriggerState::None;
		LastState = State;
		return State;
	}
};

// 
class FTriggerPressed : public FInputTrigger// 눌린 1회
{
public:
	// Fuction : Value is NonZero and LastState is None -> Triggered Ongoing else None
	ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) override
	{
		ETriggerState State = ETriggerState::None;
		if (Value.IsNonZero() && LastState == ETriggerState::None)
			State = ETriggerState::Triggered;
		else if (Value.IsNonZero())
			State = ETriggerState::Ongoing;

		LastState = Value.IsNonZero() ? ETriggerState::Ongoing : ETriggerState::None;
		return State;
	}
};

class FInputReleased : public FInputTrigger //뗀 1회
{
	// Fuction : Value is Zero and LastState is not None -> Triggered else None
	ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) override
	{
		ETriggerState State = ETriggerState::None;
		if (!Value.IsNonZero() && LastState != ETriggerState::None)
			State = ETriggerState::Triggered;
		else if (Value.IsNonZero())
			State = ETriggerState::Ongoing;

		LastState = Value.IsNonZero() ? ETriggerState::Ongoing : ETriggerState::None;
		return State;
	}
};

class FInputHold : public FInputTrigger
{
public:
	// Fuction : Value is NonZero -> HoldDuration += DeltaTime 
	// if HoldDuration >= HoldTimeThresold -> Triggered else Ongoing
	// if Value is Zero -> Reset
	float HoldTimeThreshold = 0.5f;
	float HeldDuration = 0.0f;
	bool bTriggered = false;
	ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) override
	{
		if (!Value.IsNonZero())
		{
			Reset();
			return ETriggerState::None;
		}
		HeldDuration += DeltaTime;
		if (HeldDuration >= HoldTimeThreshold)
		{
			bTriggered = true;
			LastState = ETriggerState::Triggered;
			return ETriggerState::Triggered;
		}

		LastState = ETriggerState::Ongoing;
		return ETriggerState::Ongoing;
	}
	void Reset() override
	{
		FInputTrigger::Reset();
		HeldDuration = 0.0f;
		bTriggered = false;
	}
};

class FTriggerChordAction : public FInputTrigger
{
public:
	FInputAction* ChordAction = nullptr;

	FTriggerChordAction(FInputAction* InChordAction) : ChordAction(InChordAction) {}

	ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) override
	{
		//  This requires the ChordAction to be updated in the same frame.
		// In a simple system, we might just check FInputManager directly for the key.
		// , we check if the ChordAction's current state is Triggered.
		return ETriggerState::None; // Implementation depends on EnhancedInputManager access
	}
};

