#include "ApplicationCore/WindowsApplication.h"
#include "ApplicationCore/InputState.h"
#include "ApplicationCore/Window.h"

FWindowsApplication::FWindowsApplication() = default;
FWindowsApplication::~FWindowsApplication() = default;

bool FWindowsApplication::Initialize() { return true; }

void FWindowsApplication::Initialize(FWindow *InWindow, FInputState *InInputState)
{
    Window = InWindow;
    InputState = InInputState;
}

void FWindowsApplication::Shutdown()
{
    Window = nullptr;
    InputState = nullptr;
}

bool FWindowsApplication::PumpMessages()
{
    MSG Message = {};
    while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE))
    {
        if (Message.message == WM_QUIT)
        {
            bQuitRequested = true;
            return false;
        }

        if (Message.message == WM_SETFOCUS)
        {
            SetWindowFocus(true);
        }
        else if (Message.message == WM_KILLFOCUS)
        {
            SetWindowFocus(false);
        }

        TranslateMessage(&Message);
        DispatchMessageW(&Message);
    }

    return !bQuitRequested;
}

void FWindowsApplication::RequestQuit() { bQuitRequested = true; }

bool FWindowsApplication::IsQuitRequested() const { return bQuitRequested; }

void FWindowsApplication::SetWindowFocus(bool bInFocused)
{
    bFocused = bInFocused;
    if (InputState != nullptr)
    {
        InputState->SetWindowFocus(bInFocused);
    }
}

void FWindowsApplication::OnWindowResized(int InWidth, int InHeight)
{
    LastResizeWidth = InWidth;
    LastResizeHeight = InHeight;
}

void FWindowsApplication::OnMouseWheel(float InWheelDelta, int, int)
{
    if (InputState != nullptr)
    {
        InputState->SetWheelDelta(InWheelDelta);
    }
}
