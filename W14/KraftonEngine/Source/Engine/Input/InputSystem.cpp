#include "Engine/Input/InputSystem.h"
#include <Xinput.h>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float GAMEPAD_STICK_DEADZONE = 0.20f;
    constexpr float GAMEPAD_TRIGGER_BUTTON_THRESHOLD = 0.50f;

    bool IsValidControllerIndex(int ControllerIndex)
    {
        return ControllerIndex >= 0 && ControllerIndex < INPUT_GAMEPAD_MAX_CONTROLLERS;
    }

    float NormalizeStickAxis(SHORT Value)
    {
        const float Denom = Value < 0 ? 32768.0f : 32767.0f;
        float Normalized = static_cast<float>(Value) / Denom;
        Normalized = std::clamp(Normalized, -1.0f, 1.0f);

        const float AbsValue = std::abs(Normalized);
        if (AbsValue <= GAMEPAD_STICK_DEADZONE)
        {
            return 0.0f;
        }

        const float Scaled = (AbsValue - GAMEPAD_STICK_DEADZONE) / (1.0f - GAMEPAD_STICK_DEADZONE);
        return Normalized < 0.0f ? -Scaled : Scaled;
    }

    float NormalizeTrigger(BYTE Value)
    {
        return std::clamp(static_cast<float>(Value) / 255.0f, 0.0f, 1.0f);
    }

    WORD NormalizeRumbleMotor(float Value)
    {
        const float Clamped = std::clamp(Value, 0.0f, 1.0f);
        return static_cast<WORD>(Clamped * 65535.0f);
    }
}

bool FInputSystemSnapshot::IsDown(int KeyCode, int ControllerIndex) const
{
    if (KeyCode >= 0 && KeyCode < 256)
    {
        return KeyDown[KeyCode];
    }

    const int32 ButtonIndex = GetGamepadButtonIndex(KeyCode);
    if (!IsValidControllerIndex(ControllerIndex) || ButtonIndex < 0)
    {
        return false;
    }

    return Gamepads[ControllerIndex].ButtonDown[ButtonIndex];
}

bool FInputSystemSnapshot::WasPressed(int KeyCode, int ControllerIndex) const
{
    if (KeyCode >= 0 && KeyCode < 256)
    {
        return KeyPressed[KeyCode];
    }

    const int32 ButtonIndex = GetGamepadButtonIndex(KeyCode);
    if (!IsValidControllerIndex(ControllerIndex) || ButtonIndex < 0)
    {
        return false;
    }

    return Gamepads[ControllerIndex].ButtonPressed[ButtonIndex];
}

bool FInputSystemSnapshot::WasReleased(int KeyCode, int ControllerIndex) const
{
    if (KeyCode >= 0 && KeyCode < 256)
    {
        return KeyReleased[KeyCode];
    }

    const int32 ButtonIndex = GetGamepadButtonIndex(KeyCode);
    if (!IsValidControllerIndex(ControllerIndex) || ButtonIndex < 0)
    {
        return false;
    }

    return Gamepads[ControllerIndex].ButtonReleased[ButtonIndex];
}

bool FInputSystemSnapshot::IsGamepadConnected(int ControllerIndex) const
{
    return IsValidControllerIndex(ControllerIndex) && Gamepads[ControllerIndex].bConnected;
}

float FInputSystemSnapshot::GetAxis(int AxisCode, int ControllerIndex) const
{
    if (!IsValidControllerIndex(ControllerIndex))
    {
        return 0.0f;
    }

    const FGamepadState& Pad = Gamepads[ControllerIndex];
    switch (AxisCode)
    {
    case GamepadLeftX: return Pad.LeftX;
    case GamepadLeftY: return Pad.LeftY;
    case GamepadRightX: return Pad.RightX;
    case GamepadRightY: return Pad.RightY;
    case GamepadLeftTriggerAxis: return Pad.LeftTrigger;
    case GamepadRightTriggerAxis: return Pad.RightTrigger;
    default: return 0.0f;
    }
}

