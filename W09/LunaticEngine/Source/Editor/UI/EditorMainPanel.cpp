#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Profiling/Timer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Pipeline/Renderer.h"
#include "Engine/Input/InputManager.h"

#include "Editor/UI/ImGuiSetting.h"
#include "Editor/UI/NotificationToast.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/UI/EditorPanelTitleUtils.h"
#include "Core/ProjectSettings.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Editor/UI/EditorFileUtils.h"
#include "GameFramework/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Level.h"
#include "Object/UClass.h"
#include "Core/Notification.h"

#include <filesystem>
#include <windows.h>

namespace
{
	constexpr ImVec4 UnrealPanelSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
	constexpr ImVec4 UnrealPanelSurfaceHover = ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
	constexpr ImVec4 UnrealPanelSurfaceActive = ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f);
	constexpr ImVec4 UnrealDockEmpty = ImVec4(5.0f / 255.0f, 5.0f / 255.0f, 5.0f / 255.0f, 1.0f);
	constexpr ImVec4 UnrealPopupSurface = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
	constexpr ImVec4 UnrealBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
	constexpr ImVec4 PopupSectionHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
	constexpr EOverlayStatType SupportedOverlayStats[] = {
		EOverlayStatType::FPS,
		EOverlayStatType::PickingTime,
		EOverlayStatType::Memory,
		EOverlayStatType::Shadow,
	};
	constexpr const char* CreditsDevelopers[] = {
		"Hojin Lee",
		"HyoBeom Kim",
		"Hyungjun Kim",
		"JunHyeop3631",
		"keonwookang0914",
		"kimhojun",
		"kwonhyeonsoo-goo",
		"LEE SangHoon",
		"lin-ion",
		"Park SangHyeok",
		"Seyoung Park",
		"ShimWoojin",
		"wwonnn",
		"Yonaim",
		"\xEA\xB0\x95\xEA\xB1\xB4\xEC\x9A\xB0",
		"\xEA\xB9\x80\xED\x83\x9C\xED\x98\x84",
		"\xEB\x82\xA8\xEC\x9C\xA4\xEC\xA7\x80",
		"\xEC\xA1\xB0\xED\x98\x84\xEC\x84\x9D",
	};

	void DrawPopupSectionHeader(const char* Label)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PopupSectionHeaderTextColor);
		ImGui::SeparatorText(Label);
		ImGui::PopStyleColor();
	}

	void SetNextPopupWindowPosition(ImGuiCond Condition = ImGuiCond_Appearing)
	{
		if (const ImGuiViewport* MainViewport = ImGui::GetMainViewport())
		{
			const ImVec2 PopupAnchor(
				MainViewport->Pos.x + MainViewport->Size.x * 0.5f,
				MainViewport->Pos.y + MainViewport->Size.y * 0.42f);
			ImGui::SetNextWindowPos(PopupAnchor, Condition, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowViewport(MainViewport->ID);
		}
	}

	bool BeginUtilityPopupWindow(const char* Title, bool* bOpen, const ImVec2& InitialSize, ImGuiCond SizeCondition, ImGuiWindowFlags Flags = 0)
	{
		SetNextPopupWindowPosition(ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(InitialSize, SizeCondition);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_TitleBg, UnrealPanelSurfaceHover);
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UnrealPanelSurfaceHover);
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, UnrealPanelSurfaceHover);
		ImGui::PushStyleColor(ImGuiCol_Border, UnrealBorder);
		const bool bVisible = ImGui::Begin(Title, bOpen, Flags);
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);
		return bVisible;
	}

	bool ConfirmNewScene(HWND OwnerWindowHandle)
	{
		const int32 Result = MessageBoxW(
			OwnerWindowHandle,
			L"Create a new scene?\nUnsaved changes may be lost.",
			L"New Scene",
			MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
		return Result == IDYES;
	}

	void ApplyEditorTabStyle()
	{
		ImGuiStyle& Style = ImGui::GetStyle();
		Style.TabRounding = (std::max)(Style.TabRounding, 9.0f);
		Style.TabBorderSize = (std::max)(Style.TabBorderSize, 1.0f);
		Style.TabBarBorderSize = 0.0f;
		Style.TabBarOverlineSize = 0.0f;
		Style.DockingSeparatorSize = 2.0f;

		Style.Colors[ImGuiCol_Tab] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_TabHovered] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_TabSelected] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_TabDimmed] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_TabDimmedSelected] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_TabSelectedOverline] = UnrealDockEmpty;
		Style.Colors[ImGuiCol_TabDimmedSelectedOverline] = UnrealDockEmpty;
	}

	void ApplyEditorColorTheme()
	{
		ImGuiStyle& Style = ImGui::GetStyle();
		Style.Colors[ImGuiCol_WindowBg] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_ChildBg] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_PopupBg] = UnrealPopupSurface;
		Style.Colors[ImGuiCol_TitleBg] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_TitleBgActive] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_TitleBgCollapsed] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_MenuBarBg] = UnrealDockEmpty;
		Style.Colors[ImGuiCol_FrameBg] = UnrealDockEmpty;
		Style.Colors[ImGuiCol_FrameBgHovered] = UnrealPanelSurfaceHover;
		Style.Colors[ImGuiCol_FrameBgActive] = UnrealPanelSurfaceActive;
		Style.Colors[ImGuiCol_CheckMark] = EditorAccentColor::Value;
		Style.Colors[ImGuiCol_Button] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_ButtonHovered] = UnrealPanelSurfaceHover;
		Style.Colors[ImGuiCol_ButtonActive] = UnrealPanelSurfaceActive;
		Style.Colors[ImGuiCol_Header] = UnrealPanelSurface;
		Style.Colors[ImGuiCol_HeaderHovered] = UnrealPanelSurfaceHover;
		Style.Colors[ImGuiCol_HeaderActive] = UnrealPanelSurfaceActive;
		Style.Colors[ImGuiCol_Separator] = UnrealDockEmpty;
		Style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(12.0f / 255.0f, 12.0f / 255.0f, 12.0f / 255.0f, 1.0f);
		Style.Colors[ImGuiCol_SeparatorActive] = ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f);
		Style.Colors[ImGuiCol_Border] = UnrealBorder;
		Style.Colors[ImGuiCol_DockingEmptyBg] = UnrealDockEmpty;
	}

	FString GetSceneTitleLabel(UEditorEngine* EditorEngine)
	{
		if (!EditorEngine || !EditorEngine->HasCurrentLevelFilePath())
		{
			return "Untitled.Scene";
		}

		const std::filesystem::path ScenePath(FPaths::ToWide(EditorEngine->GetCurrentLevelFilePath()));
		const std::wstring FileName = ScenePath.filename().wstring();
		return FileName.empty() ? FString("Untitled.Scene") : FPaths::ToUtf8(FileName);
	}

	float GetCustomTitleBarHeight()
	{
		return 42.0f;
	}

	float GetWindowOuterPadding()
	{
		return 6.0f;
	}

	float GetWindowCornerRadius()
	{
		return 12.0f;
	}

	float GetWindowTopContentInset(FWindowsWindow* Window)
	{
		(void)Window;
		return 0.0f;
	}

	const char* GetWindowControlIconMinimize()
	{
		return "\xEE\xA4\xA1";
	}

	const char* GetWindowControlIconMaximize()
	{
		return "\xEE\xA4\xA2";
	}

	const char* GetWindowControlIconRestore()
	{
		return "\xEE\xA4\xA3";
	}

	const char* GetWindowControlIconClose()
	{
		return "\xEE\xA2\xBB";
	}

}

