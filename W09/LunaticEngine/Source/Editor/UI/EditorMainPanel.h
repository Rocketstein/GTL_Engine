#pragma once

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorDetailsWidget.h"
#include "Editor/UI/EditorOutlinerWidget.h"
#include "Editor/UI/EditorStatWidget.h"
#include "Editor/UI/EditorShadowMapDebugWidget.h"
#include "Editor/UI/EditorPlaceActorsWidget.h"
#include "Editor/UI/ContentBrowser/ContentBrowser.h"
#include "Editor/UI/AssetEditor/AssetEditorWidget.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;
struct ImFont;

class FEditorMainPanel
{
public:
	void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine);
	void Release();
	void Render(float DeltaTime);
	void Update();
	void SaveToSettings() const;
	void HideEditorWindows();
	void ShowEditorWindows();
	void SetShowEditorOnlyComponents(bool bEnable) { DetailsWidget.SetShowEditorOnlyComponents(bEnable); }
	bool IsShowingEditorOnlyComponents() const { return DetailsWidget.IsShowingEditorOnlyComponents(); }
	void HideEditorWindowsForPIE();
	void RestoreEditorWindowsAfterPIE();
	void RefreshContentBrowser() { ContentBrowserWidget.Refresh(); }
	void SetContentBrowserIconSize(float Size) { ContentBrowserWidget.SetIconSize(Size); }
	float GetContentBrowserIconSize() const { return ContentBrowserWidget.GetIconSize(); }
	bool IsAssetEditorCapturingInput() const { return AssetEditorWidget.IsCapturingInput(); }

private:
	void RenderMainMenuBar();
	void RenderProjectSettingsWindow();
	void RenderShortcutOverlay();
	void RenderCreditsOverlay();
	void HandleGlobalShortcuts();
	void PackageGameBuild(const char* BatFileName);
	void CookCurrentScene();

	FWindowsWindow* Window = nullptr;
	UEditorEngine* EditorEngine = nullptr;
	ImFont* TitleBarFont = nullptr;
	ImFont* WindowControlIconFont = nullptr;
	FEditorConsoleWidget ConsoleWidget;
	FEditorDetailsWidget DetailsWidget;
	FEditorOutlinerWidget OutlinerWidget;
	FEditorPlaceActorsWidget PlaceActorsWidget;
	FEditorStatWidget StatWidget;
	FEditorContentBrowserWidget ContentBrowserWidget;
	FAssetEditorWidget AssetEditorWidget;
	EditorShadowMapDebugWidget ShadowMapDebugWidget;
	bool bShowWidgetList = false;
	bool bShowShortcutOverlay = false;
	bool bShowCreditsOverlay = false;
	bool bShowProjectSettings = false;
	bool bHideEditorWindows = false;
	bool bHasSavedUIVisibility = false;
	bool bSavedShowWidgetList = false;
	FEditorSettings::FUIVisibility SavedUIVisibility{};
};
