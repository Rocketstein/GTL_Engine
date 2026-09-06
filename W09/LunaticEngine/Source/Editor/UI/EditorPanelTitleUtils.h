#pragma once

#include "Object/FName.h"
#include "Resource/ResourceManager.h"
#include "Editor/UI/EditorAccentColor.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <string>
#include <vector>

namespace EditorPanelTitleUtils
{
	struct FPendingPanelDecoration
	{
		ImGuiWindow* Window = nullptr;
		const char* IconKey = nullptr;
		bool* VisibleFlag = nullptr;
		ImRect TitleRect{};
		bool bHasTitleRect = false;
		bool bFocused = false;
	};

	struct FFocusedPanelOverlay
	{
		ImDrawList* DrawList = nullptr;
		ImRect TitleRect{};
		float TabRounding = 0.0f;
	};

	inline ImFont*& GetPanelChromeIconFontStorage()
	{
		static ImFont* Font = nullptr;
		return Font;
	}

	inline std::vector<FPendingPanelDecoration>& GetPendingDecorations()
	{
		static std::vector<FPendingPanelDecoration> Decorations;
		return Decorations;
	}

	inline std::vector<FFocusedPanelOverlay>& GetFocusedPanelOverlays()
	{
		static std::vector<FFocusedPanelOverlay> Overlays;
		return Overlays;
	}

	inline void EnsurePanelChromeIconFontLoaded()
	{
		ImFont*& Font = GetPanelChromeIconFontStorage();
		if (Font)
		{
			return;
		}

		const char* FontPath = "C:/Windows/Fonts/segmdl2.ttf";
		if (!std::filesystem::exists(FontPath))
		{
			return;
		}

		ImFontConfig FontConfig{};
		FontConfig.PixelSnapH = true;
		Font = ImGui::GetIO().Fonts->AddFontFromFileTTF(FontPath, 12.0f, &FontConfig);
	}

	inline ImFont* GetPanelChromeIconFont()
	{
		return GetPanelChromeIconFontStorage();
	}

	inline ImU32 GetDockTabBarGapColor()
	{
		return IM_COL32(5, 5, 5, 255);
	}

	inline float GetPanelTitleTopGapHeight()
	{
		return 5.0f;
	}

	inline float GetPanelFrameGapThickness()
	{
		return 5.0f;
	}

	inline float GetPanelContentTopInset()
	{
		return 8.0f;
	}

	inline float GetPanelContentSideInset()
	{
		return 12.0f;
	}

	inline float GetPanelContentBottomInset()
	{
		return 10.0f;
	}

	inline float GetSelectedPanelTopBorderThickness()
	{
		return 2.0f;
	}

	inline ImU32 GetSelectedPanelTopBorderColor()
	{
		return EditorAccentColor::ToU32();
	}

	inline float GetSelectedPanelTopBorderInset()
	{
		return 1.5f;
	}

	inline const char* GetChromeCloseGlyph()
	{
		return "\xEE\xA2\xBB";
	}

	inline void BeginPanelDecorationFrame()
	{
		GetPendingDecorations().clear();
		GetFocusedPanelOverlays().clear();
	}

	inline FString GetEditorPathResource(const char* Key)
	{
		return FResourceManager::Get().ResolvePath(FName(Key));
	}

