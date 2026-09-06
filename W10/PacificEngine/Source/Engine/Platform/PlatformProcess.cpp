#include "Engine/Platform/PlatformProcess.h"

#include <Windows.h>
#include <shellapi.h>
#include <system_error>

bool FPlatformProcess::OpenFile(const std::filesystem::path& Path)
{
    if (Path.empty())
    {
        return false;
    }

    const std::filesystem::path NormalizedPath = Path.lexically_normal();

    std::error_code Ec;
    if (!std::filesystem::exists(NormalizedPath, Ec) || Ec)
    {
        return false;
    }

    HINSTANCE Result = ShellExecuteW(
        nullptr,
        L"open",
        NormalizedPath.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);

    return reinterpret_cast<intptr_t>(Result) > 32;
}
