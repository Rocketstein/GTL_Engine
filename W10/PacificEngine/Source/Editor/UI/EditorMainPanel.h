// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.

#pragma once

#include "Engine/Input/ViewportInputRouter.h"
#include "Editor/Logging/EditorLogBuffer.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorBottomBar.h"
#include "Editor/UI/EditorCameraShakePanel.h"
#include "Editor/UI/EditorContentDrawerPanel.h"
#include "Editor/UI/EditorControlPanel.h"
#include "Editor/UI/EditorDetailsPanel.h"
#include "Editor/UI/EditorOutputLogPanel.h"
#include "Editor/UI/EditorStatPanel.h"
#include "Editor/UI/EditorWorldOutlinerPanel.h"
#include "Editor/UI/EditorSkeletalMeshViewerPanel.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;

// FEditorMainPanel는 에디터 UI 표시와 입력 처리를 담당합니다.
class FEditorMainPanel
{
public:
    void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine);
    void Release();
    void Render(float DeltaTime);
    void Update();
    void HandleShortcuts(const FInputSnapshot& Input, FInputSnapshot& InOutViewportInput);
    void HideEditorWindowsForPIE();
    void RestoreEditorWindowsAfterPIE();

	 const FGuiInputCaptureState& GetGuiInputCaptureState() const
    {
        return GuiInputCaptureState;
    }

private:
    void ApplyShortcutKeySuppressions(const FInputSnapshot& Input, FInputSnapshot& InOutViewportInput);
    void SuppressShortcutKey(FInputSnapshot& InOutViewportInput, int32 Key);

private:
    FWindowsWindow* Window = nullptr;
    UEditorEngine* EditorEngine = nullptr;
    FEditorLogBuffer LogBuffer;
    FEditorBottomBar BottomBar;
    FEditorCameraShakePanel CameraShakePanel;
    FEditorContentDrawerPanel ContentDrawerPanel;
    FEditorOutputLogPanel OutputLogPanel;
    FEditorControlPanel ControlPanel;
    FEditorDetailsPanel DetailsPanel;
    FEditorWorldOutlinerPanel WorldOutlinerPanel;
    FEditorStatPanel StatPanel;
    bool bShowPanelList = false;
    bool bHideEditorWindows = false;
    bool bHasSavedUIVisibility = false;
    bool bSavedShowPanelList = false;
    FEditorSettings::FUIVisibility SavedUIVisibility{};

	FGuiInputCaptureState GuiInputCaptureState{};
    bool bSuppressShortcutKeyUntilRelease[256] = {};
};
