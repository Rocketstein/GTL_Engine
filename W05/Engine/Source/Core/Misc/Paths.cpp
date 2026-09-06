#include "Core/Misc/Paths.h"
#include <cassert>
#include <system_error>


#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

FPathConfig FPaths::Config{};
bool        FPaths::bInitialized = false;

bool FPaths::Initialize(const FPathConfig &InConfig)
{
    if (!ValidateConfig(InConfig))
    {
        return false;
    }

    FPathConfig NewConfig{};
    NewConfig.EngineRoot = Normalize(InConfig.EngineRoot);
    NewConfig.AppRoot = Normalize(InConfig.AppRoot);

    NewConfig.EngineContentDir = InConfig.EngineContentDir.empty()
                                     ? Combine(NewConfig.EngineRoot, L"Resources")
                                     : Normalize(InConfig.EngineContentDir);

    NewConfig.AppContentDir = InConfig.AppContentDir.empty()
                                  ? Combine(NewConfig.AppRoot, L"Content")
                                  : Normalize(InConfig.AppContentDir);

    NewConfig.SavedDir = InConfig.SavedDir.empty() ? Combine(NewConfig.AppRoot, L"Saved")
                                                   : Normalize(InConfig.SavedDir);

    NewConfig.ShaderDir = InConfig.ShaderDir.empty() ? Combine(NewConfig.AppContentDir, L"Shader")
                                                     : Normalize(InConfig.ShaderDir);

    NewConfig.ShaderCacheDir = InConfig.ShaderCacheDir.empty()
                                   ? Combine(NewConfig.SavedDir, L"ShaderCache")
                                   : Normalize(InConfig.ShaderCacheDir);

    Config = std::move(NewConfig);
    bInitialized = true;
    return true;
}

bool FPaths::IsInitialized() { return bInitialized; }

const fs::path &FPaths::EngineRoot() { return GetConfig().EngineRoot; }

const fs::path &FPaths::AppRoot() { return GetConfig().AppRoot; }

const fs::path &FPaths::EngineContentDir() { return GetConfig().EngineContentDir; }

const fs::path &FPaths::AppContentDir() { return GetConfig().AppContentDir; }

const fs::path &FPaths::SavedDir() { return GetConfig().SavedDir; }

const fs::path &FPaths::ShaderDir() { return GetConfig().ShaderDir; }

const fs::path &FPaths::ShaderCacheDir() { return GetConfig().ShaderCacheDir; }

void FPaths::EnsureRuntimeDirectories()
{
    const FPathConfig &CurrentConfig = GetConfig();

    std::error_code ErrorCode;
    fs::create_directories(CurrentConfig.SavedDir, ErrorCode);
    ErrorCode.clear();
    fs::create_directories(CurrentConfig.ShaderCacheDir, ErrorCode);
}

fs::path FPaths::Normalize(const fs::path &InPath)
{
    if (InPath.empty())
    {
        return {};
    }

    return InPath.lexically_normal();
}

fs::path FPaths::Combine(const fs::path &Base, const fs::path &Relative)
{
    return Normalize(Base / Relative);
}

fs::path FPaths::PathFromUtf8(const FString &Utf8Path)
{
    if (Utf8Path.empty())
    {
        return {};
    }

#if defined(_WIN32)
    const int RequiredSize = MultiByteToWideChar(CP_UTF8, 0, Utf8Path.c_str(), -1, nullptr, 0);
    if (RequiredSize <= 1)
    {
        return {};
    }

    std::wstring WidePath(static_cast<size_t>(RequiredSize), L'\0');
    const int    ConvertedSize =
        MultiByteToWideChar(CP_UTF8, 0, Utf8Path.c_str(), -1, WidePath.data(), RequiredSize);
    if (ConvertedSize <= 1)
    {
        return {};
    }

    WidePath.resize(static_cast<size_t>(ConvertedSize - 1));
    return fs::path(WidePath);
#else
    return fs::path(Utf8Path);
#endif
}

FString FPaths::Utf8FromPath(const fs::path &Path)
{
    if (Path.empty())
    {
        return {};
    }

#if defined(_WIN32)
    const std::wstring WidePath = Path.native();
    const int          RequiredSize =
        WideCharToMultiByte(CP_UTF8, 0, WidePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (RequiredSize <= 1)
    {
        return {};
    }

    FString   Utf8Path(static_cast<size_t>(RequiredSize), '\0');
    const int ConvertedSize = WideCharToMultiByte(CP_UTF8, 0, WidePath.c_str(), -1, Utf8Path.data(),
                                                  RequiredSize, nullptr, nullptr);
    if (ConvertedSize <= 1)
    {
        return {};
    }

    Utf8Path.resize(static_cast<size_t>(ConvertedSize - 1));
    return Utf8Path;
#else
    return Path.generic_string();
#endif
}

const FPathConfig &FPaths::GetConfig()
{
    assert(bInitialized && "FPaths must be initialized before use.");
    return Config;
}

bool FPaths::ValidateConfig(const FPathConfig &InConfig)
{
    return !InConfig.EngineRoot.empty() && !InConfig.AppRoot.empty();
}