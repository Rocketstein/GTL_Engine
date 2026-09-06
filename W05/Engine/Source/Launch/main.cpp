#include "Launch/EngineLoop.h"
#include <windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

FEngineLoop GEngineLoop;

int32 Main(const TCHAR *CmdLine)
{
    GEngineLoop.PreInit(CmdLine);
    if (GEngineLoop.Init() != 0)
    {
        return 1;
    }

    GEngineLoop.Run();
    GEngineLoop.Exit();
    return 0;
}

int32 WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int32) { return Main(nullptr); }
