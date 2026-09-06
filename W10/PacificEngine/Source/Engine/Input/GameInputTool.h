#pragma once

#include "Input/GameViewportTool.h"
#include "Input/GameInput.h"

class FGameInputTool : public FGameViewportTool
{
public:
    using FGameViewportTool::FGameViewportTool;

    bool InputKey(const FViewportKeyEvent& Event) override
    {
        bool bPressed = (Event.Type == EKeyEventType::Pressed);
        bool bReleased = (Event.Type == EKeyEventType::Released);
        bool bDown = (Event.Type == EKeyEventType::Pressed || Event.Type == EKeyEventType::Repeat);
        
        if (Event.Type == EKeyEventType::Released) bDown = false;

        FGameInput::UpdateKeyState(Event.Key, bDown, bPressed, bReleased);
        return false; // Allow other tools to see it
    }

    bool InputPointer(const FViewportPointerEvent& Event) override
    {
        FGameInput::UpdatePointerState(Event.Button, Event.Type, Event.LocalPos);
        return false;
    }

    void BeginInputFrame() override
    {
        FGameInput::ResetFrameState();
    }
};
