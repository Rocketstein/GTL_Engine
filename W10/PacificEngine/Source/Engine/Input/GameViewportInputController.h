#pragma once

#include <memory>
#include <vector>

#include "Core/CoreTypes.h"
#include "Input/InputTypes.h"
#include "Input/GameViewportTool.h"

class UGameViewportClient;

/**
 * @brief FGameViewportFrameInput가 한 프레임 동안 받은 viewport 입력 event를 저장하는 구조체
 */
struct FGameViewportFrameInput
{
    // ... (rest of the struct remains same)
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

class FGameViewportInputController
{
public:
    explicit FGameViewportInputController(UGameViewportClient* InOwner);
    ~FGameViewportInputController();

    bool InputKey(const FViewportKeyEvent& Event);
    bool InputAxis(const FViewportAxisEvent& Event);
    bool InputPointer(const FViewportPointerEvent& Event);

    void BeginInputFrame();
    void ResetInputState();
    void ResetKeyboardInputState();

    const FGameViewportFrameInput& GetCurrentInput() const { return CurrentInput; }

    bool HandleInput(float DeltaTime);

private:
    UGameViewportClient* Owner = nullptr;
    FGameViewportFrameInput CurrentInput;
    
    TArray<std::unique_ptr<FGameViewportTool>> Tools;
};