void InputSystem::Tick()
{
    // 윈도우 포커스가 없으면 모든 입력 상태 해제
    bWindowFocused = !OwnerHWnd || GetForegroundWindow() == OwnerHWnd;
    if (!bWindowFocused)
    {
        StopAllGamepadRumble();
        ResetAllKeyStates();
        ResetTransientState();
        UpdateCurrentSnapshot();
        return;
    }

    for (int i = 0; i < 256; ++i)
    {
        PrevStates[i] = CurrentStates[i];
        CurrentStates[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }
    PollGamepads();
    UpdateGamepadRumble();

    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;

    PrevScrollDelta = ScrollDelta;
    ScrollDelta = 0;

    PrevMousePos = MousePos;
    GetCursorPos(&MousePos);
    FrameMouseDeltaX = MousePos.x - PrevMousePos.x;
    FrameMouseDeltaY = MousePos.y - PrevMousePos.y;
    if (bUseRawMouse)
    {
        FrameMouseDeltaX = RawMouseDeltaAccumX;
        FrameMouseDeltaY = RawMouseDeltaAccumY;
    }
    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;

    if (GetKeyDown(VK_LBUTTON))
    {
        bLeftDragCandidate = true;
        LeftMouseDownPos = MousePos;
    }
    if (GetKeyDown(VK_RBUTTON))
    {
        bRightDragCandidate = true;
        RightMouseDownPos = MousePos;
    }

    // Left drag
    if (!bLeftDragging && IsDraggingLeft())
    {
        FilterDragThreshold(bLeftDragCandidate, bLeftDragging, bLeftDragJustStarted,
            LeftMouseDownPos, LeftDragStartPos);
    }
    else if (GetKeyUp(VK_LBUTTON))
    {
        if (bLeftDragging) bLeftDragJustEnded = true;
        bLeftDragging = false;
        bLeftDragCandidate = false;
    }

    // Right drag
    if (!bRightDragging && IsDraggingRight())
    {
        FilterDragThreshold(bRightDragCandidate, bRightDragging, bRightDragJustStarted,
            RightMouseDownPos, RightDragStartPos);
    }
    else if (GetKeyUp(VK_RBUTTON))
    {
        if (bRightDragging) bRightDragJustEnded = true;
        bRightDragging = false;
        bRightDragCandidate = false;
    }

    UpdateCurrentSnapshot();
}

bool InputSystem::GetKeyDown(int KeyCode, int ControllerIndex) const
{
    if (KeyCode >= 0 && KeyCode < 256)
    {
        return CurrentStates[KeyCode] && !PrevStates[KeyCode];
    }

    const int32 ButtonIndex = GetGamepadButtonIndex(KeyCode);
    if (!IsValidControllerIndex(ControllerIndex) || ButtonIndex < 0)
    {
        return false;
    }

    return CurrentGamepadButtons[ControllerIndex][ButtonIndex] && !PrevGamepadButtons[ControllerIndex][ButtonIndex];
}

bool InputSystem::GetKey(int KeyCode, int ControllerIndex) const
{
    if (KeyCode >= 0 && KeyCode < 256)
    {
        return CurrentStates[KeyCode];
    }

    const int32 ButtonIndex = GetGamepadButtonIndex(KeyCode);
    if (!IsValidControllerIndex(ControllerIndex) || ButtonIndex < 0)
    {
        return false;
    }

    return CurrentGamepadButtons[ControllerIndex][ButtonIndex];
}

bool InputSystem::GetKeyUp(int KeyCode, int ControllerIndex) const
{
    if (KeyCode >= 0 && KeyCode < 256)
    {
        return !CurrentStates[KeyCode] && PrevStates[KeyCode];
    }

    const int32 ButtonIndex = GetGamepadButtonIndex(KeyCode);
    if (!IsValidControllerIndex(ControllerIndex) || ButtonIndex < 0)
    {
        return false;
    }

    return !CurrentGamepadButtons[ControllerIndex][ButtonIndex] && PrevGamepadButtons[ControllerIndex][ButtonIndex];
}

bool InputSystem::IsGamepadConnected(int ControllerIndex) const
{
    return IsValidControllerIndex(ControllerIndex) && CurrentGamepadStates[ControllerIndex].bConnected;
}

float InputSystem::GetAxis(int AxisCode, int ControllerIndex) const
{
    if (!IsValidControllerIndex(ControllerIndex))
    {
        return 0.0f;
    }

    const FInputSystemSnapshot::FGamepadState& Pad = CurrentGamepadStates[ControllerIndex];
    switch (AxisCode)
    {
    case GamepadLeftX: return Pad.LeftX;
    case GamepadLeftY: return Pad.LeftY;
    case GamepadRightX: return Pad.RightX;
    case GamepadRightY: return Pad.RightY;
    case GamepadLeftTriggerAxis: return Pad.LeftTrigger;
    case GamepadRightTriggerAxis: return Pad.RightTrigger;
    default: return 0.0f;
    }
}

void InputSystem::PlayGamepadRumble(float LeftMotor, float RightMotor, float DurationSeconds, int ControllerIndex)
{
    if (!IsValidControllerIndex(ControllerIndex) || DurationSeconds <= 0.0f)
    {
        return;
    }

    FGamepadRumbleState& Rumble = GamepadRumbleStates[ControllerIndex];
    Rumble.LeftMotor = NormalizeRumbleMotor(LeftMotor);
    Rumble.RightMotor = NormalizeRumbleMotor(RightMotor);
    Rumble.EndTimeMs = GetTickCount64() + static_cast<ULONGLONG>(DurationSeconds * 1000.0f);
    Rumble.bActive = (Rumble.LeftMotor > 0 || Rumble.RightMotor > 0);

    if (Rumble.bActive)
    {
        ApplyGamepadRumble(ControllerIndex, Rumble.LeftMotor, Rumble.RightMotor);
    }
}

void InputSystem::StopGamepadRumble(int ControllerIndex)
{
    if (!IsValidControllerIndex(ControllerIndex))
    {
        return;
    }

    GamepadRumbleStates[ControllerIndex] = FGamepadRumbleState{};
    ApplyGamepadRumble(ControllerIndex, 0, 0);
}

void InputSystem::StopAllGamepadRumble()
{
    for (int PadIndex = 0; PadIndex < INPUT_GAMEPAD_MAX_CONTROLLERS; ++PadIndex)
    {
        StopGamepadRumble(PadIndex);
    }
}

FInputSystemSnapshot InputSystem::TickAndMakeSnapshot()
{
    Tick();
    return MakeSnapshot();
}

FInputSystemSnapshot InputSystem::MakeSnapshot() const
{
    return CurrentSnapshot;
}

void InputSystem::RefreshSnapshot()
{
    UpdateCurrentSnapshot();
}

void InputSystem::SetUseRawMouse(bool bEnable)
{
    if (bUseRawMouse == bEnable)
    {
        return;
    }

    bUseRawMouse = bEnable;
    ResetMouseDelta();
    UpdateCurrentSnapshot();
}

void InputSystem::AddRawMouseDelta(int DeltaX, int DeltaY)
{
    RawMouseDeltaAccumX += DeltaX;
    RawMouseDeltaAccumY += DeltaY;
}

void InputSystem::ResetTransientState()
{
    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;
    ResetDragState();
    ResetMouseDelta();
    ResetWheelDelta();
    UpdateCurrentSnapshot();
}

void InputSystem::ResetAllKeyStates()
{
    for (int VK = 0; VK < 256; ++VK)
    {
        CurrentStates[VK] = false;
        PrevStates[VK] = false;
    }
    ResetGamepadStates();
    UpdateCurrentSnapshot();
}

void InputSystem::ResetMouseDelta()
{
    GetCursorPos(&MousePos);
    PrevMousePos = MousePos;
    FrameMouseDeltaX = 0;
    FrameMouseDeltaY = 0;
    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;
    UpdateCurrentSnapshot();
}

void InputSystem::ResetWheelDelta()
{
    ScrollDelta = 0;
    PrevScrollDelta = 0;
    UpdateCurrentSnapshot();
}

void InputSystem::ResetCaptureStateForPIEEnd()
{
    SetUseRawMouse(false);
    ResetAllKeyStates();
    ResetTransientState();
    GuiState.bUsingMouse = false;
    GuiState.bUsingKeyboard = false;
    GuiState.bUsingTextInput = false;
    UpdateCurrentSnapshot();
}

void InputSystem::UpdateCurrentSnapshot()
{
    FInputSystemSnapshot Snapshot{};
    for (int VK = 0; VK < 256; ++VK)
    {
        Snapshot.KeyDown[VK] = CurrentStates[VK];
        Snapshot.KeyPressed[VK] = CurrentStates[VK] && !PrevStates[VK];
        Snapshot.KeyReleased[VK] = !CurrentStates[VK] && PrevStates[VK];
    }

    Snapshot.bLeftMouseDown = Snapshot.KeyDown[VK_LBUTTON];
    Snapshot.bLeftMousePressed = Snapshot.KeyPressed[VK_LBUTTON];
    Snapshot.bLeftMouseReleased = Snapshot.KeyReleased[VK_LBUTTON];
    Snapshot.bRightMouseDown = Snapshot.KeyDown[VK_RBUTTON];
    Snapshot.bRightMousePressed = Snapshot.KeyPressed[VK_RBUTTON];
    Snapshot.bRightMouseReleased = Snapshot.KeyReleased[VK_RBUTTON];
    Snapshot.bMiddleMouseDown = Snapshot.KeyDown[VK_MBUTTON];
    Snapshot.bMiddleMousePressed = Snapshot.KeyPressed[VK_MBUTTON];
    Snapshot.bMiddleMouseReleased = Snapshot.KeyReleased[VK_MBUTTON];
    Snapshot.bXButton1Down = Snapshot.KeyDown[VK_XBUTTON1];
    Snapshot.bXButton1Pressed = Snapshot.KeyPressed[VK_XBUTTON1];
    Snapshot.bXButton1Released = Snapshot.KeyReleased[VK_XBUTTON1];
    Snapshot.bXButton2Down = Snapshot.KeyDown[VK_XBUTTON2];
    Snapshot.bXButton2Pressed = Snapshot.KeyPressed[VK_XBUTTON2];
    Snapshot.bXButton2Released = Snapshot.KeyReleased[VK_XBUTTON2];

    Snapshot.MousePos = MousePos;
    Snapshot.MouseDeltaX = FrameMouseDeltaX;
    Snapshot.MouseDeltaY = FrameMouseDeltaY;
    Snapshot.ScrollDelta = PrevScrollDelta;

    Snapshot.bLeftDragStarted = bLeftDragJustStarted;
    Snapshot.bLeftDragging = bLeftDragging;
    Snapshot.bLeftDragEnded = bLeftDragJustEnded;
    Snapshot.LeftDragVector = GetLeftDragVector();

    Snapshot.bRightDragStarted = bRightDragJustStarted;
    Snapshot.bRightDragging = bRightDragging;
    Snapshot.bRightDragEnded = bRightDragJustEnded;
    Snapshot.RightDragVector = GetRightDragVector();

    Snapshot.bUsingRawMouse = bUseRawMouse;
    Snapshot.bGuiUsingMouse = GuiState.bUsingMouse;
    Snapshot.bGuiUsingKeyboard = GuiState.bUsingKeyboard;
    Snapshot.bGuiUsingTextInput = GuiState.bUsingTextInput;
    Snapshot.bWindowFocused = bWindowFocused;

    for (int PadIndex = 0; PadIndex < INPUT_GAMEPAD_MAX_CONTROLLERS; ++PadIndex)
    {
        Snapshot.Gamepads[PadIndex] = CurrentGamepadStates[PadIndex];
        for (int ButtonIndex = 0; ButtonIndex < INPUT_GAMEPAD_BUTTON_COUNT; ++ButtonIndex)
        {
            Snapshot.Gamepads[PadIndex].ButtonDown[ButtonIndex] = CurrentGamepadButtons[PadIndex][ButtonIndex];
            Snapshot.Gamepads[PadIndex].ButtonPressed[ButtonIndex] =
                CurrentGamepadButtons[PadIndex][ButtonIndex] && !PrevGamepadButtons[PadIndex][ButtonIndex];
            Snapshot.Gamepads[PadIndex].ButtonReleased[ButtonIndex] =
                !CurrentGamepadButtons[PadIndex][ButtonIndex] && PrevGamepadButtons[PadIndex][ButtonIndex];
        }
    }
    CurrentSnapshot = Snapshot;
}

void InputSystem::PollGamepads()
{
    for (int PadIndex = 0; PadIndex < INPUT_GAMEPAD_MAX_CONTROLLERS; ++PadIndex)
    {
        for (int ButtonIndex = 0; ButtonIndex < INPUT_GAMEPAD_BUTTON_COUNT; ++ButtonIndex)
        {
            PrevGamepadButtons[PadIndex][ButtonIndex] = CurrentGamepadButtons[PadIndex][ButtonIndex];
            CurrentGamepadButtons[PadIndex][ButtonIndex] = false;
        }

        FInputSystemSnapshot::FGamepadState& Pad = CurrentGamepadStates[PadIndex];
        Pad = FInputSystemSnapshot::FGamepadState{};

        XINPUT_STATE State{};
        if (XInputGetState(static_cast<DWORD>(PadIndex), &State) != ERROR_SUCCESS)
        {
            continue;
        }

        const XINPUT_GAMEPAD& Gamepad = State.Gamepad;
        Pad.bConnected = true;
        Pad.LeftX = NormalizeStickAxis(Gamepad.sThumbLX);
        Pad.LeftY = NormalizeStickAxis(Gamepad.sThumbLY);
        Pad.RightX = NormalizeStickAxis(Gamepad.sThumbRX);
        Pad.RightY = NormalizeStickAxis(Gamepad.sThumbRY);
        Pad.LeftTrigger = NormalizeTrigger(Gamepad.bLeftTrigger);
        Pad.RightTrigger = NormalizeTrigger(Gamepad.bRightTrigger);

        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadFaceButtonBottom)] = (Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadFaceButtonRight)] = (Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadFaceButtonLeft)] = (Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadFaceButtonTop)] = (Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadLeftShoulder)] = (Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadRightShoulder)] = (Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadLeftTrigger)] = Pad.LeftTrigger >= GAMEPAD_TRIGGER_BUTTON_THRESHOLD;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadRightTrigger)] = Pad.RightTrigger >= GAMEPAD_TRIGGER_BUTTON_THRESHOLD;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadLeftThumbstick)] = (Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadRightThumbstick)] = (Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadDPadUp)] = (Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadDPadDown)] = (Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadDPadLeft)] = (Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadDPadRight)] = (Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadStart)] = (Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
        CurrentGamepadButtons[PadIndex][GetGamepadButtonIndex(GamepadBack)] = (Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
    }
}