void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiSetting::LoadSetting();

	ImGuiIO& IO = ImGui::GetIO();
	IO.IniFilename = "Settings/imgui.ini";
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;
	ApplyEditorColorTheme();
	ApplyEditorTabStyle();

	Window = InWindow;
	EditorEngine = InEditorEngine;

	ImGuiStyle& Style = ImGui::GetStyle();
	Style.WindowPadding.x = (std::max)(Style.WindowPadding.x, 12.0f);
	Style.WindowPadding.y = (std::max)(Style.WindowPadding.y, 10.0f);
	Style.FramePadding.x = (std::max)(Style.FramePadding.x, 8.0f);
	Style.FramePadding.y = (std::max)(Style.FramePadding.y, 5.0f);
	Style.ItemSpacing.x = (std::max)(Style.ItemSpacing.x, 10.0f);
	Style.ItemSpacing.y = (std::max)(Style.ItemSpacing.y, 8.0f);
	Style.CellPadding.x = (std::max)(Style.CellPadding.x, 8.0f);
	Style.CellPadding.y = (std::max)(Style.CellPadding.y, 6.0f);

	const FString FontPath = FResourceManager::Get().ResolvePath(FName("Default.Font.UI"));
	const std::filesystem::path UIFontPath = std::filesystem::path(FPaths::RootDir()) / FPaths::ToWide(FontPath);
	const FString UIFontPathAbsolute = FPaths::ToUtf8(UIFontPath.lexically_normal().wstring());
	IO.Fonts->AddFontFromFileTTF(UIFontPathAbsolute.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
	TitleBarFont = IO.Fonts->AddFontFromFileTTF(UIFontPathAbsolute.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
	EditorPanelTitleUtils::EnsurePanelChromeIconFontLoaded();
	if (std::filesystem::exists("C:/Windows/Fonts/segmdl2.ttf"))
	{
		ImFontConfig IconFontConfig{};
		IconFontConfig.PixelSnapH = true;
		WindowControlIconFont = IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segmdl2.ttf", 13.0f, &IconFontConfig);
	}

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	DetailsWidget.Initialize(InEditorEngine);
	OutlinerWidget.Initialize(InEditorEngine);
	PlaceActorsWidget.Initialize(InEditorEngine);
	StatWidget.Initialize(InEditorEngine);
	AssetEditorWidget.Initialize(InEditorEngine);
	ContentBrowserWidget.Initialize(InEditorEngine, InRenderer.GetFD3DDevice().GetDevice());
	ShadowMapDebugWidget.Initialize(InEditorEngine);
}

void FEditorMainPanel::Release()
{
	AssetEditorWidget.Shutdown();
	ConsoleWidget.Shutdown();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::SaveToSettings() const
{
	ContentBrowserWidget.SaveToSettings();
}

void FEditorMainPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	EditorPanelTitleUtils::BeginPanelDecorationFrame();

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const float TitleBarHeight = GetCustomTitleBarHeight();
	const float TopFrameInset = GetWindowTopContentInset(Window);
	const float OuterPadding = GetWindowOuterPadding();
	const float CornerRadius = GetWindowCornerRadius();
	const ImVec2 ViewportMin = MainViewport->Pos;
	const ImVec2 ViewportMax(MainViewport->Pos.x + MainViewport->Size.x, MainViewport->Pos.y + MainViewport->Size.y);
	const ImVec2 FrameMin(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + OuterPadding);
	const ImVec2 FrameMax(MainViewport->Pos.x + MainViewport->Size.x - OuterPadding, MainViewport->Pos.y + MainViewport->Size.y - OuterPadding);
	ImDrawList* BackgroundDrawList = ImGui::GetBackgroundDrawList(const_cast<ImGuiViewport*>(MainViewport));
	BackgroundDrawList->AddRectFilled(ViewportMin, ViewportMax, IM_COL32(5, 5, 5, 255));
	BackgroundDrawList->AddRectFilled(FrameMin, FrameMax, IM_COL32(5, 5, 5, 255), CornerRadius);

	RenderMainMenuBar();

	ImGuiWindowClass DockspaceWindowClass{};
	DockspaceWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + TitleBarHeight + OuterPadding), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(MainViewport->Size.x - OuterPadding * 2.0f, MainViewport->Size.y - TopFrameInset - TitleBarHeight - OuterPadding * 2.0f), ImGuiCond_Always);
	ImGui::SetNextWindowViewport(MainViewport->ID);
	ImGuiWindowFlags DockspaceWindowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 6.0f));
	if (ImGui::Begin("##EditorDockSpaceHost", nullptr, DockspaceWindowFlags))
	{
		ImGui::DockSpace(ImGui::GetID("##EditorDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &DockspaceWindowClass);
	}
	ImGui::End();
	ImGui::PopStyleVar(4);

	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)
	const FEditorSettings& Settings = FEditorSettings::Get();
	if (EditorEngine && Settings.UI.bViewport)
	{
		SCOPE_STAT_CAT("EditorEngine->RenderViewportUI", "5_UI");
		EditorEngine->RenderViewportUI(DeltaTime);

		if (FLevelEditorViewportClient* ActiveViewport = EditorEngine->GetActiveViewport())
		{
			EditorEngine->GetOverlayStatSystem().RenderImGui(*EditorEngine, ActiveViewport->GetViewportScreenRect());
		}
	}

	if (!bHideEditorWindows && Settings.UI.bImGUISettings)
	{
		ImGuiSetting::ShowSetting();
	}

	if (!bHideEditorWindows && Settings.UI.bConsole)
	{
		SCOPE_STAT_CAT("ConsoleWidget.Render", "5_UI");
		ConsoleWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bProperty)
	{
		SCOPE_STAT_CAT("DetailsWidget.Render", "5_UI");
		DetailsWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bScene)
	{
		SCOPE_STAT_CAT("OutlinerWidget.Render", "5_UI");
		OutlinerWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bPlaceActors)
	{
		SCOPE_STAT_CAT("PlaceActorsWidget.Render", "5_UI");
		PlaceActorsWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bStat)
	{
		SCOPE_STAT_CAT("StatWidget.Render", "5_UI");
		StatWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bContentBrowser)
	{
		SCOPE_STAT_CAT("ContentBrowserWidget.Render", "5_UI");
		ContentBrowserWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && AssetEditorWidget.IsOpen())
	{
		SCOPE_STAT_CAT("AssetEditorWidget.Render", "5_UI");
		AssetEditorWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bShadowMapDebug)
	{
		ShadowMapDebugWidget.Render(DeltaTime);
	}

	RenderProjectSettingsWindow();
	if (EditorEngine)
	{
		EditorEngine->RenderPIEOverlayPopups();
	}

	RenderShortcutOverlay();
	RenderCreditsOverlay();
	EditorPanelTitleUtils::FlushPanelDecorations();

	// 토스트 알림 (항상 최상위에 표시)
	FNotificationToast::Render();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::RenderMainMenuBar()
{
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const float TitleBarHeight = GetCustomTitleBarHeight();
	const float TopFrameInset = GetWindowTopContentInset(Window);
	const float OuterPadding = GetWindowOuterPadding();
	const float CornerRadius = GetWindowCornerRadius();
	const float LogoSize = 36.0f;
	const float ButtonWidth = 38.0f;
	const float WindowControlHeight = 24.0f;
	const float ButtonSpacing = 2.0f;
	const float RightControlsWidth = ButtonWidth * 3.0f + ButtonSpacing * 2.0f;
	const float TitleBarPaddingY = 2.0f;
	const float LeftContentInset = 8.0f;

	ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + OuterPadding), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(MainViewport->Size.x - OuterPadding * 2.0f, TitleBarHeight), ImGuiCond_Always);
	ImGui::SetNextWindowViewport(MainViewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f + LeftContentInset, TitleBarPaddingY));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
	const ImGuiWindowFlags TitleBarFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;
	if (!ImGui::Begin("##EditorCustomTitleBar", nullptr, TitleBarFlags))
	{
		ImGui::End();
		ImGui::PopStyleVar(4);
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();
	ID3D11ShaderResourceView* LogoTexture = FResourceManager::Get().FindLoadedTexture(
		FResourceManager::Get().ResolvePath(FName("Editor.Icon.AppLogo"))).Get();

	float MenuEndX = 54.0f;
	if (ImGui::BeginMenuBar())
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 7.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
		if (TitleBarFont)
		{
			ImGui::PushFont(TitleBarFont);
		}
		const FString SceneTabLabel = GetSceneTitleLabel(EditorEngine);
		FString ScenePathTooltip = "Unsaved Scene";
		if (EditorEngine && EditorEngine->HasCurrentLevelFilePath())
		{
			ScenePathTooltip = EditorEngine->GetCurrentLevelFilePath();
		}
		const float SceneTabWidth = ImGui::CalcTextSize(SceneTabLabel.c_str()).x + 34.0f;
		const float MenuFrameHeight = ImGui::GetFrameHeight();
		const float SceneTabHeight = MenuFrameHeight;
		const float MaxContentHeight = (std::max)((std::max)(MenuFrameHeight, SceneTabHeight), (std::max)(WindowControlHeight, LogoSize));
		const float ContentStartY = (std::max)(0.0f, floorf((TitleBarHeight - MaxContentHeight) * 0.5f));
		const float RightControlsStartX = ImGui::GetWindowWidth() - RightControlsWidth;
		const float SceneTabX = RightControlsStartX - SceneTabWidth - 12.0f;
		float MenuStartX = ImGui::GetStyle().WindowPadding.x;

		if (LogoTexture)
		{
			const float LogoX = 8.0f;
			const float LogoY = ContentStartY;
			ImDrawList* DrawList = ImGui::GetForegroundDrawList(const_cast<ImGuiViewport*>(MainViewport));
			const ImVec2 WindowPos = ImGui::GetWindowPos();
			DrawList->AddImage(
				LogoTexture,
				ImVec2(WindowPos.x + LogoX, WindowPos.y + LogoY),
				ImVec2(WindowPos.x + LogoX + LogoSize, WindowPos.y + LogoY + LogoSize));
			MenuStartX = LogoX + LogoSize + 10.0f;
		}

		ImGui::SetCursorPos(ImVec2(MenuStartX, ContentStartY));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, UnrealPanelSurface);
		ImGui::PushStyleColor(ImGuiCol_Header, UnrealPanelSurface);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorAccentColor::Value);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorAccentColor::Value);

		if (ImGui::BeginMenu("File"))
		{
			DrawPopupSectionHeader("SCENE");
			if (ImGui::MenuItem("New Scene", "Ctrl+N") && EditorEngine)
			{
				HWND OwnerWindowHandle = Window ? Window->GetHWND() : nullptr;
				if (ConfirmNewScene(OwnerWindowHandle))
				{
					EditorEngine->NewScene();
				}
			}
			if (ImGui::MenuItem("Open Scene...", "Ctrl+O") && EditorEngine)
			{
				EditorEngine->LoadSceneWithDialog();
			}
			if (ImGui::MenuItem("Save Scene", "Ctrl+S") && EditorEngine)
			{
				EditorEngine->SaveScene();
			}
			if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S") && EditorEngine)
			{
				EditorEngine->RequestSaveSceneAsDialog();
			}

			DrawPopupSectionHeader("ASSET");
			if (ImGui::MenuItem("New UAsset..."))
			{
				AssetEditorWidget.CreateCameraShakeAsset();
			}
			if (ImGui::MenuItem("Open UAsset...", "Ctrl+Alt+O"))
			{
				AssetEditorWidget.OpenAssetWithDialog(Window ? Window->GetHWND() : nullptr);
			}

			DrawPopupSectionHeader("IMPORT");
			if (ImGui::MenuItem("Import Material...") && EditorEngine)
			{
				EditorEngine->ImportMaterialWithDialog();
			}
			if (ImGui::MenuItem("Import Texture...") && EditorEngine)
			{
				EditorEngine->ImportTextureWithDialog();
			}
			DrawPopupSectionHeader("COOK");
			if (ImGui::MenuItem("Cook Current Scene") && EditorEngine)
			{
				CookCurrentScene();
			}
			if (ImGui::MenuItem("Cook All Scenes"))
			{
				const int32 Count = FSceneSaveManager::CookAllScenes();
				FNotificationManager::Get().AddNotification(
					std::string("Cooked ") + std::to_string(Count) + " scenes",
					Count > 0 ? ENotificationType::Success : ENotificationType::Error);
			}
			DrawPopupSectionHeader("PACKAGE");
			if (ImGui::MenuItem("Package: Release..."))
			{
				PackageGameBuild("ReleaseBuild.bat");
			}
			if (ImGui::MenuItem("Package: Shipping..."))
			{
				PackageGameBuild("ShippingBuild.bat");
			}
			if (ImGui::MenuItem("Package: Demo..."))
			{
				PackageGameBuild("DemoBuild.bat");
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			const bool bCanUndo = EditorEngine && EditorEngine->CanUndoTransformChange();
			const bool bCanRedo = EditorEngine && EditorEngine->CanRedoTransformChange();
			if (!bCanUndo) ImGui::BeginDisabled();
			if (ImGui::MenuItem("Undo", "Ctrl+Z") && EditorEngine) EditorEngine->UndoTrackedTransformChange();
			if (!bCanUndo) ImGui::EndDisabled();
			if (!bCanRedo) ImGui::BeginDisabled();
			if (ImGui::MenuItem("Redo", "Ctrl+Y") && EditorEngine) EditorEngine->RedoTrackedTransformChange();
			if (!bCanRedo) ImGui::EndDisabled();
			ImGui::Separator();
			if (ImGui::MenuItem("Project Settings..."))
			{
				bShowProjectSettings = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			ImGui::Checkbox("Viewport", &Settings.UI.bViewport);
			ImGui::Separator();
			ImGui::Checkbox("Console", &Settings.UI.bConsole);
			ImGui::Checkbox("Details", &Settings.UI.bProperty);
			ImGui::Checkbox("Outliner", &Settings.UI.bScene);
			ImGui::Checkbox("Place Actors", &Settings.UI.bPlaceActors);
			ImGui::Checkbox("Stat Profiler", &Settings.UI.bStat);
			ImGui::Checkbox("Content Browser", &Settings.UI.bContentBrowser);
			ImGui::Checkbox("Shadow Map Debug", &Settings.UI.bShadowMapDebug);
			ImGui::Separator();
			ImGui::Checkbox("ImGuiSetting", &Settings.UI.bImGUISettings);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Stat"))
		{
			if (EditorEngine)
			{
				FOverlayStatSystem& OverlayStats = EditorEngine->GetOverlayStatSystem();
				for (EOverlayStatType StatType : SupportedOverlayStats)
				{
					bool bVisible = OverlayStats.IsStatVisible(StatType);
					if (ImGui::MenuItem(FOverlayStatSystem::GetStatDisplayName(StatType), nullptr, bVisible))
					{
						OverlayStats.SetStatVisible(StatType, !bVisible);
					}
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Hide All"))
				{
					OverlayStats.HideAll();
				}
			}
			else
			{
				ImGui::BeginDisabled();
				for (EOverlayStatType StatType : SupportedOverlayStats)
				{
					ImGui::MenuItem(FOverlayStatSystem::GetStatDisplayName(StatType), nullptr, false, false);
				}
				ImGui::Separator();
				ImGui::MenuItem("Hide All", nullptr, false, false);
				ImGui::EndDisabled();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("Shortcut Help"))
			{
				bShowShortcutOverlay = !bShowShortcutOverlay;
			}
			if (ImGui::MenuItem("Credits"))
			{
				bShowCreditsOverlay = !bShowCreditsOverlay;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Levels"))
		{
			UWorld* World = EditorEngine->GetWorld();
			if (World)
			{
				ULevel* Persistent = World->GetPersistentLevel();
				FString PersistentName = Persistent ? "Persistent Level" : "No Persistent Level";
				bool bIsPersistentCurrent = (World->GetCurrentLevel() == Persistent);
				if (ImGui::MenuItem(PersistentName.c_str(), nullptr, bIsPersistentCurrent))
				{
					World->SetCurrentLevel(Persistent);
				}

				ImGui::Separator();
				ImGui::TextDisabled("Streaming Levels");

				for (const auto& Info : World->GetStreamingLevels())
				{
					bool bIsCurrent = (World->GetCurrentLevel() == Info.LoadedLevel);
					FString DisplayName = Info.LevelName.ToString() + (Info.bIsLoaded ? "" : " (Unloaded)");

					if (ImGui::MenuItem(DisplayName.c_str(), nullptr, bIsCurrent))
					{
						if (Info.LoadedLevel) World->SetCurrentLevel(Info.LoadedLevel);
					}

					if (ImGui::BeginPopupContextItem())
					{
						if (!Info.bIsLoaded)
						{
							if (ImGui::MenuItem("Load Level")) World->LoadStreamingLevel(Info.LevelPath);
						}
						else
						{
							if (ImGui::MenuItem("Unload Level")) World->UnloadStreamingLevel(Info.LevelName);
						}
						ImGui::EndPopup();
					}
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Add Existing Level..."))
				{
					const std::wstring InitialDir = FSceneSaveManager::GetSceneDirectory();
					const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
						.Filter = L"Level Files (*.umap)\0*.umap\0",
						.Title = L"Add Existing Level",
						.InitialDirectory = InitialDir.c_str(),
						.OwnerWindowHandle = Window ? Window->GetHWND() : nullptr,
						.bFileMustExist = true,
						.bPathMustExist = true,
						.bPromptOverwrite = false,
						.bReturnRelativeToProjectRoot = false,
						});
					if (!SelectedPath.empty())
					{
						World->AddStreamingLevel(SelectedPath);
					}
				}

				if (Persistent && ImGui::BeginMenu("GameMode Override"))
				{
					const TArray<UClass*> Candidates = UClass::GetSubclassesOf(AGameModeBase::StaticClass());
					const FString CurrentName = Persistent->GetGameModeClassName();

					if (ImGui::MenuItem("(Use Project Default)", nullptr, CurrentName.empty()))
					{
						Persistent->SetGameModeClassName("");
					}
					ImGui::Separator();
					for (UClass* C : Candidates)
					{
						const bool bSelected = (CurrentName == C->GetName());
						if (ImGui::MenuItem(C->GetName(), nullptr, bSelected))
						{
							Persistent->SetGameModeClassName(C->GetName());
						}
					}
					ImGui::EndMenu();
				}
			}
			ImGui::EndMenu();
		}
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(4);

		MenuEndX = ImGui::GetCursorPosX();

		ImGui::SetCursorPos(ImVec2(SceneTabX, ContentStartY));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.74f, 1.0f));
		ImGui::Button(SceneTabLabel.c_str(), ImVec2(SceneTabWidth, SceneTabHeight));
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(520.0f);
			ImGui::Text("Current: %s", ScenePathTooltip.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);

		if (Window)
		{
			ImGui::SetCursorPos(ImVec2(RightControlsStartX, ContentStartY));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.24f, 0.26f, 1.0f));
			if (WindowControlIconFont)
			{
				ImGui::PushFont(WindowControlIconFont);
			}
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.58f));
			if (ImGui::Button(GetWindowControlIconMinimize(), ImVec2(ButtonWidth, WindowControlHeight)))
			{
				Window->Minimize();
			}
			ImGui::PopStyleVar();
			ImGui::SameLine(0.0f, ButtonSpacing);
			if (ImGui::Button(Window->IsWindowMaximized() ? GetWindowControlIconRestore() : GetWindowControlIconMaximize(), ImVec2(ButtonWidth, WindowControlHeight)))
			{
				Window->ToggleMaximize();
			}
			ImGui::SameLine(0.0f, ButtonSpacing);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.16f, 0.16f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.58f, 0.10f, 0.10f, 1.0f));
			if (ImGui::Button(GetWindowControlIconClose(), ImVec2(ButtonWidth, WindowControlHeight)))
			{
				Window->Close();
			}
			if (WindowControlIconFont)
			{
				ImGui::PopFont();
			}
			ImGui::PopStyleColor(2);
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(2);
		}

		if (TitleBarFont)
		{
			ImGui::PopFont();
		}
		ImGui::PopStyleVar(4);
		ImGui::EndMenuBar();
	}

	const float SceneTabWidth = ImGui::CalcTextSize(GetSceneTitleLabel(EditorEngine).c_str()).x + 34.0f;
	const float DragRegionStartX = MenuEndX + 8.0f;
	const float DragRegionEndX = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - RightControlsWidth - SceneTabWidth - 20.0f;
	const float DragRegionWidth = DragRegionEndX - DragRegionStartX;
	const float TitleBarClientOriginX = OuterPadding;
	const float TitleBarClientOriginY = TopFrameInset + OuterPadding;
	const float TitleBarControlRegionX = TitleBarClientOriginX + (ImGui::GetWindowWidth() - RightControlsWidth);
	const float TitleBarControlRegionY = TitleBarClientOriginY + floorf((TitleBarHeight - WindowControlHeight) * 0.5f);

	if (Window && DragRegionWidth > 24.0f)
	{
		Window->SetTitleBarDragRegion(
			TitleBarClientOriginX + DragRegionStartX,
			TitleBarClientOriginY,
			DragRegionWidth,
			TitleBarHeight);
		Window->SetTitleBarControlRegion(
			TitleBarControlRegionX,
			TitleBarControlRegionY,
			RightControlsWidth,
			WindowControlHeight);
	}
	else if (Window)
	{
		Window->ClearTitleBarDragRegion();
		Window->ClearTitleBarControlRegion();
	}

	ImGui::End();
	ImGui::PopStyleVar(4);
}

