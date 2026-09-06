// 에디터 하단 drawer strip과 drawer overlay 상태를 구현합니다.
#include "Editor/UI/EditorBottomBar.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>

namespace
{
constexpr float EditorBottomDrawerBarHeight = 36.0f;
constexpr float EditorDrawerAnimationSpeed = 10.0f;

float MoveToward(float Current, float Target, float DeltaTime)
{
    const float Step = EditorDrawerAnimationSpeed * DeltaTime;
    if (Current < Target)
    {
        return (std::min)(Current + Step, Target);
    }
    return (std::max)(Current - Step, Target);
}

float EaseOutCubic(float Value)
{
    const float T = (std::max)(0.0f, (std::min)(Value, 1.0f));
    const float Inverse = 1.0f - T;
    return 1.0f - Inverse * Inverse * Inverse;
}

const char* GetDrawerShortcutText(EEditorDrawer Drawer)
{
    switch (Drawer)
    {
    case EEditorDrawer::Content:
        return "Shortcut: Ctrl + Space";
    case EEditorDrawer::OutputLog:
        return "Shortcut: Alt + `\nCommand input focus: `";
    case EEditorDrawer::None:
    default:
        return "";
    }
}
} // namespace

void FEditorBottomBar::Render(float DeltaTime)
{
    UpdateDrawerAnimation(DeltaTime);
    RenderBar();
}

bool FEditorBottomBar::BeginDrawerOverlay()
{
    if (VisibleDrawer == EEditorDrawer::None || DrawerOpenRatio <= 0.0f)
    {
        return false;
    }

    ImGuiViewport* Viewport = ImGui::GetMainViewport();
    const float Height = (std::min)(DrawerHeight, (std::max)(160.0f, Viewport->Size.y * 0.45f));
    const float EasedRatio = EaseOutCubic(DrawerOpenRatio);
    const float VisibleHeight = (std::max)(1.0f, Height * EasedRatio);

    LastDrawerX = Viewport->Pos.x;
    LastDrawerY = Viewport->Pos.y + Viewport->Size.y - EditorBottomDrawerBarHeight - VisibleHeight;
    LastDrawerWidth = Viewport->Size.x;
    LastDrawerHeight = VisibleHeight;

    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking;

    ImGui::SetNextWindowViewport(Viewport->ID);
    ImGui::SetNextWindowPos(ImVec2(LastDrawerX, LastDrawerY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(LastDrawerWidth, LastDrawerHeight), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, (std::max)(0.35f, EasedRatio));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));

    if (!ImGui::Begin("##EditorBottomDrawer", nullptr, Flags))
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return false;
    }

    bDrawerOverlayBegun = true;
    return true;
}

void FEditorBottomBar::EndDrawerOverlay()
{
    if (!bDrawerOverlayBegun)
    {
        return;
    }

    ImGuiViewport* Viewport = ImGui::GetMainViewport();
    const ImVec2 MousePos = ImGui::GetIO().MousePos;
    const bool bMouseInDrawer =
        MousePos.x >= LastDrawerX &&
        MousePos.x <= LastDrawerX + LastDrawerWidth &&
        MousePos.y >= LastDrawerY &&
        MousePos.y <= LastDrawerY + LastDrawerHeight;
    const bool bMouseInBottomBar =
        MousePos.x >= Viewport->Pos.x &&
        MousePos.x <= Viewport->Pos.x + Viewport->Size.x &&
        MousePos.y >= Viewport->Pos.y + Viewport->Size.y - EditorBottomDrawerBarHeight &&
        MousePos.y <= Viewport->Pos.y + Viewport->Size.y;
    const bool bAnyPopupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !bAnyPopupOpen && !bMouseInDrawer && !bMouseInBottomBar)
    {
        ActiveDrawer = EEditorDrawer::None;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    bDrawerOverlayBegun = false;
}

void FEditorBottomBar::ToggleDrawer(EEditorDrawer Drawer)
{
    if (Drawer == EEditorDrawer::None)
    {
        CloseDrawer();
        return;
    }

    if (ActiveDrawer == Drawer)
    {
        CloseDrawer();
        return;
    }

    OpenDrawer(Drawer);
}

void FEditorBottomBar::OpenDrawer(EEditorDrawer Drawer)
{
    if (Drawer == EEditorDrawer::None)
    {
        CloseDrawer();
        return;
    }

    ActiveDrawer = Drawer;
    VisibleDrawer = Drawer;
}

void FEditorBottomBar::CloseDrawer()
{
    ActiveDrawer = EEditorDrawer::None;
}

