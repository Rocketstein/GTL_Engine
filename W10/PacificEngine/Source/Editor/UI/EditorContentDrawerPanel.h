// Content Drawer 내부 콘텐츠를 렌더링합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Editor/UI/EditorPanel.h"

#include <array>
#include <filesystem>

class FEditorContentDrawerPanel : public FEditorPanel
{
public:
    virtual void Render(float DeltaTime) override;
    void RequestSearchFocus() { bReclaimSearchFocus = true; }
    void SetCameraShakePanel(class FEditorCameraShakePanel* InCameraShakePanel) { CameraShakePanel = InCameraShakePanel; }

private:
    enum class EContentRoot
    {
        Content,
        Scripts,
    };

    struct FContentItem
    {
        FString Name;
        FString Extension;
        FString RootRelativePath;
        FString ProjectRelativePath;
        std::filesystem::path FullPath;
        bool bDirectory = false;
    };

    void EnsureInitialized();
    void SetCurrentContentDirectory(EContentRoot Root, const std::filesystem::path& Directory);
    void DrawToolbar();
    void DrawSidebar();
    void DrawDirectoryNode(EContentRoot Root, const std::filesystem::path& Directory, const char* Label);
    void DrawAssetArea();
    void DrawBreadcrumb();
    void DrawItemTile(const FContentItem& Item);
    void DrawItemContextMenu(const FContentItem& Item);
    void DrawAssetAreaContextMenu();
    void OpenNewLuaScriptPopup();
    void DrawNewLuaScriptPopup();
    bool CreateLuaScriptFromInput();
    void OpenContentItem(const FContentItem& Item);
    void OpenLuaScriptFile(const std::filesystem::path& ScriptPath);
    void LoadSceneFile(const std::filesystem::path& ScenePath);

    TArray<FContentItem> BuildCurrentItems() const;
    TArray<std::filesystem::path> GetChildDirectories(const std::filesystem::path& Directory) const;
    std::filesystem::path GetRootDirectory(EContentRoot Root) const;
    const char* GetRootLabel(EContentRoot Root) const;
    FString MakeRootRelativePath(EContentRoot Root, const std::filesystem::path& Path) const;
    FString MakeProjectRelativePath(const std::filesystem::path& Path) const;
    bool IsCurrentDirectory(EContentRoot Root, const std::filesystem::path& Directory) const;
    bool MatchesSearch(const FContentItem& Item) const;

private:
    std::array<char, 128> SearchBuf{};
    std::array<char, 128> NewLuaScriptNameBuf{};
    FString NewLuaScriptError;
    TSet<FString> OpenDirectoryKeys;
    EContentRoot CurrentRoot = EContentRoot::Content;
    std::filesystem::path CurrentDirectory;
    bool bInitialized = false;
    bool bReclaimSearchFocus = false;
    bool bOpenNewLuaScriptPopup = false;
    bool bFocusNewLuaScriptName = false;
    class FEditorCameraShakePanel* CameraShakePanel = nullptr;
};
