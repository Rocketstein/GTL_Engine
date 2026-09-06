#pragma once

#include <windows.h>

class FInputState;
class FWindow;

class FWindowsApplication
{
  public:
    FWindowsApplication();
    ~FWindowsApplication();

    bool Initialize();
    void Initialize(FWindow *InWindow, FInputState *InInputState);
    void Shutdown();

    bool PumpMessages();
    void RequestQuit();
    bool IsQuitRequested() const;

    void SetWindowFocus(bool bFocused);
    void OnWindowResized(int InWidth, int InHeight);
    void OnMouseWheel(float InWheelDelta, int InMouseX, int InMouseY);

    FWindow     *GetWindow() const { return Window; }
    FInputState *GetInputState() const { return InputState; }

  private:
    bool         bQuitRequested = false;
    bool         bFocused = false;
    FWindow     *Window = nullptr;
    FInputState *InputState = nullptr;
    int          LastResizeWidth = 0;
    int          LastResizeHeight = 0;
};