	inline ID3D11ShaderResourceView* GetEditorIcon(const char* Key)
	{
		if (!Key || Key[0] == '\0')
		{
			return nullptr;
		}

		if (FTextureResource* Texture = FResourceManager::Get().FindTexture(FName(Key)))
		{
			return Texture->SRV;
		}

		return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource(Key)).Get();
	}

	inline std::string MakeClosablePanelTitle(const char* Title, const char* IconKey = nullptr)
	{
		const char* Prefix = GetEditorIcon(IconKey) ? "     " : "";
		return std::string(Prefix) + Title + "          ###" + Title;
	}

	inline ImRect GetPanelTitleRect()
	{
		ImGuiWindow* Window = ImGui::GetCurrentWindow();
		if (Window && Window->DockIsActive && Window->DockNode && Window->DockNode->TabBar)
		{
			ImGuiTabBar* TabBar = Window->DockNode->TabBar;
			for (int TabIndex = 0; TabIndex < TabBar->Tabs.Size; ++TabIndex)
			{
				const ImGuiTabItem& Tab = TabBar->Tabs[TabIndex];
				if (Tab.Window != Window)
				{
					continue;
				}

				const float TabMinX = TabBar->BarRect.Min.x + Tab.Offset;
				const float TabMaxX = TabMinX + Tab.Width;
				return ImRect(
					ImVec2(TabMinX, TabBar->BarRect.Min.y),
					ImVec2(TabMaxX, TabBar->BarRect.Max.y));
			}
		}
		if (Window && Window->DockTabIsVisible)
		{
			return Window->DC.DockTabItemRect;
		}
		return Window ? Window->TitleBarRect() : ImRect();
	}

	inline ImRect GetPanelTitleRect(const ImGuiWindow* Window)
	{
		if (Window && Window->DockIsActive && Window->DockNode && Window->DockNode->TabBar)
		{
			ImGuiTabBar* TabBar = Window->DockNode->TabBar;
			for (int TabIndex = 0; TabIndex < TabBar->Tabs.Size; ++TabIndex)
			{
				const ImGuiTabItem& Tab = TabBar->Tabs[TabIndex];
				if (Tab.Window != Window)
				{
					continue;
				}

				const float TabMinX = TabBar->BarRect.Min.x + Tab.Offset;
				const float TabMaxX = TabMinX + Tab.Width;
				return ImRect(
					ImVec2(TabMinX, TabBar->BarRect.Min.y),
					ImVec2(TabMaxX, TabBar->BarRect.Max.y));
			}
		}
		return Window ? Window->TitleBarRect() : ImRect();
	}

	inline ImDrawList* GetPanelTitleDrawList(ImGuiWindow* Window)
	{
		if (Window && Window->DockIsActive && Window->DockNode && Window->DockNode->HostWindow)
		{
			return Window->DockNode->HostWindow->DrawList;
		}
		return Window ? Window->DrawList : ImGui::GetForegroundDrawList();
	}

	inline void PaintDockTabBarEmptyRegions(ImGuiWindow* Window)
	{
		if (!Window || !Window->DockIsActive || !Window->DockNode || !Window->DockNode->TabBar)
		{
			return;
		}

		ImGuiTabBar* TabBar = Window->DockNode->TabBar;
		if (TabBar->Tabs.Size <= 0)
		{
			return;
		}

		float TabsMinX = FLT_MAX;
		float TabsMaxX = -FLT_MAX;
		for (int TabIndex = 0; TabIndex < TabBar->Tabs.Size; ++TabIndex)
		{
			const ImGuiTabItem& Tab = TabBar->Tabs[TabIndex];
			const float TabMinX = TabBar->BarRect.Min.x + Tab.Offset;
			const float TabMaxX = TabMinX + Tab.Width;
			TabsMinX = (std::min)(TabsMinX, TabMinX);
			TabsMaxX = (std::max)(TabsMaxX, TabMaxX);
		}

		if (TabsMinX > TabsMaxX)
		{
			return;
		}

		ImDrawList* DrawList = GetPanelTitleDrawList(Window);
		DrawList->PushClipRect(TabBar->BarRect.Min, TabBar->BarRect.Max, true);

		if (TabsMinX > TabBar->BarRect.Min.x)
		{
			DrawList->AddRectFilled(
				TabBar->BarRect.Min,
				ImVec2(TabsMinX, TabBar->BarRect.Max.y),
				GetDockTabBarGapColor());
		}

		if (TabsMaxX < TabBar->BarRect.Max.x)
		{
			DrawList->AddRectFilled(
				ImVec2(TabsMaxX, TabBar->BarRect.Min.y),
				TabBar->BarRect.Max,
				GetDockTabBarGapColor());
		}

		for (int TabIndex = 0; TabIndex + 1 < TabBar->Tabs.Size; ++TabIndex)
		{
			const ImGuiTabItem& CurrentTab = TabBar->Tabs[TabIndex];
			const ImGuiTabItem& NextTab = TabBar->Tabs[TabIndex + 1];
			const float CurrentTabMaxX = TabBar->BarRect.Min.x + CurrentTab.Offset + CurrentTab.Width;
			const float NextTabMinX = TabBar->BarRect.Min.x + NextTab.Offset;
			if (NextTabMinX <= CurrentTabMaxX)
			{
				continue;
			}

			DrawList->AddRectFilled(
				ImVec2(CurrentTabMaxX, TabBar->BarRect.Min.y),
				ImVec2(NextTabMinX, TabBar->BarRect.Max.y),
				GetDockTabBarGapColor());
		}

		DrawList->PopClipRect();
	}

	inline void QueuePanelDecoration(const char* IconKey, bool* VisibleFlag)
	{
		ImGuiWindow* Window = ImGui::GetCurrentWindow();
		if (!Window)
		{
			return;
		}

		const ImRect TitleRect = GetPanelTitleRect();
		const bool bHasTitleRect = TitleRect.GetWidth() > 0.0f && TitleRect.GetHeight() > 0.0f;
		const bool bFocused = Window->DockTabIsVisible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		std::vector<FPendingPanelDecoration>& Decorations = GetPendingDecorations();
		for (FPendingPanelDecoration& Decoration : Decorations)
		{
			if (Decoration.Window != Window)
			{
				continue;
			}

			if (IconKey)
			{
				Decoration.IconKey = IconKey;
			}
			if (VisibleFlag)
			{
				Decoration.VisibleFlag = VisibleFlag;
			}
			Decoration.TitleRect = TitleRect;
			Decoration.bHasTitleRect = bHasTitleRect;
			Decoration.bFocused = bFocused;
			return;
		}

		FPendingPanelDecoration Decoration;
		Decoration.Window = Window;
		Decoration.IconKey = IconKey;
		Decoration.VisibleFlag = VisibleFlag;
		Decoration.TitleRect = TitleRect;
		Decoration.bHasTitleRect = bHasTitleRect;
		Decoration.bFocused = bFocused;
		Decorations.push_back(Decoration);
	}

	inline void DrawPanelTitleIcon(const char* IconKey, float IconSize = 16.0f)
	{
		(void)IconSize;
		QueuePanelDecoration(IconKey, nullptr);
	}

	inline bool DrawSmallPanelCloseButton(const char* DisplayTitle, bool& bVisible, const char* Id)
	{
		(void)DisplayTitle;
		(void)Id;
		QueuePanelDecoration(nullptr, &bVisible);
		return false;
	}

	inline void ApplyPanelContentTopInset(bool bApplySideInset = true, bool bApplyBottomInset = true)
	{
		ImGuiWindow* Window = ImGui::GetCurrentWindow();
		if (!Window)
		{
			return;
		}

		const ImGuiID InsetStateId = ImHashStr("##EditorPanelContentInsetApplied");
		int* const LastAppliedFrame = Window->StateStorage.GetIntRef(InsetStateId, -1);
		if (*LastAppliedFrame != ImGui::GetFrameCount())
		{
			*LastAppliedFrame = ImGui::GetFrameCount();

			const float SideInset = bApplySideInset ? (std::max)(GetPanelContentSideInset(), GetPanelFrameGapThickness()) : 0.0f;
			const float BottomInset = bApplyBottomInset ? (std::max)(GetPanelContentBottomInset(), GetPanelFrameGapThickness()) : 0.0f;
			Window->WorkRect.Min.x = (std::min)(Window->WorkRect.Max.x, Window->WorkRect.Min.x + SideInset);
			Window->WorkRect.Max.x = (std::max)(Window->WorkRect.Min.x, Window->WorkRect.Max.x - SideInset);
			Window->WorkRect.Max.y = (std::max)(Window->WorkRect.Min.y, Window->WorkRect.Max.y - BottomInset);

			Window->ContentRegionRect.Min.x = (std::min)(Window->ContentRegionRect.Max.x, Window->ContentRegionRect.Min.x + SideInset);
			Window->ContentRegionRect.Max.x = (std::max)(Window->ContentRegionRect.Min.x, Window->ContentRegionRect.Max.x - SideInset);
			Window->ContentRegionRect.Max.y = (std::max)(Window->ContentRegionRect.Min.y, Window->ContentRegionRect.Max.y - BottomInset);

			Window->DC.Indent.x += SideInset;
			Window->DC.CursorPos.x += SideInset;
			Window->DC.CursorStartPos.x += SideInset;
			Window->DC.CursorMaxPos.x = (std::max)(Window->DC.CursorMaxPos.x, Window->DC.CursorPos.x);
		}

		ImGui::Dummy(ImVec2(0.0f, GetPanelContentTopInset()));
	}

	inline void FlushPanelDecorations()
	{
		std::vector<ImGuiTabBar*> PaintedTabBars;
		for (FPendingPanelDecoration& Decoration : GetPendingDecorations())
		{
			ImGuiWindow* Window = Decoration.Window;
			if (!Window)
			{
				continue;
			}

			if (Window->DockIsActive && Window->DockNode && Window->DockNode->TabBar)
			{
				ImGuiTabBar* TabBar = Window->DockNode->TabBar;
				if (std::find(PaintedTabBars.begin(), PaintedTabBars.end(), TabBar) == PaintedTabBars.end())
				{
					PaintDockTabBarEmptyRegions(Window);
					PaintedTabBars.push_back(TabBar);
				}
			}

			const ImRect TitleRect = Decoration.bHasTitleRect ? Decoration.TitleRect : GetPanelTitleRect(Window);
			if (TitleRect.GetWidth() <= 0.0f || TitleRect.GetHeight() <= 0.0f)
			{
				continue;
			}

			ImDrawList* DrawList = GetPanelTitleDrawList(Window);
			const float FrameGapThickness = GetPanelFrameGapThickness();
			const ImRect WindowRect = Window->Rect();
			const ImRect ExpandedWindowRect(
				ImVec2(WindowRect.Min.x - FrameGapThickness, WindowRect.Min.y),
				ImVec2(WindowRect.Max.x + FrameGapThickness, WindowRect.Max.y));
			const float TabRounding = (std::min)(ImGui::GetStyle().TabRounding, TitleRect.GetHeight() * 0.5f);
			const float TitleGapHeight = (std::min)(GetPanelTitleTopGapHeight(), TitleRect.GetHeight());
			const float BodyTopY = (std::max)(WindowRect.Min.y + FrameGapThickness, TitleRect.Max.y);
			DrawList->PushClipRect(ExpandedWindowRect.Min, ExpandedWindowRect.Max, true);
			DrawList->AddRectFilled(
				ExpandedWindowRect.Min,
				ImVec2(ExpandedWindowRect.Max.x, (std::min)(ExpandedWindowRect.Min.y + FrameGapThickness, ExpandedWindowRect.Max.y)),
				GetDockTabBarGapColor(),
				TabRounding,
				ImDrawFlags_RoundCornersTop);
			if (BodyTopY < ExpandedWindowRect.Max.y)
			{
				DrawList->AddRectFilled(
					ImVec2(ExpandedWindowRect.Min.x, BodyTopY),
					ImVec2((std::min)(WindowRect.Min.x + FrameGapThickness, ExpandedWindowRect.Max.x), ExpandedWindowRect.Max.y),
					GetDockTabBarGapColor());
				DrawList->AddRectFilled(
					ImVec2((std::max)(WindowRect.Max.x - FrameGapThickness, ExpandedWindowRect.Min.x), BodyTopY),
					ExpandedWindowRect.Max,
					GetDockTabBarGapColor());
			}
			DrawList->PopClipRect();

			DrawList->PushClipRect(TitleRect.Min, TitleRect.Max, true);
			DrawList->AddRectFilled(
				TitleRect.Min,
				ImVec2(TitleRect.Max.x, TitleRect.Min.y + TitleGapHeight),
				GetDockTabBarGapColor(),
				TabRounding,
				ImDrawFlags_RoundCornersTop);
			if (Decoration.bFocused)
			{
				FFocusedPanelOverlay Overlay;
				Overlay.DrawList = DrawList;
				Overlay.TitleRect = TitleRect;
				Overlay.TabRounding = TabRounding;
				GetFocusedPanelOverlays().push_back(Overlay);
			}

			if (ID3D11ShaderResourceView* Icon = GetEditorIcon(Decoration.IconKey))
			{
				const float IconSize = 14.0f;
				const float IconX = TitleRect.Min.x + 8.0f;
				const float IconY = TitleRect.Min.y + (TitleRect.GetHeight() - IconSize) * 0.5f;
				DrawList->AddImage(
					reinterpret_cast<ImTextureID>(Icon),
					ImVec2(IconX, IconY),
					ImVec2(IconX + IconSize, IconY + IconSize));
			}

			if (Decoration.VisibleFlag)
			{
				const float ButtonSize = (std::max)(TitleRect.GetHeight() - 8.0f, 16.0f);
				const ImVec2 ButtonPos(
					TitleRect.Max.x - ButtonSize - 6.0f,
					TitleRect.Min.y + (TitleRect.GetHeight() - ButtonSize) * 0.5f);
				const ImRect ButtonRect(ButtonPos, ImVec2(ButtonPos.x + ButtonSize, ButtonPos.y + ButtonSize));
				const ImVec2 MousePos = ImGui::GetIO().MousePos;
				const bool bHovered = ButtonRect.Contains(MousePos);
				const bool bClicked = bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

				if (bHovered)
				{
					DrawList->AddRectFilled(
						ButtonRect.Min,
						ButtonRect.Max,
						IM_COL32(66, 66, 74, 242),
						4.0f);
				}

				const ImU32 GlyphColor = bHovered ? IM_COL32(240, 240, 240, 255) : IM_COL32(190, 190, 190, 255);
				const char* Glyph = GetChromeCloseGlyph();
				if (ImFont* IconFont = GetPanelChromeIconFont())
				{
					const float FontSize = 11.0f;
					const ImVec2 GlyphSize = IconFont->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Glyph);
					DrawList->AddText(
						IconFont,
						FontSize,
						ImVec2(ButtonRect.Min.x + (ButtonSize - GlyphSize.x) * 0.5f, ButtonRect.Min.y + (ButtonSize - GlyphSize.y) * 0.5f + 0.5f),
						GlyphColor,
						Glyph);
				}

				if (bClicked)
				{
					*Decoration.VisibleFlag = false;
				}
			}

			DrawList->PopClipRect();
		}

		for (const FFocusedPanelOverlay& Overlay : GetFocusedPanelOverlays())
		{
			if (!Overlay.DrawList || Overlay.TitleRect.GetWidth() <= 0.0f || Overlay.TitleRect.GetHeight() <= 0.0f)
			{
				continue;
			}

			const float BorderThickness = (std::min)(GetSelectedPanelTopBorderThickness(), Overlay.TitleRect.GetHeight() * 0.5f);
			const float BorderInset = GetSelectedPanelTopBorderInset();
			const float TopGapHeight = (std::min)(GetPanelTitleTopGapHeight(), Overlay.TitleRect.GetHeight());
			const ImRect InnerTopBorderRect(
				ImVec2(Overlay.TitleRect.Min.x + BorderInset, Overlay.TitleRect.Min.y + TopGapHeight + BorderInset),
				ImVec2(Overlay.TitleRect.Max.x - BorderInset, Overlay.TitleRect.Min.y + TopGapHeight + BorderInset + BorderThickness));
			if (InnerTopBorderRect.GetWidth() <= 0.0f || InnerTopBorderRect.GetHeight() <= 0.0f)
			{
				continue;
			}

			Overlay.DrawList->PushClipRect(Overlay.TitleRect.Min, Overlay.TitleRect.Max, true);
			Overlay.DrawList->AddRectFilled(
				InnerTopBorderRect.Min,
				InnerTopBorderRect.Max,
				GetSelectedPanelTopBorderColor(),
				(std::max)(Overlay.TabRounding - BorderInset, 0.0f),
				ImDrawFlags_RoundCornersTop);
			Overlay.DrawList->PopClipRect();
		}
	}
}
