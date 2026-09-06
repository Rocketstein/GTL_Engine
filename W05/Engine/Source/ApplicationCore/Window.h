#pragma once

#include <windows.h>

class FWindowsApplication;

class FWindow
{
public:
    FWindow();
    ~FWindow();

    bool Create(FWindowsApplication* InOwnerApplication,
                const wchar_t*      InTitle,
                int                 InClientWidth,
                int                 InClientHeight);
    void Destroy();

    bool IsValid() const;

    HWND GetHwnd() const;

    int GetClientWidth() const;
    int GetClientHeight() const;

    POINT GetMouseClientPosition() const;
    void  SetTitle(const wchar_t* InTitle) const;

    void UpdateClientSizeFromOS();

private:
    static LRESULT CALLBACK StaticWndProc(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam);
    LRESULT HandleMessage(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam);

private:
    FWindowsApplication* OwnerApplication;
    HWND                  Hwnd;
    HINSTANCE             InstanceHandle;
    int                   ClientWidth;
    int                   ClientHeight;
    bool                  bClassRegistered;
};
