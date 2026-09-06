#include "Input/InputKeyCodes.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace
{
    struct FInputKeyNameEntry
    {
        const char* Name;
        int32       Code;
    };

    struct FInputAxisNameEntry
    {
        const char* Name;
        int32       Code;
    };

    FString NormalizeKeyName(FString Value)
    {
        Value.erase(std::remove_if(Value.begin(), Value.end(), [](unsigned char C)
        {
            return C == ' ' || C == '_' || C == '-';
        }), Value.end());
        std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char C)
        {
            return static_cast<char>(std::toupper(C));
        });
        return Value;
    }

    const TArray<FInputKeyNameEntry>& GetInputKeyNameEntries()
    {
        static const TArray<FInputKeyNameEntry> Entries = {
            {"None", 0},
            {"LeftMouseButton", 0x01},
            {"RightMouseButton", 0x02},
            {"MiddleMouseButton", 0x04},
            {"MouseXButton1", 0x05},
            {"MouseXButton2", 0x06},
            {"Backspace", 0x08},
            {"Tab", 0x09},
            {"Enter", 0x0D},
            {"Shift", 0x10},
            {"Ctrl", 0x11},
            {"Control", 0x11},
            {"Alt", 0x12},
            {"Pause", 0x13},
            {"CapsLock", 0x14},
            {"Escape", 0x1B},
            {"Space", 0x20},
            {"PageUp", 0x21},
            {"PageDown", 0x22},
            {"End", 0x23},
            {"Home", 0x24},
            {"Left", 0x25},
            {"Up", 0x26},
            {"Right", 0x27},
            {"Down", 0x28},
            {"Insert", 0x2D},
            {"Delete", 0x2E},
            {"0", 0x30}, {"1", 0x31}, {"2", 0x32}, {"3", 0x33}, {"4", 0x34},
            {"5", 0x35}, {"6", 0x36}, {"7", 0x37}, {"8", 0x38}, {"9", 0x39},
            {"A", 0x41}, {"B", 0x42}, {"C", 0x43}, {"D", 0x44}, {"E", 0x45},
            {"F", 0x46}, {"G", 0x47}, {"H", 0x48}, {"I", 0x49}, {"J", 0x4A},
            {"K", 0x4B}, {"L", 0x4C}, {"M", 0x4D}, {"N", 0x4E}, {"O", 0x4F},
            {"P", 0x50}, {"Q", 0x51}, {"R", 0x52}, {"S", 0x53}, {"T", 0x54},
            {"U", 0x55}, {"V", 0x56}, {"W", 0x57}, {"X", 0x58}, {"Y", 0x59}, {"Z", 0x5A},
            {"Numpad0", 0x60}, {"Numpad1", 0x61}, {"Numpad2", 0x62}, {"Numpad3", 0x63}, {"Numpad4", 0x64},
            {"Numpad5", 0x65}, {"Numpad6", 0x66}, {"Numpad7", 0x67}, {"Numpad8", 0x68}, {"Numpad9", 0x69},
            {"Multiply", 0x6A}, {"Add", 0x6B}, {"Subtract", 0x6D}, {"Decimal", 0x6E}, {"Divide", 0x6F},
            {"F1", 0x70}, {"F2", 0x71}, {"F3", 0x72}, {"F4", 0x73}, {"F5", 0x74}, {"F6", 0x75},
            {"F7", 0x76}, {"F8", 0x77}, {"F9", 0x78}, {"F10", 0x79}, {"F11", 0x7A}, {"F12", 0x7B},
            {"LeftShift", 0xA0}, {"RightShift", 0xA1},
            {"LeftCtrl", 0xA2}, {"RightCtrl", 0xA3},
            {"LeftAlt", 0xA4}, {"RightAlt", 0xA5},
            {"Semicolon", 0xBA}, {"Equals", 0xBB}, {"Comma", 0xBC}, {"Minus", 0xBD}, {"Period", 0xBE}, {"Slash", 0xBF},
            {"BackQuote", 0xC0}, {"LeftBracket", 0xDB}, {"Backslash", 0xDC}, {"RightBracket", 0xDD}, {"Apostrophe", 0xDE},
            // Aliases commonly used by designers.
            {"LMB", 0x01}, {"RMB", 0x02}, {"MMB", 0x04},
            {"MouseLeft", 0x01}, {"MouseRight", 0x02}, {"MouseMiddle", 0x04},
            {"Return", 0x0D}, {"Esc", 0x1B}, {"PgUp", 0x21}, {"PgDn", 0x22},
            {"LeftControl", 0xA2}, {"RightControl", 0xA3},
            {"GamepadFaceButtonBottom", GamepadFaceButtonBottom},
            {"GamepadFaceButtonRight", GamepadFaceButtonRight},
            {"GamepadFaceButtonLeft", GamepadFaceButtonLeft},
            {"GamepadFaceButtonTop", GamepadFaceButtonTop},
            {"GamepadLeftShoulder", GamepadLeftShoulder},
            {"GamepadRightShoulder", GamepadRightShoulder},
            {"GamepadLeftTrigger", GamepadLeftTrigger},
            {"GamepadRightTrigger", GamepadRightTrigger},
            {"GamepadLeftThumbstick", GamepadLeftThumbstick},
            {"GamepadRightThumbstick", GamepadRightThumbstick},
            {"GamepadDPadUp", GamepadDPadUp},
            {"GamepadDPadDown", GamepadDPadDown},
            {"GamepadDPadLeft", GamepadDPadLeft},
            {"GamepadDPadRight", GamepadDPadRight},
            {"GamepadStart", GamepadStart},
            {"GamepadBack", GamepadBack},
            {"GamepadA", GamepadFaceButtonBottom},
            {"GamepadB", GamepadFaceButtonRight},
            {"GamepadX", GamepadFaceButtonLeft},
            {"GamepadY", GamepadFaceButtonTop},
            {"GamepadLB", GamepadLeftShoulder},
            {"GamepadRB", GamepadRightShoulder},
            {"GamepadLT", GamepadLeftTrigger},
            {"GamepadRT", GamepadRightTrigger},
            {"GamepadL3", GamepadLeftThumbstick},
            {"GamepadR3", GamepadRightThumbstick}
        };
        return Entries;
    }

    const TArray<FInputAxisNameEntry>& GetInputAxisNameEntries()
    {
        static const TArray<FInputAxisNameEntry> Entries = {
            {"GamepadLeftX", GamepadLeftX},
            {"GamepadLeftY", GamepadLeftY},
            {"GamepadRightX", GamepadRightX},
            {"GamepadRightY", GamepadRightY},
            {"GamepadLeftTriggerAxis", GamepadLeftTriggerAxis},
            {"GamepadRightTriggerAxis", GamepadRightTriggerAxis},
            {"LeftStickX", GamepadLeftX},
            {"LeftStickY", GamepadLeftY},
            {"RightStickX", GamepadRightX},
            {"RightStickY", GamepadRightY},
            {"LeftTriggerAxis", GamepadLeftTriggerAxis},
            {"RightTriggerAxis", GamepadRightTriggerAxis},
        };
        return Entries;
    }
}