void FEditorBottomBar::UpdateDrawerAnimation(float DeltaTime)
{
    if (ActiveDrawer != EEditorDrawer::None)
    {
        VisibleDrawer = ActiveDrawer;
    }

    const float TargetRatio = ActiveDrawer == EEditorDrawer::None ? 0.0f : 1.0f;
    DrawerOpenRatio = MoveToward(DrawerOpenRatio, TargetRatio, DeltaTime);

    if (DrawerOpenRatio <= 0.0f && ActiveDrawer == EEditorDrawer::None)
    {
        VisibleDrawer = EEditorDrawer::None;
    }
}

void FEditorBottomBar::RenderBar()
{
    ImGuiViewport* Viewport = ImGui::GetMainViewport();

    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    if (ImGui::BeginViewportSideBar("##EditorBottomDrawerBar", Viewport, ImGuiDir_Down, EditorBottomDrawerBarHeight, Flags))
    {
        DrawDrawerButton("Content Drawer", EEditorDrawer::Content);
        ImGui::SameLine();
        DrawDrawerButton("Output Log", EEditorDrawer::OutputLog);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void FEditorBottomBar::DrawDrawerButton(const char* Label, EEditorDrawer Drawer)
{
    const bool bActive = ActiveDrawer == Drawer;
    const float ButtonHeight = 26.0f;
    const float IconSize = 14.0f;
    const float PaddingX = 10.0f;
    const float IconGap = 7.0f;

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImVec2 Pos = ImGui::GetCursorScreenPos();
    const ImVec2 TextSize = ImGui::CalcTextSize(Label);
    const ImVec2 Size(PaddingX * 2.0f + IconSize + IconGap + TextSize.x, ButtonHeight);

    const bool bPressed = ImGui::InvisibleButton(Label, Size);
    const bool bHovered = ImGui::IsItemHovered();

    if (bPressed)
    {
        ToggleDrawer(Drawer);
    }

    if (bActive || bHovered)
    {
        const ImU32 BgColor = bActive ? IM_COL32(54, 54, 54, 255) : IM_COL32(42, 42, 42, 255);
        DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Size.x, Pos.y + Size.y), BgColor, 3.0f);
    }

    if (bHovered)
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(Label);
        ImGui::Separator();
        ImGui::TextUnformatted(GetDrawerShortcutText(Drawer));
        ImGui::EndTooltip();
    }

    if (bActive)
    {
        DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Size.x, Pos.y + 2.0f), IM_COL32(70, 145, 255, 255), 1.0f);
    }

    const ImU32 IconColor = bActive ? IM_COL32(235, 235, 235, 255) : IM_COL32(185, 185, 185, 255);
    const ImU32 TextColor = bActive ? IM_COL32(245, 245, 245, 255) : IM_COL32(205, 205, 205, 255);
    const ImVec2 IconPos(Pos.x + PaddingX, Pos.y + (ButtonHeight - IconSize) * 0.5f);

    if (Drawer == EEditorDrawer::Content)
    {
        DrawList->AddRectFilled(
            ImVec2(IconPos.x + 1.0f, IconPos.y + 3.0f),
            ImVec2(IconPos.x + 7.0f, IconPos.y + 6.0f),
            IconColor,
            1.0f);
        DrawList->AddRect(
            ImVec2(IconPos.x + 1.0f, IconPos.y + 5.0f),
            ImVec2(IconPos.x + 13.0f, IconPos.y + 12.0f),
            IconColor,
            2.0f);
    }
    else if (Drawer == EEditorDrawer::OutputLog)
    {
        DrawList->AddRect(
            ImVec2(IconPos.x + 1.0f, IconPos.y + 2.0f),
            ImVec2(IconPos.x + 13.0f, IconPos.y + 12.0f),
            IconColor,
            2.0f);
        DrawList->AddLine(
            ImVec2(IconPos.x + 4.0f, IconPos.y + 5.0f),
            ImVec2(IconPos.x + 10.0f, IconPos.y + 5.0f),
            IconColor);
        DrawList->AddLine(
            ImVec2(IconPos.x + 4.0f, IconPos.y + 8.0f),
            ImVec2(IconPos.x + 10.0f, IconPos.y + 8.0f),
            IconColor);
    }

    DrawList->AddText(
        ImVec2(Pos.x + PaddingX + IconSize + IconGap, Pos.y + (ButtonHeight - TextSize.y) * 0.5f),
        TextColor,
        Label);
}
