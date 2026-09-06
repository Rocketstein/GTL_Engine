#pragma once
#include "Input/InputTypes.h"
#include <memory>

class FSkeletalMeshViewer;
class FPreviewViewportClient;
class FPreviewViewportTool;

struct FViewportFrameInput
{
    bool KeyDown[256] = {};
    bool KeyPressed[256] = {};
    bool KeyReleased[256] = {};
    bool KeyRepeated[256] = {};

    POINT MouseLocalPos = { 0, 0 };
    POINT MouseClientPos = { 0, 0 };
    POINT MouseScreenPos = { 0, 0 };

    POINT MouseAxisDelta = { 0, 0 };
    float MouseWheelNotches = 0.0f;

    bool bLeftPressed = false;
    bool bLeftDown = false;
    bool bLeftReleased = false;

    bool bRightPressed = false;
    bool bRightDown = false;
    bool bRightReleased = false;

    bool bMiddlePressed = false;
    bool bMiddleDown = false;
    bool bMiddleReleased = false;

    bool bX1Pressed = false;
    bool bX1Down = false;
    bool bX1Released = false;

    bool bX2Pressed = false;
    bool bX2Down = false;
    bool bX2Released = false;

    FInputModifiers Modifiers;
};

class FSkelViewerViewportInputController
{
public:
    explicit FSkelViewerViewportInputController(FSkeletalMeshViewer* InViewer, FPreviewViewportClient* InOwner);

	~FSkelViewerViewportInputController();

    bool InputKey(const FViewportKeyEvent& Event);
    bool InputAxis(const FViewportAxisEvent& Event);
    bool InputPointer(const FViewportPointerEvent& Event);

    void BeginInputFrame();
    void ResetInputState();
    void ResetKeyboardInputState();

    const FViewportFrameInput& GetCurrentInput() const { return CurrentInput; }

    bool HandleInput(float DeltaTime);

private:
    FPreviewViewportClient* Owner = nullptr;
    FSkeletalMeshViewer* OwnViewer = nullptr;
    FViewportFrameInput CurrentInput;

    TArray<std::unique_ptr<FPreviewViewportTool>> Tools;
};
