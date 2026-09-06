#include "Editor/UI/EditorPlayToolbarWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/PIE/PIETypes.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Resource/ResourceManager.h"
#include "ImGui/imgui.h"
#include "WICTextureLoader.h"

#include <d3d11.h>

void FEditorPlayToolbarWidget::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
	Editor = InEditor;
	if (!InDevice) return;

	const FString PlayIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Play"));
	const FString PauseIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Pause"));
	const FString StopIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Stop"));
	const FString UndoIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Undo"));
	const FString RedoIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Redo"));

	DirectX::CreateWICTextureFromFile(
		InDevice, FPaths::ToWide(PlayIconPath).c_str(),
		nullptr, &PlayIcon);

	DirectX::CreateWICTextureFromFile(
		InDevice, FPaths::ToWide(PauseIconPath).c_str(),
		nullptr, &PauseIcon);

	DirectX::CreateWICTextureFromFile(
		InDevice, FPaths::ToWide(StopIconPath).c_str(),
		nullptr, &StopIcon);

	DirectX::CreateWICTextureFromFile(
		InDevice, FPaths::ToWide(UndoIconPath).c_str(),
		nullptr, &UndoIcon);

	DirectX::CreateWICTextureFromFile(
		InDevice, FPaths::ToWide(RedoIconPath).c_str(),
		nullptr, &RedoIcon);
}

void FEditorPlayToolbarWidget::Release()
{
	if (PlayIcon) { PlayIcon->Release(); PlayIcon = nullptr; }
	if (PauseIcon) { PauseIcon->Release(); PauseIcon = nullptr; }
	if (StopIcon) { StopIcon->Release(); StopIcon = nullptr; }
	if (UndoIcon) { UndoIcon->Release(); UndoIcon = nullptr; }
	if (RedoIcon) { RedoIcon->Release(); RedoIcon = nullptr; }
	Editor = nullptr;
}

