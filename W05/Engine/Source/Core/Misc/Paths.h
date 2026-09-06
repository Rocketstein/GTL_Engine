#pragma once

#include "Core/Containers/String.h"
#include <filesystem>
#include <utility>

struct FPathConfig
{
    std::filesystem::path EngineRoot;
    std::filesystem::path AppRoot;

    // Optional. If empty, FPaths::Initialize() derives defaults from EngineRoot/AppRoot.
    std::filesystem::path EngineContentDir;
    std::filesystem::path AppContentDir;
    std::filesystem::path SavedDir;
    std::filesystem::path ShaderDir;
    std::filesystem::path ShaderCacheDir;
};

class FPaths
{
  public:
    static bool Initialize(const FPathConfig &InConfig);
    static bool IsInitialized();

    static const std::filesystem::path &EngineRoot();
    static const std::filesystem::path &AppRoot();
    static const std::filesystem::path &EngineContentDir();
    static const std::filesystem::path &AppContentDir();
    static const std::filesystem::path &SavedDir();
    static const std::filesystem::path &ShaderDir();
    static const std::filesystem::path &ShaderCacheDir();

    static void EnsureRuntimeDirectories();

    static std::filesystem::path Normalize(const std::filesystem::path &InPath);

    static std::filesystem::path Combine(const std::filesystem::path &Base,
                                         const std::filesystem::path &Relative);

    static std::filesystem::path PathFromUtf8(const FString &Utf8Path);
    static FString               Utf8FromPath(const std::filesystem::path &Path);

    template <typename... TPaths>
    static std::filesystem::path Combine(const std::filesystem::path &Base,
                                         const std::filesystem::path &Next, const TPaths &...Rest)
    {
        std::filesystem::path Result = Base;
        Result /= Next;
        ((Result /= Rest), ...);
        return Normalize(Result);
    }

  private:
    static const FPathConfig &GetConfig();
    static bool               ValidateConfig(const FPathConfig &InConfig);

  private:
    static FPathConfig Config;
    static bool        bInitialized;
};