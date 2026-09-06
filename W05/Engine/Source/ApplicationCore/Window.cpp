#include "Window.h"
#include "ImGui/imgui_impl_win32.h"
#include "WindowsApplication.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

namespace
{
    constexpr const wchar_t *GWindowClassName = L"Week4WindowClass";
}

FWindow::FWindow()
    : OwnerApplication(nullptr), Hwnd(nullptr), InstanceHandle(GetModuleHandle(nullptr)),
      ClientWidth(0), ClientHeight(0), bClassRegistered(false)
{
}

FWindow::~FWindow() { Destroy(); }

bool FWindow::Create(FWindowsApplication *InOwnerApplication, const wchar_t *InTitle,
                     int InClientWidth, int InClientHeight)
{
    OwnerApplication = InOwnerApplication;
    ClientWidth = InClientWidth;
    ClientHeight = InClientHeight;

    WNDCLASSEXW WindowClass = {};
    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    WindowClass.lpfnWndProc = &FWindow::StaticWndProc;
    WindowClass.hInstance = InstanceHandle;
    WindowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    WindowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    WindowClass.lpszClassName = GWindowClassName;

    if (RegisterClassExW(&WindowClass) != 0)
    {
        bClassRegistered = true;
    }
    else
    {
        const DWORD LastError = GetLastError();
        if (LastError != ERROR_CLASS_ALREADY_EXISTS)
        {
            return false;
        }
    }

    RECT WindowRect = {0, 0, InClientWidth, InClientHeight};
    AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

    Hwnd =
        CreateWindowExW(0, GWindowClassName, InTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, WindowRect.right - WindowRect.left,
                        WindowRect.bottom - WindowRect.top, nullptr, nullptr, InstanceHandle, this);

    if (Hwnd == nullptr)
    {
        return false;
    }

    ShowWindow(Hwnd, SW_SHOW);
    UpdateWindow(Hwnd);
    UpdateClientSizeFromOS();
    return true;
}

void FWindow::Destroy()
{
    if (Hwnd != nullptr)
    {
        DestroyWindow(Hwnd);
        Hwnd = nullptr;
    }
}

bool FWindow::IsValid() const { return Hwnd != nullptr; }

HWND FWindow::GetHwnd() const { return Hwnd; }

int FWindow::GetClientWidth() const { return ClientWidth; }

int FWindow::GetClientHeight() const { return ClientHeight; }

POINT FWindow::GetMouseClientPosition() const
{
    POINT Point = {};
    GetCursorPos(&Point);
    if (Hwnd != nullptr)
    {
        ScreenToClient(Hwnd, &Point);
    }
    return Point;
}

void FWindow::SetTitle(const wchar_t *InTitle) const
{
    if (Hwnd != nullptr)
    {
        SetWindowTextW(Hwnd, InTitle);
    }
}

void FWindow::UpdateClientSizeFromOS()
{
    if (Hwnd == nullptr)
    {
        ClientWidth = 0;
        ClientHeight = 0;
        return;
    }

    RECT ClientRect = {};
    GetClientRect(Hwnd, &ClientRect);
    ClientWidth = ClientRect.right - ClientRect.left;
    ClientHeight = ClientRect.bottom - ClientRect.top;
}

LRESULT CALLBACK FWindow::StaticWndProc(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    FWindow *Window = nullptr;

    if (Msg == WM_NCCREATE)
    {
        const CREATESTRUCTW *CreateStruct = reinterpret_cast<const CREATESTRUCTW *>(LParam);
        Window = static_cast<FWindow *>(CreateStruct->lpCreateParams);
        SetWindowLongPtrW(HWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Window));
        if (Window != nullptr)
        {
            Window->Hwnd = HWnd;
        }
    }
    else
    {
        Window = reinterpret_cast<FWindow *>(GetWindowLongPtrW(HWnd, GWLP_USERDATA));
    }

    if (Window != nullptr)
    {
        return Window->HandleMessage(HWnd, Msg, WParam, LParam);
    }

    return DefWindowProcW(HWnd, Msg, WParam, LParam);
}

LRESULT FWindow::HandleMessage(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    if (ImGui_ImplWin32_WndProcHandler(HWnd, Msg, WParam, LParam))
    {
        return 1;
    }

    switch (Msg)
    {
    case WM_SIZE:
        UpdateClientSizeFromOS();
        if (OwnerApplication != nullptr)
        {
            OwnerApplication->OnWindowResized(ClientWidth, ClientHeight);
        }
        return 0;

    case WM_CLOSE:
        if (OwnerApplication != nullptr)
        {
            OwnerApplication->RequestQuit();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_MOUSEWHEEL:
        if (OwnerApplication != nullptr)
        {
            POINT ScreenPoint = {GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam)};
            ScreenToClient(HWnd, &ScreenPoint);
            const float WheelDelta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) /
                                     static_cast<float>(WHEEL_DELTA);
            OwnerApplication->OnMouseWheel(WheelDelta, ScreenPoint.x, ScreenPoint.y);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(HWnd, Msg, WParam, LParam);
}
