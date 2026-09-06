#pragma once

#include "Core/Types/CoreTypes.h"

// Human-readable key name helpers for editor/LuaBlueprint/Lua bindings.
// Runtime input still stores virtual-key integers internally, but user-facing
// authoring should use stable names such as "Space", "W", "LeftMouseButton".
inline constexpr int32 INPUT_GAMEPAD_MAX_CONTROLLERS = 4;
inline constexpr int32 INPUT_GAMEPAD_KEY_BASE = 0x100;
inline constexpr int32 INPUT_GAMEPAD_BUTTON_COUNT = 16;

enum EInputGamepadKey : int32
{
	GamepadFaceButtonBottom = INPUT_GAMEPAD_KEY_BASE,
	GamepadFaceButtonRight,
	GamepadFaceButtonLeft,
	GamepadFaceButtonTop,
	GamepadLeftShoulder,
	GamepadRightShoulder,
	GamepadLeftTrigger,
	GamepadRightTrigger,
	GamepadLeftThumbstick,
	GamepadRightThumbstick,
	GamepadDPadUp,
	GamepadDPadDown,
	GamepadDPadLeft,
	GamepadDPadRight,
	GamepadStart,
	GamepadBack,
};

enum EInputGamepadAxis : int32
{
	GamepadAxisInvalid = -1,
	GamepadLeftX = 0,
	GamepadLeftY,
	GamepadRightX,
	GamepadRightY,
	GamepadLeftTriggerAxis,
	GamepadRightTriggerAxis,
};

bool IsGamepadKeyCode(int32 KeyCode);
int32 GetGamepadButtonIndex(int32 KeyCode);
int32 ResolveInputKeyCode(const FString& KeyName);
FString GetInputKeyName(int32 KeyCode);
const TArray<FString>& GetKnownInputKeyNames();
int32 ResolveInputAxisCode(const FString& AxisName);
FString GetInputAxisName(int32 AxisCode);
