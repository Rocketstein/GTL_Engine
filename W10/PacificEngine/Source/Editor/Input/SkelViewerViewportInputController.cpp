#include "SkelViewerViewportInputController.h"
#include "Input/PreviewViewportTool.h"
#include "Viewport/PreviewViewportClient.h"
#include "Viewport/SkeletalMeshViewer.h"

FSkelViewerViewportInputController::FSkelViewerViewportInputController(FSkeletalMeshViewer* InViewer, FPreviewViewportClient* InOwner)
{
    Owner = InOwner;
    OwnViewer = InViewer;

    Tools.emplace_back(std::make_unique<FSkeletalMeshCommandTool>(Owner, OwnViewer, this));
    Tools.emplace_back(std::make_unique<FSkeletalMeshGizmoTool>(Owner, OwnViewer, this));
    Tools.emplace_back(std::make_unique<FSkeletalMeshViewerBoneSelectTool>(Owner, OwnViewer, this));
    Tools.emplace_back(std::make_unique<FSkeletalMeshNavigationTool>(Owner, OwnViewer, this));
}

FSkelViewerViewportInputController::~FSkelViewerViewportInputController() = default;

bool FSkelViewerViewportInputController::InputKey(const FViewportKeyEvent& Event)
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

    return false;
}

bool FSkelViewerViewportInputController::InputAxis(const FViewportAxisEvent& Event)
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

    return false;
}

bool FSkelViewerViewportInputController::InputPointer(const FViewportPointerEvent& Event)
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

    return false;
}

void FSkelViewerViewportInputController::BeginInputFrame()
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
}

void FSkelViewerViewportInputController::ResetInputState()
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

    for (const std::unique_ptr<FPreviewViewportTool>& Tool : Tools)
    {
        if (Tool)
        {
            Tool->ResetState();
        }
    }
}

void FSkelViewerViewportInputController::ResetKeyboardInputState()
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

bool FSkelViewerViewportInputController::HandleInput(float DeltaTime)
{
    for (const std::unique_ptr<FPreviewViewportTool>& Tool : Tools)
    {
        if (Tool && Tool->HandleInput(DeltaTime))
        {
            return true;
        }
    }

    return false;
}