void FEditorMainPanel::RenderProjectSettingsWindow()
{
	if (!bShowProjectSettings)
	{
		return;
	}

	if (!BeginUtilityPopupWindow(
		"Project Settings",
		&bShowProjectSettings,
		ImVec2(560.0f, 460.0f),
		ImGuiCond_Appearing))
	{
		ImGui::End();
		return;
	}

	FProjectSettings& ProjectSettings = FProjectSettings::Get();

	auto DrawClassDropdown = [](const char* Label, UClass* BaseClass, FString& InOutValue)
	{
		const TArray<UClass*> Candidates = UClass::GetSubclassesOf(BaseClass);
		const char* Preview = InOutValue.empty() ? "(none)" : InOutValue.c_str();
		if (ImGui::BeginCombo(Label, Preview))
		{
			for (UClass* C : Candidates)
			{
				const bool bSelected = (InOutValue == C->GetName());
				if (ImGui::Selectable(C->GetName(), bSelected))
				{
					InOutValue = C->GetName();
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	};

	DrawPopupSectionHeader("WINDOW");
	ImGui::InputScalar("Window Width", ImGuiDataType_U32, &ProjectSettings.Game.WindowWidth);
	ImGui::InputScalar("Window Height", ImGuiDataType_U32, &ProjectSettings.Game.WindowHeight);
	ImGui::Checkbox("Lock Resolution", &ProjectSettings.Game.bLockWindowResolution);
	if (ProjectSettings.Game.WindowWidth < 320) ProjectSettings.Game.WindowWidth = 320;
	if (ProjectSettings.Game.WindowHeight < 240) ProjectSettings.Game.WindowHeight = 240;

	ImGui::PushStyleColor(ImGuiCol_Button, EditorAccentColor::WithAlpha(0.92f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorAccentColor::Value);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorAccentColor::Value);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.97f, 0.98f, 1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 8.0f));
	if (ImGui::Button("Apply Resolution"))
	{
		ProjectSettings.SaveToFile(FProjectSettings::GetDefaultPath());
		if (Window)
		{
			Window->ResizeClientArea(ProjectSettings.Game.WindowWidth, ProjectSettings.Game.WindowHeight);
		}
	}
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(4);
	ImGui::SameLine();
	ImGui::TextDisabled(ProjectSettings.Game.bLockWindowResolution
		? "Packaged game resize is locked. The editor window stays resizable."
		: "The editor stays resizable. Packaged game uses this size on next launch.");

	

	DrawPopupSectionHeader("PERFORMANCE");
	bool bPerformanceChanged = false;
	bPerformanceChanged |= ImGui::Checkbox("Limit FPS", &ProjectSettings.Performance.bLimitFPS);
	ImGui::BeginDisabled(!ProjectSettings.Performance.bLimitFPS);
	bPerformanceChanged |= ImGui::InputScalar("Max FPS", ImGuiDataType_U32, &ProjectSettings.Performance.MaxFPS);
	ImGui::EndDisabled();
	if (ProjectSettings.Performance.MaxFPS == 0)
	{
		ProjectSettings.Performance.MaxFPS = 1;
	}
	else if (ProjectSettings.Performance.MaxFPS > 1000)
	{
		ProjectSettings.Performance.MaxFPS = 1000;
	}
	if (bPerformanceChanged && GEngine && GEngine->GetTimer())
	{
		GEngine->GetTimer()->SetMaxFPS(
			ProjectSettings.Performance.bLimitFPS
			? static_cast<float>(ProjectSettings.Performance.MaxFPS)
			: 0.0f);
	}

	DrawPopupSectionHeader("SHADOW");
	ImGui::Checkbox("Enable Shadows", &ProjectSettings.Shadow.bEnabled);
	ImGui::InputScalar("CSM Resolution", ImGuiDataType_U32, &ProjectSettings.Shadow.CSMResolution);
	ImGui::InputScalar("Spot Atlas Resolution", ImGuiDataType_U32, &ProjectSettings.Shadow.SpotAtlasResolution);
	ImGui::InputScalar("Point Atlas Resolution", ImGuiDataType_U32, &ProjectSettings.Shadow.PointAtlasResolution);
	ImGui::InputScalar("Max Spot Atlas Pages", ImGuiDataType_U32, &ProjectSettings.Shadow.MaxSpotAtlasPages);
	ImGui::InputScalar("Max Point Atlas Pages", ImGuiDataType_U32, &ProjectSettings.Shadow.MaxPointAtlasPages);

	DrawPopupSectionHeader("LIGHT CULLING");
	int32 LightCullingMode = static_cast<int32>(ProjectSettings.LightCulling.Mode);
	ImGui::RadioButton("Off", &LightCullingMode, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Tile", &LightCullingMode, 1);
	ImGui::SameLine();
	ImGui::RadioButton("Cluster", &LightCullingMode, 2);
	ProjectSettings.LightCulling.Mode = static_cast<uint32>(LightCullingMode);
	ImGui::SliderFloat("Heat Map Max", &ProjectSettings.LightCulling.HeatMapMax, 1.0f, 100.0f, "%.0f");
	ImGui::Checkbox("Enable 2.5D Culling", &ProjectSettings.LightCulling.bEnable25DCulling);

	DrawPopupSectionHeader("SCENE DEPTH");
	int32 SceneDepthMode = static_cast<int32>(ProjectSettings.SceneDepth.Mode);
	ImGui::Combo("Mode", &SceneDepthMode, "Power\0Linear\0");
	ProjectSettings.SceneDepth.Mode = static_cast<uint32>(SceneDepthMode);
	ImGui::SliderFloat("Exponent", &ProjectSettings.SceneDepth.Exponent, 1.0f, 512.0f, "%.0f");

	DrawPopupSectionHeader("GAME");
	DrawClassDropdown("GameInstance Class", UGameInstance::StaticClass(), ProjectSettings.Game.GameInstanceClass);
	DrawClassDropdown("Default GameMode Class", AGameModeBase::StaticClass(), ProjectSettings.Game.DefaultGameModeClass);

	// Default Map
	{
		const TArray<FString> Scenes = FSceneSaveManager::GetSceneFileList();
		const char* Preview = ProjectSettings.Game.DefaultScene.empty() ? "(none)" : ProjectSettings.Game.DefaultScene.c_str();
		if (ImGui::BeginCombo("Default Map", Preview))
		{
			for (const FString& Stem : Scenes)
			{
				const bool bSelected = (ProjectSettings.Game.DefaultScene == Stem);
				if (ImGui::Selectable(Stem.c_str(), bSelected))
				{
					ProjectSettings.Game.DefaultScene = Stem;
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}
	ImGui::TextDisabled("(GameInstance class change requires restart)");

	if (ImGui::Button("Save"))
	{
		ProjectSettings.SaveToFile(FProjectSettings::GetDefaultPath());
	}
	ImGui::SameLine();
	if (ImGui::Button("Close"))
	{
		bShowProjectSettings = false;
	}

	ImGui::End();
}

void FEditorMainPanel::RenderShortcutOverlay()
{
	if (!bShowShortcutOverlay)
	{
		return;
	}

	if (!BeginUtilityPopupWindow(
		"Shortcut Help",
		&bShowShortcutOverlay,
		ImVec2(320.0f, 150.0f),
		ImGuiCond_Appearing,
		ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("File");
	ImGui::Separator();
	ImGui::TextUnformatted("Ctrl+N : New Scene");
	ImGui::TextUnformatted("Ctrl+O : Open Scene");
	ImGui::TextUnformatted("Ctrl+S : Save Scene");
	ImGui::TextUnformatted("Ctrl+Shift+S : Save Scene As");
	ImGui::TextUnformatted("Ctrl+Z : Undo Scene Change");
	ImGui::TextUnformatted("Ctrl+Y : Redo Scene Change");
	ImGui::TextUnformatted("` : Toggle Console");
	ImGui::TextUnformatted("Ctrl+Space : Toggle Content Browser");
	ImGui::Separator();
	ImGui::TextUnformatted("F : Focus on selection");
	ImGui::TextUnformatted("Ctrl + LMB : Multi Picking (Toggle)");
	ImGui::TextUnformatted("Ctrl + Alt + LMB Drag : Area Selection");

	ImGui::End();
}

void FEditorMainPanel::RenderCreditsOverlay()
{
	if (!bShowCreditsOverlay)
	{
		return;
	}

	if (!BeginUtilityPopupWindow(
		"Credits",
		&bShowCreditsOverlay,
		ImVec2(420.0f, 560.0f),
		ImGuiCond_Appearing,
		ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	ID3D11ShaderResourceView* CreditsTexture = FResourceManager::Get().FindLoadedTexture("Asset/Editor/App/lunatic_icon.png").Get();
	if (!CreditsTexture)
	{
		CreditsTexture = FResourceManager::Get().FindLoadedTexture(
			FResourceManager::Get().ResolvePath(FName("Editor.Icon.AppLogo"))).Get();
	}

	if (CreditsTexture)
	{
		constexpr float ImageSize = 180.0f;
		const float CursorX = (ImGui::GetContentRegionAvail().x - ImageSize) * 0.5f;
		ImGui::SetCursorPosX((std::max)(CursorX, 0.0f));
		ImGui::Image(CreditsTexture, ImVec2(ImageSize, ImageSize));
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	const float TitleWidth = ImGui::CalcTextSize("Developers").x;
	ImGui::SetCursorPosX((std::max)((ImGui::GetContentRegionAvail().x - TitleWidth) * 0.5f, 0.0f));
	ImGui::TextUnformatted("Developers");
	ImGui::Spacing();

	for (const char* Developer : CreditsDevelopers)
	{
		const float NameWidth = ImGui::CalcTextSize(Developer).x;
		ImGui::SetCursorPosX((std::max)((ImGui::GetContentRegionAvail().x - NameWidth) * 0.5f, 0.0f));
		ImGui::TextUnformatted(Developer);
	}

	ImGui::End();
}

void FEditorMainPanel::Update()
{
	HandleGlobalShortcuts();

	ImGuiIO& IO = ImGui::GetIO();

	// 뷰포트 슬롯 위에서는 bUsingMouse를 해제해야 TickInteraction이 동작
	bool bWantMouse = IO.WantCaptureMouse;
	bool bWantKeyboard = IO.WantCaptureKeyboard || bShowShortcutOverlay || bShowCreditsOverlay;
	const bool bAssetEditorCapturingInput = AssetEditorWidget.IsCapturingInput();
	const bool bPIEPopupOpen = EditorEngine && EditorEngine->IsScoreSavePopupOpen();
	if (bPIEPopupOpen)
	{
		bWantMouse = true;
		bWantKeyboard = true;
	}

	if (EditorEngine && EditorEngine->IsMouseOverViewport() && !bAssetEditorCapturingInput)
	{
		if (!bPIEPopupOpen)
		{
			bWantMouse = false;
			if (!IO.WantTextInput && !bShowShortcutOverlay && !bShowCreditsOverlay)
			{
				bWantKeyboard = false;
			}
		}
	}
	FInputManager::Get().SetGuiCaptureOverride(bWantMouse, bWantKeyboard, IO.WantTextInput);

	// IME는 ImGui가 텍스트 입력을 원할 때만 활성화.
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

void FEditorMainPanel::HandleGlobalShortcuts()
{
	if (!EditorEngine)
	{
		return;
	}
	if (EditorEngine->IsPIEPossessedMode())
	{
		return;
	}

	ImGuiIO& IO = ImGui::GetIO();
	if (IO.WantTextInput)
	{
		return;
	}

	FInputManager& Input = FInputManager::Get();
	FEditorSettings& Settings = FEditorSettings::Get();

	if (Input.IsKeyPressed(VK_OEM_3))
	{
		Settings.UI.bConsole = !Settings.UI.bConsole;
		return;
	}

	if (!Input.IsKeyDown(VK_CONTROL))
	{
		return;
	}

	const bool bShift = Input.IsKeyDown(VK_SHIFT);
	if (Input.IsKeyPressed(VK_SPACE))
	{
		Settings.UI.bContentBrowser = !Settings.UI.bContentBrowser;
		return;
	}

	if (Input.IsKeyPressed('N'))
	{
		EditorEngine->NewScene();
	}
	else if (Input.IsKeyPressed('O'))
	{
		EditorEngine->LoadSceneWithDialog();
	}
	else if (Input.IsKeyPressed('S'))
	{
		if (bShift)
		{
			EditorEngine->RequestSaveSceneAsDialog();
		}
		else
		{
			EditorEngine->SaveScene();
		}
	}
	else if (Input.IsKeyPressed('Z'))
	{
		EditorEngine->UndoTrackedTransformChange();
	}
	else if (Input.IsKeyPressed('Y'))
	{
		EditorEngine->RedoTrackedTransformChange();
	}
}

void FEditorMainPanel::HideEditorWindows()
{
	if (bHasSavedUIVisibility)
	{
		bHideEditorWindows = true;
		bShowWidgetList = false;
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();
	SavedUIVisibility = Settings.UI;
	bSavedShowWidgetList = bShowWidgetList;
	bHasSavedUIVisibility = true;
	bHideEditorWindows = true;
	bShowWidgetList = false;

	Settings.UI.bConsole = false;
	Settings.UI.bProperty = false;
	Settings.UI.bScene = false;
	Settings.UI.bPlaceActors = false;
	Settings.UI.bStat = false;
	Settings.UI.bContentBrowser = false;
	Settings.UI.bImGUISettings = false;
	Settings.UI.bShadowMapDebug = false;
}

void FEditorMainPanel::ShowEditorWindows()
{
	if (!bHasSavedUIVisibility)
	{
		bHideEditorWindows = false;
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();
	Settings.UI = SavedUIVisibility;
	bShowWidgetList = bSavedShowWidgetList;
	bHideEditorWindows = false;
	bHasSavedUIVisibility = false;
}

void FEditorMainPanel::HideEditorWindowsForPIE()
{
	HideEditorWindows();
}

void FEditorMainPanel::RestoreEditorWindowsAfterPIE()
{
	ShowEditorWindows();
}

void FEditorMainPanel::CookCurrentScene()
{
	if (!EditorEngine || !EditorEngine->HasCurrentLevelFilePath())
	{
		FNotificationManager::Get().AddNotification(
			"Cook: save the current scene first.",
			ENotificationType::Error);
		return;
	}

	const FString& InPath = EditorEngine->GetCurrentLevelFilePath();
	std::filesystem::path Out(FPaths::ToWide(InPath));
	Out.replace_extension(L".umap");
	const FString OutPath = FPaths::ToUtf8(Out.wstring());

	const bool bOk = FSceneSaveManager::CookSceneToBinary(InPath, OutPath);
	FNotificationManager::Get().AddNotification(
		bOk ? std::string("Cooked: ") + Out.filename().string()
			: std::string("Cook failed: ") + Out.filename().string(),
		bOk ? ENotificationType::Success : ENotificationType::Error);
}

void FEditorMainPanel::PackageGameBuild(const char* BatFileName)
{
	// 솔루션 루트(.bat 위치)를 찾는다 — 후보 경로를 차례대로 검사.
	// FPaths::RootDir()은 보통 LunaticEngine/ (개발) 또는 exe 디렉터리(배포)를 반환한다.
	// 트레일링 슬래시 때문에 parent_path()가 의도대로 안 나올 수 있으므로 lexically_normal로 정규화.
	std::filesystem::path RootDir = std::filesystem::path(FPaths::RootDir()).lexically_normal();

	std::filesystem::path SolutionDir;
	std::filesystem::path BatPath;
	const std::filesystem::path Candidates[] = {
		RootDir,                                            // exe 디렉터리에 .bat이 있는 경우 (배포)
		RootDir.parent_path(),                              // LunaticEngine/의 상위 = 솔루션 루트 (개발)
		RootDir.parent_path().parent_path(),                // 한 단계 더 (혹시 모를 중첩)
		std::filesystem::current_path(),                    // 마지막 폴백
	};
	for (const auto& Candidate : Candidates)
	{
		const std::filesystem::path Tentative = Candidate / BatFileName;
		if (std::filesystem::exists(Tentative))
		{
			SolutionDir = Candidate;
			BatPath = Tentative;
			break;
		}
	}

	if (BatPath.empty())
	{
		FNotificationManager::Get().AddNotification(
			std::string("Package script not found: ") + BatFileName + " (searched near " + RootDir.string() + ")",
			ENotificationType::Error);
		return;
	}

	// .bat은 별도 콘솔 창에서 실행 (편집 중인 에디터를 막지 않게).
	// "cmd /c start \"Title\" /D <SolutionDir> cmd /k \"<bat>\"" 형태로 cmd 콘솔에서 띄움.
	std::wstring SolutionDirW = SolutionDir.wstring();
	std::wstring BatPathW = BatPath.wstring();

	std::wstring CommandLine = L"/c start \"Package Game Build\" /D \"" + SolutionDirW + L"\" cmd /k \"\"" + BatPathW + L"\"\"";

	HINSTANCE Result = ShellExecuteW(
		Window ? Window->GetHWND() : nullptr,
		L"open",
		L"cmd.exe",
		CommandLine.c_str(),
		SolutionDirW.c_str(),
		SW_SHOWNORMAL);

	if (reinterpret_cast<INT_PTR>(Result) <= 32)
	{
		FNotificationManager::Get().AddNotification(
			std::string("Failed to launch package script: ") + BatFileName,
			ENotificationType::Error);
		return;
	}

	FNotificationManager::Get().AddNotification(
		std::string("Packaging started: ") + BatFileName,
		ENotificationType::Info);
}