void FEditorPlayToolbarWidget::Render(float Width)
{
	if (!Editor) return;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
	if (!ImGui::BeginChild("##PlayToolbar", ImVec2(Width, ToolbarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		return;
	}

	const bool bPlaying = Editor->IsPlayingInEditor();
	const bool bPaused = Editor->IsGamePaused();
	const float ButtonSize = ToolbarHeight - 14.0f;
	const float ButtonPadding = (ToolbarHeight - ButtonSize) * 0.5f;
	const float GroupPaddingX = 8.0f;
	const float GroupSpacing = 12.0f;
	const float GroupHeight = ButtonSize + 10.0f;
	const float GroupY = (ToolbarHeight - GroupHeight) * 0.5f;
	const float GroupInnerPadding = 6.0f;

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 7.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorAccentColor::Value);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorAccentColor::Value);
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.35f, 0.39f, 0.9f));

	auto DrawIconButton = [&](const char* Id, ID3D11ShaderResourceView* Icon, const char* FallbackLabel, bool bDisabled, const ImVec4& TintColor) -> bool
	{
		if (bDisabled)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
		}

		bool bClicked = false;
		if (Icon)
		{
			bClicked = ImGui::ImageButton(
				Id,
				reinterpret_cast<ImTextureID>(Icon),
				ImVec2(IconSize, IconSize),
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f),
				ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
				TintColor);
		}
		else
		{
			bClicked = ImGui::Button(FallbackLabel, ImVec2(ButtonSize, ButtonSize));
		}

		if (bDisabled)
		{
			ImGui::PopStyleVar();
			bClicked = false;
		}

		return bClicked;
	};

	auto DrawButtonSeparator = [&]()
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 CursorScreenPos = ImGui::GetCursorScreenPos();
		const float SeparatorX = CursorScreenPos.x + ButtonSpacing * 0.5f;
		const float SeparatorTop = CursorScreenPos.y + 3.0f;
		const float SeparatorBottom = CursorScreenPos.y + ButtonSize - 3.0f;
		DrawList->AddLine(
			ImVec2(SeparatorX, SeparatorTop),
			ImVec2(SeparatorX, SeparatorBottom),
			ImGui::GetColorU32(ImVec4(0.36f, 0.36f, 0.40f, 0.9f)),
			1.0f);
	};

	auto DrawToolbarGroup = [&](const char* Id, float X, float Width)
	{
		ImGui::SetCursorPos(ImVec2(X, GroupY));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.13f, 0.95f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
		ImGui::BeginChild(Id, ImVec2(Width, GroupHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::SetCursorPos(ImVec2(GroupPaddingX, 5.0f));
	};

	auto EndToolbarGroup = [&]()
	{
		ImGui::EndChild();
	};

	const ImVec4 PlayTint = bPlaying ? ImVec4(1.0f, 1.0f, 1.0f, 0.7f) : ImVec4(0.30f, 0.90f, 0.35f, 1.0f);
	const ImVec4 PauseTint = (bPlaying && bPaused) ? EditorAccentColor::Value : ImVec4(1.0f, 1.0f, 1.0f, bPlaying ? 1.0f : 0.7f);
	const ImVec4 StopTint = bPlaying ? ImVec4(0.95f, 0.28f, 0.25f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
	const float PlayGroupWidth = GroupPaddingX * 2.0f + GroupInnerPadding * 2.0f + ButtonSize * 3.0f + ButtonSpacing * 2.0f;
	const float HistoryGroupWidth = GroupPaddingX * 2.0f + GroupInnerPadding * 2.0f + ButtonSize * 2.0f + ButtonSpacing;
	const float HistoryGroupX = (std::max)(ButtonPadding, Width - ButtonPadding - HistoryGroupWidth);
	const float PlayGroupX = ButtonPadding;

	DrawToolbarGroup("##PlayGroup", PlayGroupX, PlayGroupWidth);
	if (DrawIconButton("##PIE_Play", PlayIcon, "Play", bPlaying, PlayTint))
	{
		FRequestPlaySessionParams Params;
		Editor->RequestPlaySession(Params);
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::SetTooltip("Play");
	}

	DrawButtonSeparator();
	ImGui::SameLine(0.0f, ButtonSpacing);

	if (DrawIconButton("##PIE_Pause", PauseIcon, "Pause", !bPlaying, PauseTint))
	{
		Editor->SetGamePaused(!Editor->IsGamePaused());
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::SetTooltip(bPaused ? "Resume" : "Pause");
	}

	DrawButtonSeparator();
	ImGui::SameLine(0.0f, ButtonSpacing);

	if (DrawIconButton("##PIE_Stop", StopIcon, "Stop", !bPlaying, StopTint))
	{
		Editor->RequestEndPlayMap();
		Editor->SetGamePaused(false);
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::SetTooltip("Stop");
	}
	EndToolbarGroup();

	DrawToolbarGroup("##HistoryGroup", HistoryGroupX, HistoryGroupWidth);
	const bool bDisableHistory = bPlaying;
	if (DrawIconButton("##SceneUndo", UndoIcon, "Undo", bDisableHistory || !Editor->CanUndoTransformChange(), ImVec4(1.0f, 1.0f, 1.0f, bDisableHistory ? 0.35f : 0.9f)))
	{
		Editor->UndoTrackedTransformChange();
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::SetTooltip("Undo");
	}

	DrawButtonSeparator();
	ImGui::SameLine(0.0f, ButtonSpacing);
	if (DrawIconButton("##SceneRedo", RedoIcon, "Redo", bDisableHistory || !Editor->CanRedoTransformChange(), ImVec4(1.0f, 1.0f, 1.0f, bDisableHistory ? 0.35f : 0.9f)))
	{
		Editor->RedoTrackedTransformChange();
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::SetTooltip("Redo");
	}
	EndToolbarGroup();

	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);
	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}
