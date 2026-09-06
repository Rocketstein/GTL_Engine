#pragma once

#include <filesystem>

class FPlatformProcess
{
public:
    static bool OpenFile(const std::filesystem::path& Path);
};
