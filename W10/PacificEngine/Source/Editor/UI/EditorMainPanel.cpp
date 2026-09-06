// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorMainPanel.h"

#include "Core/CoreGlobals.h"
#include "Core/Logging/LogMacros.h"
#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Renderer.h"
#include "Engine/Input/InputSystem.h"
#include "Component/CameraComponent.h"
#include "Platform/Paths.h"

#include <commdlg.h>
#include <filesystem>

#include "Core/Logging/LogBuffer.h"

namespace
{
constexpr int32 EditorShortcutGraveKey = VK_OEM_3;

bool IsValidVirtualKey(int32 Key)
{
    return Key >= 0 && Key < 256;
}

void ClearKeyForViewport(FInputSnapshot& Input, int32 Key)
{
    if (!IsValidVirtualKey(Key))
    {
        return;
    }

    Input.KeyDown[Key] = false;
    Input.KeyPressed[Key] = false;
    Input.KeyReleased[Key] = false;
}

void SaveEditorSettings()
{
    FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
}

std::string OpenSceneFileDialog(bool bSave)
{
    wchar_t FileName[MAX_PATH] = L"";
    const std::filesystem::path SceneDir = FPaths::SceneDir();

    OPENFILENAMEW Ofn = {};
    Ofn.lStructSize = sizeof(OPENFILENAMEW);
    Ofn.lpstrFilter = L"Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
    Ofn.lpstrFile = FileName;
    Ofn.nMaxFile = MAX_PATH;
    Ofn.lpstrDefExt = L"scene";
    Ofn.lpstrInitialDir = SceneDir.c_str();
    Ofn.Flags = bSave
                    ? (OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR)
                    : (OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR);

    const BOOL bResult = bSave ? GetSaveFileNameW(&Ofn) : GetOpenFileNameW(&Ofn);
    return bResult ? FPaths::FromWide(FileName) : std::string();
}

void SaveSceneFromEditor(UEditorEngine* EditorEngine)
{
    if (!EditorEngine)
    {
        UE_LOG(EditorUI, Warning, "SaveScene requested without editor engine.");
        return;
    }

    const std::string FilePath = OpenSceneFileDialog(true);
    if (FilePath.empty())
    {
        UE_LOG(EditorUI, Debug, "SaveScene dialog canceled.");
        return;
    }

    EditorEngine->StopPlayInEditorImmediate();
    FWorldContext* Ctx = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
    if (!Ctx)
    {
        UE_LOG(EditorUI, Error, "SaveScene failed because active world context is missing.");
        return;
    }

    UCameraComponent* PerspectiveCam = nullptr;
    for (FLevelEditorViewportClient* VC : EditorEngine->GetLevelViewportClients())
    {
        if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective ||
            VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
        {
            PerspectiveCam = VC->GetCamera();
            break;
        }
    }

    std::filesystem::path ScenePath = FPaths::ToPath(FilePath);
    FSceneSaveManager::SaveSceneAsJSON(FPaths::FromPath(ScenePath.stem()), *Ctx, PerspectiveCam);
    UE_LOG(EditorUI, Info, "Scene saved: %s", FilePath.c_str());
}

void LoadSceneFromEditor(UEditorEngine* EditorEngine)
{
    if (!EditorEngine)
    {
        UE_LOG(EditorUI, Warning, "LoadScene requested without editor engine.");
        return;
    }

    const std::string FilePath = OpenSceneFileDialog(false);
    if (FilePath.empty())
    {
        UE_LOG(EditorUI, Debug, "LoadScene dialog canceled.");
        return;
    }

    EditorEngine->StopPlayInEditorImmediate();
    EditorEngine->ClearScene();

    FWorldContext LoadCtx;
    FPerspectiveCameraData CamData;
    FSceneSaveManager::LoadSceneFromJSON(FilePath, LoadCtx, CamData);
    if (LoadCtx.World)
    {
        EditorEngine->GetWorldList().push_back(LoadCtx);
        EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
        EditorEngine->GetSelectionManager().SetWorld(LoadCtx.World);
        LoadCtx.World->WarmupPickingData();
        UE_LOG(EditorUI, Info, "Scene loaded: %s", FilePath.c_str());
    }
    EditorEngine->ResetViewport();

    if (CamData.bValid)
    {
        for (FLevelEditorViewportClient* VC : EditorEngine->GetLevelViewportClients())
        {
            if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective ||
                VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
            {
                if (UCameraComponent* Cam = VC->GetCamera())
                {
                    Cam->SetWorldLocation(CamData.Location);
                    Cam->SetRelativeRotation(CamData.Rotation);
                    FCameraState CS = Cam->GetCameraState();
                    CS.FOV = CamData.FOV;
                    CS.NearZ = CamData.NearClip;
                    CS.FarZ = CamData.FarClip;
                    Cam->SetCameraState(CS);
                }
                break;
            }
        }
    }
}
} // namespace