void InputSystem::UpdateGamepadRumble()
{
    const ULONGLONG NowMs = GetTickCount64();
    for (int PadIndex = 0; PadIndex < INPUT_GAMEPAD_MAX_CONTROLLERS; ++PadIndex)
    {
        FGamepadRumbleState& Rumble = GamepadRumbleStates[PadIndex];
        if (!Rumble.bActive)
        {
            continue;
        }

        if (!CurrentGamepadStates[PadIndex].bConnected || NowMs >= Rumble.EndTimeMs)
        {
            StopGamepadRumble(PadIndex);
            continue;
        }

        ApplyGamepadRumble(PadIndex, Rumble.LeftMotor, Rumble.RightMotor);
    }
}

void InputSystem::ApplyGamepadRumble(int ControllerIndex, WORD LeftMotor, WORD RightMotor)
{
    if (!IsValidControllerIndex(ControllerIndex))
    {
        return;
    }

    XINPUT_VIBRATION Vibration{};
    Vibration.wLeftMotorSpeed = LeftMotor;
    Vibration.wRightMotorSpeed = RightMotor;
    XInputSetState(static_cast<DWORD>(ControllerIndex), &Vibration);
}

void InputSystem::ResetGamepadStates()
{
    StopAllGamepadRumble();
    for (int PadIndex = 0; PadIndex < INPUT_GAMEPAD_MAX_CONTROLLERS; ++PadIndex)
    {
        CurrentGamepadStates[PadIndex] = FInputSystemSnapshot::FGamepadState{};
        for (int ButtonIndex = 0; ButtonIndex < INPUT_GAMEPAD_BUTTON_COUNT; ++ButtonIndex)
        {
            CurrentGamepadButtons[PadIndex][ButtonIndex] = false;
            PrevGamepadButtons[PadIndex][ButtonIndex] = false;
        }
    }
}

