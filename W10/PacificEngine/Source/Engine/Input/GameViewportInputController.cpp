#include "Input/GameViewportInputController.h"
#include "Input/GameInputTool.h"
#include "Input/PIEExitCommandTool.h"
#include "Viewport/GameViewportClient.h"
#include "Engine/Runtime/Engine.h"

FGameViewportInputController::FGameViewportInputController(UGameViewportClient* InOwner)
    : Owner(InOwner)
{
#if WITH_EDITOR
    Tools.push_back(std::make_unique<FPIEExitCommandTool>(InOwner, this));  // ESC키로 PIE 종료- 툴
#endif
    Tools.push_back(std::make_unique<FGameInputTool>(InOwner, this));       // 실제 인게임 입력을 위한 툴
}

FGameViewportInputController::~FGameViewportInputController() = default;

bool FGameViewportInputController::InputKey(const FViewportKeyEvent& Event)
{
    if (Event.Key < 0 || Event.Key >= 256)
    {
        return false;
    }

    CurrentInput.Modifiers = Event.Modifiers;

    switch (Event.Type)
    {
    case EKeyEventType::Pressed:
        CurrentInput.KeyPressed[Event.Key] = true;
        CurrentInput.KeyDown[Event.Key] = true;
        break;

    case EKeyEventType::Released:
        CurrentInput.KeyReleased[Event.Key] = true;
        CurrentInput.KeyDown[Event.Key] = false;
        break;

    case EKeyEventType::Repeat:
        CurrentInput.KeyRepeated[Event.Key] = true;
        CurrentInput.KeyDown[Event.Key] = true;
        break;
    }

    // 도구들이 순서대로 입력을 소비함
    for (auto& Tool : Tools)
    {
        if (Tool->InputKey(Event))
        {
            return true;
        }
    }

    return true;
}

bool FGameViewportInputController::InputAxis(const FViewportAxisEvent& Event)
{
    CurrentInput.Modifiers = Event.Modifiers;

    switch (Event.Axis)
    {
    case EInputAxis::MouseX:
        CurrentInput.MouseAxisDelta.x += static_cast<LONG>(Event.Value);
        break;

    case EInputAxis::MouseY:
        CurrentInput.MouseAxisDelta.y += static_cast<LONG>(Event.Value);
        break;

    case EInputAxis::MouseWheel:
        CurrentInput.MouseWheelNotches += Event.Value;
        break;

    default:
        break;
    }

    for (auto& Tool : Tools)
    {
        if (Tool->InputAxis(Event))
        {
            return true;
        }
    }

    return false;
}

bool FGameViewportInputController::InputPointer(const FViewportPointerEvent& Event)
{
    CurrentInput.Modifiers = Event.Modifiers;

    CurrentInput.MouseLocalPos = Event.LocalPos;
    CurrentInput.MouseClientPos = Event.ClientPos;
    CurrentInput.MouseScreenPos = Event.ScreenPos;

    switch (Event.Button)
    {
    case EPointerButton::Left:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bLeftPressed = true;
            CurrentInput.bLeftDown = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bLeftReleased = true;
            CurrentInput.bLeftDown = false;
        }
        break;

    case EPointerButton::Right:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bRightPressed = true;
            CurrentInput.bRightDown = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bRightReleased = true;
            CurrentInput.bRightDown = false;
        }
        break;

    case EPointerButton::Middle:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bMiddlePressed = true;
            CurrentInput.bMiddleDown = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bMiddleReleased = true;
            CurrentInput.bMiddleDown = false;
        }
        break;

    case EPointerButton::X1:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bX1Pressed = true;
            CurrentInput.bX1Down = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bX1Released = true;
            CurrentInput.bX1Down = false;
        }
        break;

    case EPointerButton::X2:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bX2Pressed = true;
            CurrentInput.bX2Down = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bX2Released = true;
            CurrentInput.bX2Down = false;
        }
        break;

    case EPointerButton::None:
    default:
        break;
    }

    for (auto& Tool : Tools)
    {
        if (Tool->InputPointer(Event))
        {
            return true;
        }
    }

    return false;
}

void FGameViewportInputController::BeginInputFrame()
{
    for (int32 VK = 0; VK < 256; ++VK)
    {
        CurrentInput.KeyPressed[VK] = false;
        CurrentInput.KeyReleased[VK] = false;
        CurrentInput.KeyRepeated[VK] = false;
    }

    CurrentInput.MouseAxisDelta = { 0, 0 };
    CurrentInput.MouseWheelNotches = 0.0f;

    CurrentInput.bLeftPressed = false;
    CurrentInput.bLeftReleased = false;

    CurrentInput.bRightPressed = false;
    CurrentInput.bRightReleased = false;

    CurrentInput.bMiddlePressed = false;
    CurrentInput.bMiddleReleased = false;

    CurrentInput.bX1Pressed = false;
    CurrentInput.bX1Released = false;

    CurrentInput.bX2Pressed = false;
    CurrentInput.bX2Released = false;

    for (auto& Tool : Tools)
    {
        Tool->BeginInputFrame();
    }
}

void FGameViewportInputController::ResetInputState()
{
    for (int32 VK = 0; VK < 256; ++VK)
    {
        CurrentInput.KeyDown[VK] = false;
        CurrentInput.KeyPressed[VK] = false;
        CurrentInput.KeyReleased[VK] = false;
        CurrentInput.KeyRepeated[VK] = false;
    }

    CurrentInput.MouseAxisDelta = { 0, 0 };
    CurrentInput.MouseWheelNotches = 0.0f;

    CurrentInput.bLeftPressed = false;
    CurrentInput.bLeftDown = false;
    CurrentInput.bLeftReleased = false;

    CurrentInput.bRightPressed = false;
    CurrentInput.bRightDown = false;
    CurrentInput.bRightReleased = false;

    CurrentInput.bMiddlePressed = false;
    CurrentInput.bMiddleDown = false;
    CurrentInput.bMiddleReleased = false;

    CurrentInput.bX1Pressed = false;
    CurrentInput.bX1Down = false;
    CurrentInput.bX1Released = false;

    CurrentInput.bX2Pressed = false;
    CurrentInput.bX2Down = false;
    CurrentInput.bX2Released = false;

    CurrentInput.Modifiers = {};

    for (auto& Tool : Tools)
    {
        Tool->ResetState();
    }
}

void FGameViewportInputController::ResetKeyboardInputState()
{
    for (int32 VK = 0; VK < 256; ++VK)
    {
        CurrentInput.KeyDown[VK] = false;
        CurrentInput.KeyPressed[VK] = false;
        CurrentInput.KeyReleased[VK] = false;
        CurrentInput.KeyRepeated[VK] = false;
    }

    CurrentInput.Modifiers = {};
}

bool FGameViewportInputController::HandleInput(float DeltaTime)
{
    return false;
}
