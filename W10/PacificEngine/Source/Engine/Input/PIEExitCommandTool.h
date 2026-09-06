#pragma once

#include "Input/GameViewportTool.h"
#include "Engine/Runtime/Engine.h"

class FPIEExitCommandTool : public FGameViewportTool
{
public:
    using FGameViewportTool::FGameViewportTool;

    bool InputKey(const FViewportKeyEvent& Event) override
    {
        if (Event.Type == EKeyEventType::Pressed && Event.Key == VK_ESCAPE)
        {
            if (GEngine)
            {
                GEngine->RequestEndPlayMap();
                return true; // Consume ESC
            }
        }
        return false;
    }
};