bool IsGamepadKeyCode(int32 KeyCode)
{
    return KeyCode >= INPUT_GAMEPAD_KEY_BASE && KeyCode < INPUT_GAMEPAD_KEY_BASE + INPUT_GAMEPAD_BUTTON_COUNT;
}

int32 GetGamepadButtonIndex(int32 KeyCode)
{
    return IsGamepadKeyCode(KeyCode) ? KeyCode - INPUT_GAMEPAD_KEY_BASE : -1;
}

int32 ResolveInputKeyCode(const FString& KeyName)
{
    const FString Normalized = NormalizeKeyName(KeyName);
    if (Normalized.empty() || Normalized == "NONE" || Normalized == "EXISTING" || Normalized == "EXISTINGMAPPING")
    {
        return 0;
    }

    for (const FInputKeyNameEntry& Entry : GetInputKeyNameEntries())
    {
        if (NormalizeKeyName(Entry.Name) == Normalized)
        {
            return Entry.Code;
        }
    }

    // Backward-compatible escape hatch: numeric strings still parse, but editor UI no longer asks for them.
    const char* NumericText = KeyName.c_str();
    if (Normalized.rfind("VK", 0) == 0 && Normalized.size() > 2)
    {
        NumericText = KeyName.c_str() + 2;
    }

    char* End = nullptr;
    const long Parsed = std::strtol(NumericText, &End, 0);
    if (End && *End == '\0' && Parsed >= 0 && Parsed <= 255)
    {
        return static_cast<int32>(Parsed);
    }

    return 0;
}

FString GetInputKeyName(int32 KeyCode)
{
    if (KeyCode == 0)
    {
        return "None";
    }

    // Prefer the canonical first entry for each code, skip aliases later in the table.
    for (const FInputKeyNameEntry& Entry : GetInputKeyNameEntries())
    {
        if (Entry.Code == KeyCode)
        {
            return Entry.Name;
        }
    }

    return FString("VK") + std::to_string(KeyCode);
}

int32 ResolveInputAxisCode(const FString& AxisName)
{
    const FString Normalized = NormalizeKeyName(AxisName);
    if (Normalized.empty() || Normalized == "NONE")
    {
        return GamepadAxisInvalid;
    }

    for (const FInputAxisNameEntry& Entry : GetInputAxisNameEntries())
    {
        if (NormalizeKeyName(Entry.Name) == Normalized)
        {
            return Entry.Code;
        }
    }

    char* End = nullptr;
    const long Parsed = std::strtol(AxisName.c_str(), &End, 0);
    if (End && *End == '\0' && Parsed >= GamepadLeftX && Parsed <= GamepadRightTriggerAxis)
    {
        return static_cast<int32>(Parsed);
    }

    return GamepadAxisInvalid;
}

FString GetInputAxisName(int32 AxisCode)
{
    for (const FInputAxisNameEntry& Entry : GetInputAxisNameEntries())
    {
        if (Entry.Code == AxisCode)
        {
            return Entry.Name;
        }
    }

    return "None";
}

const TArray<FString>& GetKnownInputKeyNames()
{
    static TArray<FString> Names;
    if (Names.empty())
    {
        TSet<int32> AddedCodes;
        for (const FInputKeyNameEntry& Entry : GetInputKeyNameEntries())
        {
            if (Entry.Code == 0 || AddedCodes.find(Entry.Code) != AddedCodes.end())
            {
                continue;
            }
            AddedCodes.insert(Entry.Code);
            Names.push_back(Entry.Name);
        }
    }
    return Names;
}
