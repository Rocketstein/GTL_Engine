#pragma once

#include "Core/Platform/PlatformTypes.h"
#include <cstdint>

class FWindow;

enum class EMouseButton : uint8
{
    Left = 0,
    Right,
    Middle,
    Count
};

class FInputState
{
public:
    FInputState();

    void BeginFrame();
    void Update(const FWindow& Window);

    bool IsKeyDown(int KeyCode) const;
    bool WasKeyPressed(int KeyCode) const;
    bool WasKeyReleased(int KeyCode) const;

    bool IsMouseDown(EMouseButton Button) const;
    bool WasMousePressed(EMouseButton Button) const;
    bool WasMouseReleased(EMouseButton Button) const;

    int GetMouseX() const;
    int GetMouseY() const;
    int GetMouseDeltaX() const;
    int GetMouseDeltaY() const;

    float GetWheelDelta() const;
    bool  HasWindowFocus() const;

    void SetWheelDelta(float InWheelDelta);
    void SetWindowFocus(bool bInFocused);

private:
    static int ToMouseIndex(EMouseButton Button);

private:
    bool CurrentKeys[256];
    bool PreviousKeys[256];

    bool CurrentMouseButtons[static_cast<int>(EMouseButton::Count)];
    bool PreviousMouseButtons[static_cast<int>(EMouseButton::Count)];

    int MouseX;
    int MouseY;
    int PrevMouseX;
    int PrevMouseY;

    float WheelDelta;
    bool  bWindowFocused;
};
