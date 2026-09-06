#include "InputState.h"
#include "Window.h"
#include <cstring>
#include <windows.h>


FInputState::FInputState()
    : MouseX(0), MouseY(0), PrevMouseX(0), PrevMouseY(0), WheelDelta(0.0f), bWindowFocused(true)
{
    std::memset(CurrentKeys, 0, sizeof(CurrentKeys));
    std::memset(PreviousKeys, 0, sizeof(PreviousKeys));
    std::memset(CurrentMouseButtons, 0, sizeof(CurrentMouseButtons));
    std::memset(PreviousMouseButtons, 0, sizeof(PreviousMouseButtons));
}

void FInputState::BeginFrame()
{
    std::memcpy(PreviousKeys, CurrentKeys, sizeof(CurrentKeys));
    std::memcpy(PreviousMouseButtons, CurrentMouseButtons, sizeof(CurrentMouseButtons));
    PrevMouseX = MouseX;
    PrevMouseY = MouseY;
    WheelDelta = 0.0f;
}

void FInputState::Update(const FWindow &Window)
{
    for (int KeyCode = 0; KeyCode < 256; ++KeyCode)
    {
        CurrentKeys[KeyCode] = (GetAsyncKeyState(KeyCode) & 0x8000) != 0;
    }

    CurrentMouseButtons[ToMouseIndex(EMouseButton::Left)] =
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    CurrentMouseButtons[ToMouseIndex(EMouseButton::Right)] =
        (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    CurrentMouseButtons[ToMouseIndex(EMouseButton::Middle)] =
        (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

    const POINT MousePosition = Window.GetMouseClientPosition();
    MouseX = MousePosition.x;
    MouseY = MousePosition.y;
}

bool FInputState::IsKeyDown(int KeyCode) const
{
    return (KeyCode >= 0 && KeyCode < 256) ? CurrentKeys[KeyCode] : false;
}

bool FInputState::WasKeyPressed(int KeyCode) const
{
    return (KeyCode >= 0 && KeyCode < 256) ? (CurrentKeys[KeyCode] && !PreviousKeys[KeyCode])
                                           : false;
}

bool FInputState::WasKeyReleased(int KeyCode) const
{
    return (KeyCode >= 0 && KeyCode < 256) ? (!CurrentKeys[KeyCode] && PreviousKeys[KeyCode])
                                           : false;
}

bool FInputState::IsMouseDown(EMouseButton Button) const
{
    const int Index = ToMouseIndex(Button);
    return CurrentMouseButtons[Index];
}

bool FInputState::WasMousePressed(EMouseButton Button) const
{
    const int Index = ToMouseIndex(Button);
    return CurrentMouseButtons[Index] && !PreviousMouseButtons[Index];
}

bool FInputState::WasMouseReleased(EMouseButton Button) const
{
    const int Index = ToMouseIndex(Button);
    return !CurrentMouseButtons[Index] && PreviousMouseButtons[Index];
}

int FInputState::GetMouseX() const { return MouseX; }

int FInputState::GetMouseY() const { return MouseY; }

int FInputState::GetMouseDeltaX() const { return MouseX - PrevMouseX; }

int FInputState::GetMouseDeltaY() const { return MouseY - PrevMouseY; }

float FInputState::GetWheelDelta() const { return WheelDelta; }

bool FInputState::HasWindowFocus() const { return bWindowFocused; }

void FInputState::SetWheelDelta(float InWheelDelta) { WheelDelta += InWheelDelta; }

void FInputState::SetWindowFocus(bool bInFocused) { bWindowFocused = bInFocused; }

int FInputState::ToMouseIndex(EMouseButton Button) { return static_cast<int>(Button); }
