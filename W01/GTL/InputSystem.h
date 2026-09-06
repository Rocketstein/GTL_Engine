#pragma once
#include <windows.h>

class InputSystem {
private:
public:
    static bool currentStates[256];
    static bool prevStates[256];

    static void Update() {
        for (int i = 0; i < 256; ++i) {
            prevStates[i] = currentStates[i];
            currentStates[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
        }
    }

    // --- Keyboard & Mouse Common ---
    static bool GetKeyDown(int vk) { return currentStates[vk] && !prevStates[vk]; }
    static bool GetKey(int vk) { return currentStates[vk]; }
    static bool GetKeyUp(int vk) { return !currentStates[vk] && prevStates[vk]; }
};