void InputSystem::ResetDragState()
{
    bLeftDragCandidate = false;
    bRightDragCandidate = false;
    bLeftDragging = false;
    bRightDragging = false;
    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;
    LeftDragStartPos = MousePos;
    LeftMouseDownPos = MousePos;
    RightDragStartPos = MousePos;
    RightMouseDownPos = MousePos;
}

void InputSystem::FilterDragThreshold(
    bool& bCandidate, bool& bDragging, bool& bJustStarted,
    const POINT& MouseDownPos, POINT& DragStartPos)
{
    if (bCandidate && !bDragging)
    {
        int DX = MousePos.x - MouseDownPos.x;
        int DY = MousePos.y - MouseDownPos.y;
        int DistSq = DX * DX + DY * DY;

        if (DistSq >= DRAG_THRESHOLD * DRAG_THRESHOLD)
        {
            bJustStarted = true;
            bDragging = true;
            DragStartPos = MouseDownPos;
        }
    }
}

POINT InputSystem::GetLeftDragVector() const
{
    POINT V;
    V.x = MousePos.x - LeftDragStartPos.x;
    V.y = MousePos.y - LeftDragStartPos.y;
    return V;
}

POINT InputSystem::GetRightDragVector() const
{
    POINT V;
    V.x = MousePos.x - RightDragStartPos.x;
    V.y = MousePos.y - RightDragStartPos.y;
    return V;
}

float InputSystem::GetLeftDragDistance() const
{
    POINT V = GetLeftDragVector();
    return std::sqrt((float)(V.x * V.x + V.y * V.y));
}

float InputSystem::GetRightDragDistance() const
{
    POINT V = GetRightDragVector();
    return std::sqrt((float)(V.x * V.x + V.y * V.y));
}
