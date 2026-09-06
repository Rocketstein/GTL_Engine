#include "Engine/Runtime/WindowsWindow.h"

#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

void FWindowsWindow::Initialize(HWND InHWindow)
{
	HWindow = InHWindow;
	bResizeLocked = false;

	RECT Rect;
	GetClientRect(HWindow, &Rect);
	Width = static_cast<float>(Rect.right - Rect.left);
	Height = static_cast<float>(Rect.bottom - Rect.top);
	UpdateWindowVisualStyle();
}

void FWindowsWindow::OnResized(unsigned int InWidth, unsigned int InHeight)
{
	Width = static_cast<float>(InWidth);
	Height = static_cast<float>(InHeight);
	UpdateWindowVisualStyle();
}

void FWindowsWindow::Minimize() const
{
	if (HWindow)
	{
		ShowWindow(HWindow, SW_MINIMIZE);
	}
}

void FWindowsWindow::ToggleMaximize() const
{
	if (!HWindow)
	{
		return;
	}

	ShowWindow(HWindow, IsZoomed(HWindow) ? SW_RESTORE : SW_MAXIMIZE);
}

void FWindowsWindow::Close() const
{
	if (HWindow)
	{
		PostMessage(HWindow, WM_CLOSE, 0, 0);
	}
}

void FWindowsWindow::ResizeClientArea(unsigned int InWidth, unsigned int InHeight) const
{
	if (!HWindow || InWidth == 0 || InHeight == 0)
	{
		return;
	}

	RECT WindowRect{};
	RECT ClientRect{};
	if (!GetWindowRect(HWindow, &WindowRect) || !GetClientRect(HWindow, &ClientRect))
	{
		return;
	}

	const int CurrentWindowWidth = WindowRect.right - WindowRect.left;
	const int CurrentWindowHeight = WindowRect.bottom - WindowRect.top;
	const int CurrentClientWidth = ClientRect.right - ClientRect.left;
	const int CurrentClientHeight = ClientRect.bottom - ClientRect.top;
	const int TargetWindowWidth = CurrentWindowWidth + (static_cast<int>(InWidth) - CurrentClientWidth);
	const int TargetWindowHeight = CurrentWindowHeight + (static_cast<int>(InHeight) - CurrentClientHeight);

	SetWindowPos(HWindow, nullptr, 0, 0, TargetWindowWidth, TargetWindowHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void FWindowsWindow::SetResizeLocked(bool bLocked) const
{
	if (!HWindow)
	{
		return;
	}

	bResizeLocked = bLocked;

	LONG_PTR Style = GetWindowLongPtr(HWindow, GWL_STYLE);
	if (bResizeLocked)
	{
		Style &= ~static_cast<LONG_PTR>(WS_THICKFRAME);
	}
	else
	{
		Style |= static_cast<LONG_PTR>(WS_THICKFRAME);
	}

	Style |= static_cast<LONG_PTR>(WS_MAXIMIZEBOX);

	SetWindowLongPtr(HWindow, GWL_STYLE, Style);
	SetWindowPos(HWindow, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void FWindowsWindow::StartWindowDrag() const
{
	if (!HWindow)
	{
		return;
	}

	ReleaseCapture();
	// Avoid re-entering the Win32 move loop while we are still inside the current ImGui frame.
	PostMessage(HWindow, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

bool FWindowsWindow::IsWindowMaximized() const
{
	return HWindow && IsZoomed(HWindow);
}

float FWindowsWindow::GetTopFrameInset() const
{
	if (!HWindow || IsWindowMaximized())
	{
		return 0.0f;
	}

	const UINT Dpi = GetDpiForWindow(HWindow);
	const int FrameY = GetSystemMetricsForDpi(SM_CYFRAME, Dpi);
	const int PaddedBorder = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, Dpi);
	return static_cast<float>(FrameY + PaddedBorder);
}

void FWindowsWindow::SetTitleBarDragRegion(float X, float Y, float InWidth, float InHeight)
{
	TitleBarDragRegion.left = static_cast<LONG>(X);
	TitleBarDragRegion.top = static_cast<LONG>(Y);
	TitleBarDragRegion.right = static_cast<LONG>(X + InWidth);
	TitleBarDragRegion.bottom = static_cast<LONG>(Y + InHeight);
	bHasTitleBarDragRegion = InWidth > 0.0f && InHeight > 0.0f;
}

void FWindowsWindow::ClearTitleBarDragRegion()
{
	TitleBarDragRegion = RECT{ 0, 0, 0, 0 };
	bHasTitleBarDragRegion = false;
}

bool FWindowsWindow::IsInTitleBarDragRegion(POINT ClientPoint) const
{
	return bHasTitleBarDragRegion &&
		ClientPoint.x >= TitleBarDragRegion.left &&
		ClientPoint.x < TitleBarDragRegion.right &&
		ClientPoint.y >= TitleBarDragRegion.top &&
		ClientPoint.y < TitleBarDragRegion.bottom;
}

void FWindowsWindow::SetTitleBarControlRegion(float X, float Y, float InWidth, float InHeight)
{
	TitleBarControlRegion.left = static_cast<LONG>(X);
	TitleBarControlRegion.top = static_cast<LONG>(Y);
	TitleBarControlRegion.right = static_cast<LONG>(X + InWidth);
	TitleBarControlRegion.bottom = static_cast<LONG>(Y + InHeight);
	bHasTitleBarControlRegion = InWidth > 0.0f && InHeight > 0.0f;
}

void FWindowsWindow::ClearTitleBarControlRegion()
{
	TitleBarControlRegion = RECT{ 0, 0, 0, 0 };
	bHasTitleBarControlRegion = false;
}

bool FWindowsWindow::IsInTitleBarControlRegion(POINT ClientPoint) const
{
	return bHasTitleBarControlRegion &&
		ClientPoint.x >= TitleBarControlRegion.left &&
		ClientPoint.x < TitleBarControlRegion.right &&
		ClientPoint.y >= TitleBarControlRegion.top &&
		ClientPoint.y < TitleBarControlRegion.bottom;
}

POINT FWindowsWindow::ScreenToClientPoint(POINT ScreenPoint) const
{
	ScreenToClient(HWindow, &ScreenPoint);
	return ScreenPoint;
}

void FWindowsWindow::UpdateWindowVisualStyle() const
{
	if (!HWindow)
	{
		return;
	}

	const DWM_WINDOW_CORNER_PREFERENCE CornerPreference = IsWindowMaximized() ? DWMWCP_DONOTROUND : DWMWCP_ROUND;
	DwmSetWindowAttribute(HWindow, DWMWA_WINDOW_CORNER_PREFERENCE, &CornerPreference, sizeof(CornerPreference));

	const BOOL DarkModeEnabled = TRUE;
	DwmSetWindowAttribute(HWindow, DWMWA_USE_IMMERSIVE_DARK_MODE, &DarkModeEnabled, sizeof(DarkModeEnabled));

	const COLORREF BorderColorNone = 0xFFFFFFFEu;
	DwmSetWindowAttribute(HWindow, DWMWA_BORDER_COLOR, &BorderColorNone, sizeof(BorderColorNone));

	const COLORREF CaptionColor = RGB(0, 0, 0);
	DwmSetWindowAttribute(HWindow, DWMWA_CAPTION_COLOR, &CaptionColor, sizeof(CaptionColor));
}
