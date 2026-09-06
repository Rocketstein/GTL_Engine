#include "Input/GameInput.h"
#include "Core/CoreTypes.h"

FGameInput::FState FGameInput::CurrentState = {};

bool FGameInput::GetKey(int vk)
{
    if (vk < 0 || vk >= 256) return false;
    return CurrentState.KeyDown[vk];
}

bool FGameInput::GetKeyDown(int vk)
{
    if (vk < 0 || vk >= 256) return false;
    return CurrentState.KeyPressed[vk];
}

bool FGameInput::GetKeyUp(int vk)
{
    if (vk < 0 || vk >= 256) return false;
    return CurrentState.KeyReleased[vk];
}

float FGameInput::GetAxis(const FString& axisName)
{
    if (axisName == "Horizontal")
    {
        float val = 0.0f;
        if (GetKey('D') || GetKey(VK_RIGHT)) val += 1.0f;
        if (GetKey('A') || GetKey(VK_LEFT)) val -= 1.0f;
        return val;
    }
    else if (axisName == "Vertical")
    {
        float val = 0.0f;
        if (GetKey('W') || GetKey(VK_UP)) val += 1.0f;
        if (GetKey('S') || GetKey(VK_DOWN)) val -= 1.0f;
        return val;
    }
    return 0.0f;
}

void FGameInput::UpdateKeyState(int vk, bool bDown, bool bPressed, bool bReleased)
{
    if (vk < 0 || vk >= 256) return;
    CurrentState.KeyDown[vk] = bDown;
    if (bPressed) CurrentState.KeyPressed[vk] = true;
    if (bReleased) CurrentState.KeyReleased[vk] = true;
}

bool FGameInput::GetMouseButton(int button)
{
    if (button < 0 || button >= 6) return false;
    return CurrentState.MouseButtonDown[button];
}

bool FGameInput::GetMouseButtonDown(int button)
{
    if (button < 0 || button >= 6) return false;
    return CurrentState.MouseButtonPressed[button];
}

bool FGameInput::GetMouseButtonUp(int button)
{
    if (button < 0 || button >= 6) return false;
    return CurrentState.MouseButtonReleased[button];
}

void FGameInput::GetMousePosition(int& outX, int& outY)
{
    outX = CurrentState.MousePosition.x;
    outY = CurrentState.MousePosition.y;
}

void FGameInput::UpdatePointerState(EPointerButton button, EPointerEventType type, POINT localPos)
{
    CurrentState.MousePosition = localPos;

    if (button == EPointerButton::None) return;

    int buttonIdx = (int)button;
    if (buttonIdx < 0 || buttonIdx >= 6) return;

    if (type == EPointerEventType::Pressed)
    {
        CurrentState.MouseButtonDown[buttonIdx] = true;
        CurrentState.MouseButtonPressed[buttonIdx] = true;
    }
    else if (type == EPointerEventType::Released)
    {
        CurrentState.MouseButtonDown[buttonIdx] = false;
        CurrentState.MouseButtonReleased[buttonIdx] = true;
    }
}

void FGameInput::ResetFrameState()
{
    for (int i = 0; i < 256; ++i)
    {
        CurrentState.KeyPressed[i] = false;
        CurrentState.KeyReleased[i] = false;
    }

    for (int i = 0; i < 6; ++i)
    {
        CurrentState.MouseButtonPressed[i] = false;
        CurrentState.MouseButtonReleased[i] = false;
    }
}
