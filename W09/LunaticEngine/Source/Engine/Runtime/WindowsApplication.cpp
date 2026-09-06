#include "Engine/Runtime/WindowsApplication.h"
#include "Engine/Runtime/resource.h"
#include "Core/ProjectSettings.h"

#include <windowsx.h>
#include <vector>

#include "Engine/Input/InputManager.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);

namespace
{
#if WITH_EDITOR || IS_OBJ_VIEWER
	constexpr LONG WindowedStyle = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_VISIBLE;
#else
	constexpr LONG WindowedStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
#endif
	constexpr UINT ResizeRedrawTimerId = 1;
	constexpr UINT ResizeRedrawIntervalMs = 16;
	int GetResizeBorderForWindow(HWND hWnd)
	{
		if (!hWnd)
		{
			return 0;
		}

		const UINT Dpi = GetDpiForWindow(hWnd);
		return GetSystemMetricsForDpi(SM_CXFRAME, Dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, Dpi);
	}
}

LRESULT CALLBACK FWindowsApplication::StaticWndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam)
{
	FWindowsApplication* App = reinterpret_cast<FWindowsApplication*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (Msg == WM_NCCREATE)
	{
		CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
		App = reinterpret_cast<FWindowsApplication*>(CreateStruct->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(App));
	}

	if (App)
	{
		return App->WndProc(hWnd, Msg, wParam, lParam);
	}

	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

LRESULT FWindowsApplication::WndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam)
{
	FInputManager::Get().ProcessMessage(hWnd, Msg, wParam, lParam);

	if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
	{
		return true;
	}

	switch (Msg)
	{
#if WITH_EDITOR || IS_OBJ_VIEWER
	case WM_NCCALCSIZE:
		return 0;
#endif
	case WM_GETMINMAXINFO:
	{
		MONITORINFO MonitorInfo{};
		MonitorInfo.cbSize = sizeof(MONITORINFO);
		if (GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &MonitorInfo))
		{
			MINMAXINFO* MinMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
			const RECT& WorkArea = MonitorInfo.rcWork;
			const RECT& MonitorArea = MonitorInfo.rcMonitor;

			MinMaxInfo->ptMaxPosition.x = WorkArea.left - MonitorArea.left;
			MinMaxInfo->ptMaxPosition.y = WorkArea.top - MonitorArea.top;
			MinMaxInfo->ptMaxSize.x = WorkArea.right - WorkArea.left;
			MinMaxInfo->ptMaxSize.y = WorkArea.bottom - WorkArea.top;
		}
		return 0;
	}
#if WITH_EDITOR || IS_OBJ_VIEWER
	case WM_NCHITTEST:
	{
		POINT Cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		POINT ClientPoint = Cursor;
		ScreenToClient(hWnd, &ClientPoint);
		if (Window.IsInTitleBarControlRegion(ClientPoint))
		{
			return HTCLIENT;
		}

		RECT WindowRect{};
		GetWindowRect(hWnd, &WindowRect);
		const int ResizeBorderThickness = GetResizeBorderForWindow(hWnd);
		const bool bAllowResize = !Window.IsResizeLocked() && !IsZoomed(hWnd) && ResizeBorderThickness > 0;

		if (bAllowResize)
		{
			const bool bLeft = Cursor.x >= WindowRect.left && Cursor.x < WindowRect.left + ResizeBorderThickness;
			const bool bRight = Cursor.x < WindowRect.right && Cursor.x >= WindowRect.right - ResizeBorderThickness;
			const bool bBottom = Cursor.y < WindowRect.bottom && Cursor.y >= WindowRect.bottom - ResizeBorderThickness;
			const bool bTop = Cursor.y >= WindowRect.top && Cursor.y < WindowRect.top + ResizeBorderThickness;

			if (bTop && bLeft) return HTTOPLEFT;
			if (bTop && bRight) return HTTOPRIGHT;
			if (bBottom && bLeft) return HTBOTTOMLEFT;
			if (bBottom && bRight) return HTBOTTOMRIGHT;
			if (bTop) return HTTOP;
			if (bLeft) return HTLEFT;
			if (bRight) return HTRIGHT;
			if (bBottom) return HTBOTTOM;
		}

		if (Window.IsInTitleBarDragRegion(ClientPoint))
		{
			return HTCAPTION;
		}

		return HTCLIENT;
	}
#endif
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			unsigned int Width = LOWORD(lParam);
			unsigned int Height = HIWORD(lParam);
			Window.OnResized(Width, Height);
			if (OnResizedCallback)
			{
				OnResizedCallback(Width, Height);
			}
		}
		return 0;
	case WM_ENTERSIZEMOVE:
		bIsResizing = true;
		SetTimer(hWnd, ResizeRedrawTimerId, ResizeRedrawIntervalMs, nullptr);
		return 0;
	case WM_EXITSIZEMOVE:
		bIsResizing = false;
		KillTimer(hWnd, ResizeRedrawTimerId);
		return 0;
	case WM_SIZING:
		if (OnSizingCallback)
		{
			OnSizingCallback();
		}
		RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		return 0;
	case WM_TIMER:
		if (wParam == ResizeRedrawTimerId && bIsResizing && OnSizingCallback)
		{
			OnSizingCallback();
			RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
			return 0;
		}
		break;
	default:
		break;
	}

	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

bool FWindowsApplication::Init(HINSTANCE InHInstance)
{
	HInstance = InHInstance;

	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"LunaticEngine";
	WNDCLASSEXW WndClass = {};
	WndClass.cbSize = sizeof(WNDCLASSEXW);
	WndClass.lpfnWndProc = StaticWndProc;
	WndClass.hInstance = HInstance;
	WndClass.hIcon = LoadIconW(HInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
	WndClass.hIconSm = LoadIconW(HInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
	WndClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	WndClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
	WndClass.lpszClassName = WindowClass;

	RegisterClassExW(&WndClass);

	const uint32 WindowWidth = (std::max)(320u, FProjectSettings::Get().Game.WindowWidth);
	const uint32 WindowHeight = (std::max)(240u, FProjectSettings::Get().Game.WindowHeight);

	HWND HWindow = CreateWindowExW(
		0,
		WindowClass,
		Title,
		WindowedStyle,
		CW_USEDEFAULT, CW_USEDEFAULT,
		static_cast<int>(WindowWidth), static_cast<int>(WindowHeight),
		nullptr, nullptr, HInstance, this);

	if (!HWindow)
	{
		return false;
	}

	RAWINPUTDEVICE RawMouseDevice = {};
	RawMouseDevice.usUsagePage = 0x01;
	RawMouseDevice.usUsage = 0x02;
	RawMouseDevice.dwFlags = RIDEV_INPUTSINK;
	RawMouseDevice.hwndTarget = HWindow;
	RegisterRawInputDevices(&RawMouseDevice, 1, sizeof(RAWINPUTDEVICE));

	Window.Initialize(HWindow);
#if WITH_EDITOR || IS_OBJ_VIEWER
	Window.SetResizeLocked(false);
#else
	Window.SetResizeLocked(FProjectSettings::Get().Game.bLockWindowResolution);
#endif
	Window.ResizeClientArea(WindowWidth, WindowHeight);
	return true;
}

void FWindowsApplication::PumpMessages()
{
	MSG Msg;
	while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);

		if (Msg.message == WM_QUIT)
		{
			bIsExitRequested = true;
			break;
		}
	}
}

void FWindowsApplication::Destroy()
{
	if (Window.GetHWND())
	{
		DestroyWindow(Window.GetHWND());
	}
}
