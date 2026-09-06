#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Platform/Paths.h"

#include <filesystem>

namespace MeshImportPathUtils
{
inline std::filesystem::path ResolveAbsolutePath(const FString& PathValue)
{
    std::filesystem::path Path = FPaths::ToPath(FPaths::ToWide(PathValue));
    if (!Path.is_absolute())
    {
        Path = FPaths::ToPath(FPaths::RootDir()) / Path;
    }

    std::error_code Ec;
    std::filesystem::path Canonical = std::filesystem::weakly_canonical(Path, Ec);
    if (!Ec)
    {
        return Canonical;
    }

    return Path.lexically_normal();
}

inline bool IsUnderDirectory(const std::filesystem::path& Candidate, const std::filesystem::path& Parent)
{
    std::error_code Ec;
    const std::filesystem::path Relative = std::filesystem::relative(Candidate, Parent, Ec);
    if (Ec || Relative.empty())
    {
        return false;
    }

    auto It = Relative.begin();
    return It == Relative.end() || It->wstring() != L"..";
}

inline FString MakeProjectRelativePath(const std::filesystem::path& FullPath)
{
    const std::filesystem::path Normalized = FullPath.lexically_normal();
    const std::filesystem::path Root = std::filesystem::path(FPaths::RootDir()).lexically_normal();
    if (IsUnderDirectory(Normalized, Root))
    {
        return FPaths::MakeRelativeToRoot(Normalized);
    }

    return FPaths::FromPath(Normalized);
}

inline std::filesystem::path BuildFBXEmbeddedTextureDirectory(const FString& FBXFilePath)
{
    return ResolveAbsolutePath(FBXFilePath).parent_path() / L"texture";
}

inline std::filesystem::path BuildFBXMaterialOutputDirectory(const FString& FBXFilePath)
{
    const std::filesystem::path SourcePath = ResolveAbsolutePath(FBXFilePath);
    const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::ContentDir()).lexically_normal();
    const std::filesystem::path MaterialsRoot = ContentRoot / L"Materials";

    std::error_code Ec;
    std::filesystem::path RelativeToContent = std::filesystem::relative(SourcePath, ContentRoot, Ec);
    if (Ec || RelativeToContent.empty())
    {
        return MaterialsRoot / SourcePath.stem();
    }

    return (MaterialsRoot / RelativeToContent.parent_path() / SourcePath.stem()).lexically_normal();
}

inline FString SanitizeFileNameComponent(FString Name)
{
    for (char& Ch : Name)
    {
        switch (Ch)
        {
        case '<':
        case '>':
        case ':':
        case '"':
        case '/':
        case '\\':
        case '|':
        case '?':
        case '*':
            Ch = '_';
            break;
        default:
            break;
        }
    }

    while (!Name.empty() && (Name.back() == '.' || Name.back() == ' '))
    {
        Name.pop_back();
    }

    return Name;
}
} // namespace MeshImportPathUtils
