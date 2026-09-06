#pragma once
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include "Core/Platform/PlatformTypes.h"
#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>

namespace D3D11Util
{
    template <typename T>
    inline void SafeRelease(T*& Resource)
    {
        if (Resource != nullptr)
        {
            Resource->Release();
            Resource = nullptr;
        }
    }
}