void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    Window = InWindow;
    EditorEngine = InEditorEngine;
    //GLog = &LogBuffer;

    IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
    IO.FontGlobalScale = 1.05f;
    ImGui::GetStyle().ScaleAllSizes(1.05f);

    ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
    ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

    OutputLogPanel.Initialize(InEditorEngine, &GetGlobalLogBuffer());
    ControlPanel.Initialize(InEditorEngine);
    DetailsPanel.Initialize(InEditorEngine);
    CameraShakePanel.Initialize(InEditorEngine);
    ContentDrawerPanel.Initialize(InEditorEngine);
    ContentDrawerPanel.SetCameraShakePanel(&CameraShakePanel);
    WorldOutlinerPanel.Initialize(InEditorEngine, InRenderer.GetFD3DDevice().GetDevice());
    StatPanel.Initialize(InEditorEngine);
    //SkeletalMeshViewerPanel.Initialize(InEditorEngine, );
    UE_LOG(EditorUI, Info, "Editor main panel initialized.");
}

void FEditorMainPanel::Release()
{
    UE_LOG(EditorUI, Info, "Editor main panel releasing.");
    if (GLog == &LogBuffer)
    {
        GLog = nullptr;
    }
    CameraShakePanel.StopPreview();
    WorldOutlinerPanel.Release();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void FEditorMainPanel::Render(float DeltaTime)
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene"))
            {
                EditorEngine->NewScene();
            }
            if (ImGui::MenuItem("Load Scene..."))
            {
                LoadSceneFromEditor(EditorEngine);
            }
            if (ImGui::MenuItem("Save Scene..."))
            {
                SaveSceneFromEditor(EditorEngine);
            }
            if (ImGui::MenuItem("Open Viewer..."))
            {
                EditorEngine->OpenViewer();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows"))
        {
            FEditorSettings& S = FEditorSettings::Get();
            ImGui::MenuItem("Control Panel", nullptr, &S.UI.bControl);
            ImGui::MenuItem("Details", nullptr, &S.UI.bProperty);
            ImGui::MenuItem("World Outliner", nullptr, &S.UI.bWorldOutliner);
            ImGui::MenuItem("Stat Profiler", nullptr, &S.UI.bStat);
            ImGui::MenuItem("Camera Shake Editor", nullptr, &S.UI.bCameraShakeEditor);
            bool bShadowAtlasDebug = DetailsPanel.IsShadowAtlasDebugWindowOpen();
            if (ImGui::MenuItem("Shadow Atlas", nullptr, &bShadowAtlasDebug))
            {
                DetailsPanel.SetShadowAtlasDebugWindowOpen(bShadowAtlasDebug);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Stat"))
        {
            if (ImGui::MenuItem("Open Stat Profiler"))
            {
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("stat fps"))
            {
                EditorEngine->GetOverlayStatSystem().ShowFPS(true);
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            if (ImGui::MenuItem("stat memory"))
            {
                EditorEngine->GetOverlayStatSystem().ShowMemory(true);
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            if (ImGui::MenuItem("stat shadow"))
            {
                EditorEngine->GetOverlayStatSystem().ShowShadow(true);
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            if (ImGui::MenuItem("stat lightcull"))
            {
                EditorEngine->GetOverlayStatSystem().ShowLightCull(true);
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            if (ImGui::MenuItem("stat none"))
            {
                EditorEngine->GetOverlayStatSystem().HideAll();
                FEditorSettings::Get().UI.bStat = true;
                StatPanel.RequestOpen();
            }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("About"))
        {
            ImGui::OpenPopup("About Editor");
        }

        ImGui::EndMainMenuBar();
    }

    ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(MainViewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::Dummy(ImVec2(320.0f, 0.0f));
        ImGui::Text("Jungle Editor v0.9.0");
        ImGui::Separator();
        ImGui::Text("Contributors");
        ImGui::BulletText("Alex Kim");
        ImGui::BulletText("Mina Park");
        ImGui::BulletText("Chris Lee");
        ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 120.0f) * 0.5f);
        if (ImGui::Button("Close", ImVec2(120.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (EditorEngine)
    {
        SCOPE_STAT_CAT("EditorEngine->RenderViewportUI", "5_UI");
        EditorEngine->RenderViewportUI(DeltaTime);
    }

    const FEditorSettings& Settings = FEditorSettings::Get();

    if (!bHideEditorWindows && Settings.UI.bControl)
    {
        SCOPE_STAT_CAT("ControlPanel.Render", "5_UI");
        ControlPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.UI.bProperty)
    {
        SCOPE_STAT_CAT("DetailsPanel.Render", "5_UI");
        DetailsPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.UI.bWorldOutliner)
    {
        SCOPE_STAT_CAT("WorldOutlinerPanel.Render", "5_UI");
        WorldOutlinerPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.UI.bStat)
    {
        SCOPE_STAT_CAT("StatPanel.Render", "5_UI");
        StatPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.UI.bCameraShakeEditor)
    {
        SCOPE_STAT_CAT("CameraShakePanel.Render", "5_UI");
        CameraShakePanel.Render(DeltaTime);
    }
    else
    {
        CameraShakePanel.StopPreview();
    }

    if (!bHideEditorWindows && DetailsPanel.IsShadowAtlasDebugWindowOpen())
    {
        SCOPE_STAT_CAT("DetailsPanel.RenderShadowAtlasDebugWindow", "5_UI");
        DetailsPanel.RenderShadowAtlasDebugWindow();
    }

    if (!bHideEditorWindows)
    {
        SCOPE_STAT_CAT("EditorDrawer.Render", "5_UI");
        BottomBar.Render(DeltaTime);
        if (BottomBar.BeginDrawerOverlay())
        {
            switch (BottomBar.GetVisibleDrawer())
            {
            case EEditorDrawer::Content:
                ContentDrawerPanel.Render(DeltaTime);
                break;
            case EEditorDrawer::OutputLog:
                OutputLogPanel.RenderContent(DeltaTime);
                break;
            case EEditorDrawer::None:
            default:
                break;
            }
            BottomBar.EndDrawerOverlay();
        }
    }

    if (!bHideEditorWindows && EditorEngine)
    {
        EditorEngine->GetAssetViewerManager().Render(DeltaTime);
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::Update()
{
    ImGuiIO& IO = ImGui::GetIO();

    const bool bWantTextInput = IO.WantTextInput;
    const bool bAnyItemActive = ImGui::IsAnyItemActive();
    const bool bAnyPopupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    const bool bRightMouseDown = IO.MouseDown[1];
    const bool bAnyMouseButtonDown =
        IO.MouseDown[0] ||
        IO.MouseDown[1] ||
        IO.MouseDown[2] ||
        IO.MouseDown[3] ||
        IO.MouseDown[4]; // L / M / R / X1 / X2(그 마우스 옆에 쪼마낳게 달려있는 버튼 두 개)
    const bool bActiveMouseInteraction = bAnyItemActive && bAnyMouseButtonDown;

    bool bWantMouse = IO.WantCaptureMouse;
    bool bWantKeyboard = IO.WantCaptureKeyboard || bWantTextInput;

    if (EditorEngine && EditorEngine->IsMouseOverViewport())
    {
        const bool bViewportKeyboardFocusRequest = !bAnyPopupOpen && bRightMouseDown;

        if (!bAnyPopupOpen && !bActiveMouseInteraction)
        {
            bWantMouse = false;
        }

        if (!bAnyPopupOpen && !bWantTextInput && !bAnyItemActive)
        {
            bWantKeyboard = false;
        }

        if (bViewportKeyboardFocusRequest)
        {
            ImGui::ClearActiveID();
            bWantKeyboard = false;
        }
    }

    GuiInputCaptureState.bMouse = bWantMouse;
    GuiInputCaptureState.bKeyboard = bWantKeyboard;

    if (Window)
    {
        HWND hWnd = Window->GetHWND();
        if (IO.WantTextInput)
        {
            ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
        }
        else
        {
            ImmAssociateContext(hWnd, NULL);
        }
    }
}

void FEditorMainPanel::HandleShortcuts(const FInputSnapshot& Input, FInputSnapshot& InOutViewportInput)
{
    ApplyShortcutKeySuppressions(Input, InOutViewportInput);

    if (bHideEditorWindows)
    {
        return;
    }

    // 이번 국소 패치는 PIE 중 게임 입력과 충돌하지 않도록 에디터 상태에서만 drawer 단축키를 처리합니다.
    if (EditorEngine && EditorEngine->IsPlayingInEditor())
    {
        return;
    }

    const bool bCtrl = Input.Modifiers.bCtrl;
    const bool bAlt = Input.Modifiers.bAlt;
    const bool bShift = Input.Modifiers.bShift;

    if (BottomBar.GetActiveDrawer() == EEditorDrawer::Content && bCtrl && !bAlt && Input.KeyPressed[VK_SPACE])
    {
        BottomBar.CloseDrawer();
        ImGui::ClearActiveID();
        SuppressShortcutKey(InOutViewportInput, VK_SPACE);
        return;
    }

    if (BottomBar.GetActiveDrawer() == EEditorDrawer::OutputLog && bAlt && !bCtrl && Input.KeyPressed[EditorShortcutGraveKey])
    {
        BottomBar.CloseDrawer();
        ImGui::ClearActiveID();
        SuppressShortcutKey(InOutViewportInput, EditorShortcutGraveKey);
        return;
    }

    ImGuiIO& IO = ImGui::GetIO();
    const bool bTextInputActive = IO.WantTextInput;
    const bool bAnyItemActive = ImGui::IsAnyItemActive();
    const bool bAnyPopupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    if (bTextInputActive || bAnyItemActive || bAnyPopupOpen)
    {
        return;
    }

    if (Input.KeyPressed[VK_ESCAPE] && BottomBar.IsDrawerOpen())
    {
        BottomBar.CloseDrawer();
        SuppressShortcutKey(InOutViewportInput, VK_ESCAPE);
        return;
    }

    if (bCtrl && !bAlt && Input.KeyPressed[VK_SPACE])
    {
        BottomBar.ToggleDrawer(EEditorDrawer::Content);
        if (BottomBar.GetActiveDrawer() == EEditorDrawer::Content)
        {
            ContentDrawerPanel.RequestSearchFocus();
        }
        SuppressShortcutKey(InOutViewportInput, VK_SPACE);
        return;
    }

    if (bAlt && !bCtrl && Input.KeyPressed[EditorShortcutGraveKey])
    {
        BottomBar.ToggleDrawer(EEditorDrawer::OutputLog);
        if (BottomBar.GetActiveDrawer() == EEditorDrawer::OutputLog)
        {
            OutputLogPanel.RequestCommandInputFocus();
        }
        SuppressShortcutKey(InOutViewportInput, EditorShortcutGraveKey);
        return;
    }

    if (!bCtrl && !bAlt && !bShift && Input.KeyPressed[EditorShortcutGraveKey])
    {
        BottomBar.OpenDrawer(EEditorDrawer::OutputLog);
        OutputLogPanel.RequestCommandInputFocus();
        SuppressShortcutKey(InOutViewportInput, EditorShortcutGraveKey);
    }
}

void FEditorMainPanel::ApplyShortcutKeySuppressions(const FInputSnapshot& Input, FInputSnapshot& InOutViewportInput)
{
    for (int32 Key = 0; Key < 256; ++Key)
    {
        if (!bSuppressShortcutKeyUntilRelease[Key])
        {
            continue;
        }

        ClearKeyForViewport(InOutViewportInput, Key);

        if (Input.KeyReleased[Key] || !Input.KeyDown[Key])
        {
            bSuppressShortcutKeyUntilRelease[Key] = false;
        }
    }
}

void FEditorMainPanel::SuppressShortcutKey(FInputSnapshot& InOutViewportInput, int32 Key)
{
    if (!IsValidVirtualKey(Key))
    {
        return;
    }

    bSuppressShortcutKeyUntilRelease[Key] = true;
    ClearKeyForViewport(InOutViewportInput, Key);
}

void FEditorMainPanel::HideEditorWindowsForPIE()
{
    if (bHasSavedUIVisibility)
    {
        bHideEditorWindows = true;
        bShowPanelList = false;
        return;
    }

    FEditorSettings& Settings = FEditorSettings::Get();
    SavedUIVisibility = Settings.UI;
    bSavedShowPanelList = bShowPanelList;
    bHasSavedUIVisibility = true;
    bHideEditorWindows = true;
    bShowPanelList = false;

    Settings.UI.bControl = false;
    Settings.UI.bProperty = false;
    Settings.UI.bWorldOutliner = false;
    Settings.UI.bStat = false;
    Settings.UI.bCameraShakeEditor = false;
}

void FEditorMainPanel::RestoreEditorWindowsAfterPIE()
{
    if (!bHasSavedUIVisibility)
    {
        bHideEditorWindows = false;
        return;
    }

    FEditorSettings& Settings = FEditorSettings::Get();
    Settings.UI = SavedUIVisibility;
    bShowPanelList = bSavedShowPanelList;
    bHideEditorWindows = false;
    bHasSavedUIVisibility = false;
}
