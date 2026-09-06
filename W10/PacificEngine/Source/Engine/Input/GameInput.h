#pragma once

#include "Input/InputTypes.h"
#include <string>

// Unity 스타일의 입력 관리자
// GameInputTool까지 Unreal 스타일로 입력을 처리하다가 여기서 Unity 스타일로 래핑
struct FGameInput {
    static bool GetKey(int vk);
    static bool GetKeyDown(int vk);
    static bool GetKeyUp(int vk);
    static float GetAxis(const std::string& axisName);

    // Mouse Input
    static bool GetMouseButton(int button);
    static bool GetMouseButtonDown(int button);
    static bool GetMouseButtonUp(int button);
    static void GetMousePosition(int& outX, int& outY);

    // Internal use by FGameInputTool
    static void UpdateKeyState(int vk, bool bDown, bool bPressed, bool bReleased);
    static void UpdatePointerState(EPointerButton button, EPointerEventType type, POINT localPos);
    static void ResetFrameState();

    struct FState {
        bool KeyDown[256] = {};
        bool KeyPressed[256] = {};
        bool KeyReleased[256] = {};

        bool MouseButtonDown[6] = {};
        bool MouseButtonPressed[6] = {};
        bool MouseButtonReleased[6] = {};
        POINT MousePosition = { 0, 0 };
    };

    static FState CurrentState;
};
