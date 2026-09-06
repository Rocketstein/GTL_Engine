#include "Editor/UI/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorPanelTitleUtils.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Resource/ResourceManager.h"

#include "ImGui/imgui.h"
#include "Profiling/Stats.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>

namespace
{
	constexpr ImVec4 PopupSectionHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
	constexpr ImVec4 OutlinerSelectionHeaderColor = EditorAccentColor::Value;
	constexpr ImVec4 OutlinerSelectionHeaderHoveredColor = EditorAccentColor::Value;
	constexpr ImVec4 OutlinerSelectionHeaderActiveColor = EditorAccentColor::Value;
	constexpr ImVec4 PopupMenuItemHoverColor = EditorAccentColor::Value;
	constexpr ImVec4 PopupMenuItemActiveColor = EditorAccentColor::Value;
	constexpr ImVec4 OutlinerFolderArrowColor = ImVec4(0.66f, 0.66f, 0.68f, 1.0f);
	constexpr ImVec4 OutlinerItemLabelColor = ImVec4(0.86f, 0.86f, 0.88f, 1.0f);
	constexpr ImU32 OutlinerFolderIconTint = IM_COL32(184, 140, 58, 255);
	constexpr ImVec4 OutlinerButtonColor = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
	constexpr ImVec4 OutlinerButtonHoveredColor = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
	constexpr ImVec4 OutlinerButtonActiveColor = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
	constexpr ImVec4 OutlinerButtonBorderColor = ImVec4(0.42f, 0.42f, 0.45f, 0.90f);
	constexpr float OutlinerToolbarButtonHeight = 0.0f;
	constexpr float OutlinerFooterButtonHeight = 24.0f;
	constexpr size_t OutlinerFolderNameCapacity = 128;

	FString GetEditorPathResource(const char* Key)
	{
		return FResourceManager::Get().ResolvePath(FName(Key));
	}

	ID3D11ShaderResourceView* GetEditorIcon(const char* Key)
	{
		return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource(Key)).Get();
	}

	void DrawPopupSectionHeader(const char* Label)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PopupSectionHeaderTextColor);
		ImGui::SeparatorText(Label);
		ImGui::PopStyleColor();
	}

	enum class EActorTypeSection
	{
		Lights,
		Gameplay,
		Scene,
		Other
	};

	EActorTypeSection GetActorTypeSection(const FString& TypeName)
	{
		if (TypeName.find("Light") != FString::npos)
		{
			return EActorTypeSection::Lights;
		}
		if (TypeName.find("Pawn") != FString::npos
			|| TypeName.find("Character") != FString::npos
			|| TypeName.find("Camera") != FString::npos)
		{
			return EActorTypeSection::Gameplay;
		}
		if (TypeName.find("StaticMesh") != FString::npos
			|| TypeName.find("Actor") != FString::npos
			|| TypeName.find("Decal") != FString::npos
			|| TypeName.find("Fog") != FString::npos)
		{
			return EActorTypeSection::Scene;
		}
		return EActorTypeSection::Other;
	}

	bool DrawSearchInputWithIcon(const char* Id, const char* Hint, char* Buffer, size_t BufferSize, float Width)
	{
		ImGuiStyle& Style = ImGui::GetStyle();
		ImGui::SetNextItemWidth(Width);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 11.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Style.FramePadding.x + 26.0f, Style.FramePadding.y));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.42f, 0.45f, 0.90f));
		const std::string PaddedHint = std::string("   ") + Hint;
		const bool bChanged = ImGui::InputTextWithHint(Id, PaddedHint.c_str(), Buffer, BufferSize);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);

		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const float LeadingSlotWidth = (std::min)(30.0f, Max.x - Min.x);
		ImGui::GetWindowDrawList()->AddRectFilled(
			ImVec2(Min.x + 1.0f, Min.y + 1.0f),
			ImVec2(Min.x + LeadingSlotWidth, Max.y - 1.0f),
			IM_COL32(5, 5, 5, 255),
			11.0f,
			ImDrawFlags_RoundCornersLeft);

		if (ID3D11ShaderResourceView* SearchIcon = GetEditorIcon("Editor.Icon.Search"))
		{
			const float IconSize = ImGui::GetFrameHeight() - 12.0f;
			const float IconY = Min.y + (ImGui::GetFrameHeight() - IconSize) * 0.5f;
			ImGui::GetWindowDrawList()->AddImage(
				reinterpret_cast<ImTextureID>(SearchIcon),
				ImVec2(Min.x + 7.0f, IconY),
				ImVec2(Min.x + 7.0f + IconSize, IconY + IconSize),
				ImVec2(1.0f, 0.0f),
				ImVec2(0.0f, 1.0f),
				IM_COL32(210, 210, 210, 255));
		}

		return bChanged;
	}

	void DrawCenteredButtonIcon(const char* IconKey, float IconSize, ImU32 Tint = IM_COL32_WHITE)
	{
		if (ID3D11ShaderResourceView* Icon = GetEditorIcon(IconKey))
		{
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Size = ImGui::GetItemRectSize();
			const float X = Min.x + (Size.x - IconSize) * 0.5f;
			const float Y = Min.y + (Size.y - IconSize) * 0.5f;
			ImGui::GetWindowDrawList()->AddImage(
				reinterpret_cast<ImTextureID>(Icon),
				ImVec2(X, Y),
				ImVec2(X + IconSize, Y + IconSize),
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f),
				Tint);
		}
	}

	void PushOutlinerButtonStyle(float FrameRounding = 6.0f)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, FrameRounding);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, OutlinerButtonColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OutlinerButtonHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, OutlinerButtonActiveColor);
		ImGui::PushStyleColor(ImGuiCol_Border, OutlinerButtonBorderColor);
	}

	void PopOutlinerButtonStyle()
	{
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);
	}

	bool DrawIconLabelButton(const char* Id, const char* IconKey, const char* Label, const ImVec2& Size, ImU32 Tint = IM_COL32_WHITE)
	{
		const float Width = Size.x > 0.0f ? Size.x : 140.0f;
		const float Height = Size.y > 0.0f ? Size.y : 24.0f;
		ImGui::InvisibleButton(Id, ImVec2(Width, Height));
		const bool bClicked = ImGui::IsItemClicked();
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const bool bHeld = ImGui::IsItemActive();
		const bool bHovered = ImGui::IsItemHovered();
		ImU32 BackgroundColor = ImGui::GetColorU32(ImGuiCol_Button);
		if (bHeld)
		{
			BackgroundColor = ImGui::GetColorU32(ImGuiCol_ButtonActive);
		}
		else if (bHovered)
		{
			BackgroundColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
		}
		DrawList->AddRectFilled(Min, Max, BackgroundColor, ImGui::GetStyle().FrameRounding);
		DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding);
		float CursorX = Min.x + 8.0f;
		const float CenterY = Min.y + (Max.y - Min.y) * 0.5f;

		if (ID3D11ShaderResourceView* Icon = GetEditorIcon(IconKey))
		{
			const float IconSize = (std::min)(ImGui::GetItemRectSize().y - 8.0f, 14.0f);
			DrawList->AddImage(
				reinterpret_cast<ImTextureID>(Icon),
				ImVec2(CursorX, CenterY - IconSize * 0.5f),
				ImVec2(CursorX + IconSize, CenterY + IconSize * 0.5f),
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f),
				Tint);
			CursorX += IconSize + 8.0f;
		}

		const ImVec2 LabelSize = ImGui::CalcTextSize(Label);
		DrawList->AddText(
			ImVec2(CursorX, CenterY - LabelSize.y * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text),
			Label);

		return bClicked;
	}

	bool DrawLockToggle(const char* Id, bool bLocked)
	{
		const ImVec2 Size(20.0f, 16.0f);
		ImGui::InvisibleButton(Id, Size);
		const bool bClicked = ImGui::IsItemClicked();

		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const float IconSize = 13.0f;
		const float IconX = Min.x + ((Max.x - Min.x) - IconSize) * 0.5f;
		const float IconY = Min.y + ((Max.y - Min.y) - IconSize) * 0.5f;

		if (bLocked)
		{
			if (ID3D11ShaderResourceView* Icon = GetEditorIcon("Editor.Icon.Locked"))
			{
				ImGui::GetWindowDrawList()->AddImage(
					reinterpret_cast<ImTextureID>(Icon),
					ImVec2(IconX, IconY),
					ImVec2(IconX + IconSize, IconY + IconSize),
					ImVec2(0.0f, 0.0f),
					ImVec2(1.0f, 1.0f),
					IM_COL32(255, 255, 255, 255));
			}
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", bLocked ? "Unlock Actor Movement" : "Lock Actor Movement");
		}

		return bClicked;
	}

	bool IsFolderNameUsed(const UWorld* World, const FString& FolderName, const FString& IgnoredFolderName = FString())
	{
		if (!World || FolderName.empty())
		{
			return false;
		}

		if (const ULevel* PersistentLevel = World->GetPersistentLevel())
		{
			for (const FString& ExistingFolderName : PersistentLevel->GetOutlinerFolders())
			{
				if (ExistingFolderName == FolderName && ExistingFolderName != IgnoredFolderName)
				{
					return true;
				}
			}
		}

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor)
			{
				continue;
			}

			const FString& ExistingFolderName = Actor->GetFolderPath();
			if (ExistingFolderName == FolderName && ExistingFolderName != IgnoredFolderName)
			{
				return true;
			}
		}

		return false;
	}

}

void FEditorOutlinerWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorOutlinerWidget::Render(float DeltaTime)
{
	if (!EditorEngine)
	{
		return;
	}

	(void)DeltaTime;
	ImGui::SetNextWindowSize(ImVec2(520.0f, 420.0f), ImGuiCond_Once);

	FEditorSettings& Settings = FEditorSettings::Get();
	if (!Settings.UI.bScene)
	{
		return;
	}

	constexpr const char* PanelIconKey = "Editor.Icon.Panel.Outliner";
	const std::string WindowTitle = EditorPanelTitleUtils::MakeClosablePanelTitle("Outliner", PanelIconKey);
	const bool bIsOpen = ImGui::Begin(WindowTitle.c_str());
	EditorPanelTitleUtils::DrawPanelTitleIcon(PanelIconKey);
	EditorPanelTitleUtils::DrawSmallPanelCloseButton("    Outliner", Settings.UI.bScene, "x##CloseOutliner");
	if (!bIsOpen)
	{
		ImGui::End();
		return;
	}
	EditorPanelTitleUtils::ApplyPanelContentTopInset();

	UWorld* World = EditorEngine->GetWorld();
	std::set<FString> ActorTypes;
	if (World)
	{
		for (AActor* Actor : World->GetActors())
		{
			if (Actor)
			{
				ActorTypes.insert(Actor->GetClass()->GetName());
			}
		}
	}

	PushOutlinerButtonStyle();
	if (ImGui::Button("##OutlinerFilter", ImVec2(34.0f, OutlinerToolbarButtonHeight)))
	{
		ImGui::OpenPopup("##OutlinerTypeFilterPopup");
	}
	if (ID3D11ShaderResourceView* FilterIcon = GetEditorIcon("Editor.Icon.Filter"))
	{
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Size = ImGui::GetItemRectSize();
		const float IconSize = 16.0f;
		ImGui::GetWindowDrawList()->AddImage(
			reinterpret_cast<ImTextureID>(FilterIcon),
			ImVec2(Min.x + (Size.x - IconSize) * 0.5f, Min.y + (Size.y - IconSize) * 0.5f),
			ImVec2(Min.x + (Size.x + IconSize) * 0.5f, Min.y + (Size.y + IconSize) * 0.5f));
	}
	if (ImGui::BeginPopup("##OutlinerTypeFilterPopup"))
	{
		const bool bAllSelected = (TypeFilter == "All Types");
		DrawPopupSectionHeader("ALL TYPES");
		if (ImGui::Selectable("ALL TYPES", bAllSelected))
		{
			TypeFilter = "All Types";
		}
		if (bAllSelected)
		{
			ImGui::SetItemDefaultFocus();
		}

		auto DrawActorTypeSection = [&](const char* SectionLabel, EActorTypeSection Section)
		{
			bool bHasAny = false;
			for (const FString& TypeName : ActorTypes)
			{
				if (GetActorTypeSection(TypeName) == Section)
				{
					bHasAny = true;
					break;
				}
			}

			if (!bHasAny)
			{
				return;
			}

			DrawPopupSectionHeader(SectionLabel);
			for (const FString& TypeName : ActorTypes)
			{
				if (GetActorTypeSection(TypeName) != Section)
				{
					continue;
				}

				const bool bSelected = (TypeFilter == TypeName);
				if (ImGui::Selectable(TypeName.c_str(), bSelected))
				{
					TypeFilter = TypeName;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
		};

		DrawActorTypeSection("LIGHTS", EActorTypeSection::Lights);
		DrawActorTypeSection("GAMEPLAY", EActorTypeSection::Gameplay);
		DrawActorTypeSection("SCENE", EActorTypeSection::Scene);
		DrawActorTypeSection("OTHER", EActorTypeSection::Other);
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	const float AddButtonWidth = 34.0f;
	const float SearchWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x - AddButtonWidth - ImGui::GetStyle().ItemSpacing.x);
	DrawSearchInputWithIcon("##OutlinerSearch", "Search...", SearchBuffer, sizeof(SearchBuffer), SearchWidth);
	ImGui::SameLine();
	if (ImGui::Button("##OutlinerCreateFolder", ImVec2(AddButtonWidth, OutlinerToolbarButtonHeight)))
	{
		NewFolderBuffer[0] = '\0';
		ImGui::OpenPopup("##NewOutlinerFolder");
	}
	DrawCenteredButtonIcon("Editor.Icon.CreateFolder", 16.0f);
	if (ImGui::BeginPopup("##NewOutlinerFolder"))
	{
		ImGui::InputText("Folder Name", NewFolderBuffer, sizeof(NewFolderBuffer));
		UWorld* PopupWorld = EditorEngine ? EditorEngine->GetWorld() : nullptr;
		const FString NewFolderName = NewFolderBuffer;
		const bool bCanCreate = NewFolderBuffer[0] != '\0'
			&& PopupWorld
			&& PopupWorld->GetPersistentLevel()
			&& !IsFolderNameUsed(PopupWorld, NewFolderName);
		if (!bCanCreate)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Create"))
		{
			if (ULevel* PersistentLevel = PopupWorld ? PopupWorld->GetPersistentLevel() : nullptr)
			{
				EditorEngine->BeginTrackedSceneChange();
				PersistentLevel->AddOutlinerFolder(NewFolderName);
				EditorEngine->CommitTrackedSceneChange();
			}
			ImGui::CloseCurrentPopup();
		}
		if (!bCanCreate)
		{
			ImGui::EndDisabled();
		}
		ImGui::EndPopup();
	}
	PopOutlinerButtonStyle();
	ImGui::Separator();
	const float FooterHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y + 8.0f;
	ImGui::BeginChild("##OutlinerTableRegion", ImVec2(0.0f, -FooterHeight), false);
	RenderActorOutliner();
	ImGui::EndChild();

	ImGui::Separator();
	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	ImGui::Text("%d Actors (%d selected)",
		EditorEngine->GetWorld() ? static_cast<int32>(EditorEngine->GetWorld()->GetActors().ToArray().size()) : 0,
		static_cast<int32>(Selection.GetSelectedActors().size()));

	const bool bOutlinerFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (bOutlinerFocused && !ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false))
	{
		SelectAllVisibleActors();
	}

	const bool bCanSelectAll = !ValidActorIndices.empty();
	const float SelectAllWidth = 92.0f;
	const bool bCanRemove = !Selection.GetSelectedActors().empty();
	const float RemoveWidth = 136.0f;
	ImGui::SameLine((std::max)(0.0f, ImGui::GetContentRegionAvail().x - SelectAllWidth - RemoveWidth - ImGui::GetStyle().ItemSpacing.x));
	if (!bCanSelectAll)
	{
		ImGui::BeginDisabled();
	}
	PushOutlinerButtonStyle();
	if (ImGui::Button("Select All", ImVec2(SelectAllWidth, OutlinerFooterButtonHeight)))
	{
		SelectAllVisibleActors();
	}
	PopOutlinerButtonStyle();
	if (!bCanSelectAll)
	{
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	if (!bCanRemove)
	{
		ImGui::BeginDisabled();
	}
	PushOutlinerButtonStyle();
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.16f, 0.16f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.20f, 0.20f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.34f, 0.12f, 0.12f, 1.0f));
	if (DrawIconLabelButton("##DeleteSelectedActors", "Editor.Icon.Delete", "Delete Selected", ImVec2(RemoveWidth, OutlinerFooterButtonHeight), IM_COL32(255, 225, 225, 255)))
	{
		EditorEngine->BeginTrackedSceneChange();
		Selection.DeleteSelectedActors();
		EditorEngine->InvalidateOcclusionResults();
		EditorEngine->CommitTrackedSceneChange();
	}
	ImGui::PopStyleColor(3);
	PopOutlinerButtonStyle();
	if (!bCanRemove)
	{
		ImGui::EndDisabled();
	}
	ImGui::End();
}

void FEditorOutlinerWidget::SelectAllVisibleActors()
{
	if (!EditorEngine)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}

	const TArray<AActor*> Actors = World->GetActors().ToArray();
	TArray<AActor*> ActorsToSelect;
	ActorsToSelect.reserve(ValidActorIndices.size());

	for (int32 ActorIndex : ValidActorIndices)
	{
		if (ActorIndex >= 0 && ActorIndex < static_cast<int32>(Actors.size()) && Actors[ActorIndex])
		{
			ActorsToSelect.push_back(Actors[ActorIndex]);
		}
	}

	EditorEngine->GetSelectionManager().SelectActors(ActorsToSelect);
}

void FEditorOutlinerWidget::StartActorRename(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	CancelFolderRename();
	RenamingActor = Actor;
	bFocusRenameInput = true;
	const FString CurrentName = Actor->GetFName().ToString().empty() ? Actor->GetClass()->GetName() : Actor->GetFName().ToString();
	strncpy_s(RenameBuffer, CurrentName.c_str(), _TRUNCATE);
}

void FEditorOutlinerWidget::StartFolderRename(const FString& FolderName)
{
	if (FolderName.empty())
	{
		return;
	}

	CancelActorRename();
	RenamingFolder = FolderName;
	bFocusRenameInput = true;
	strncpy_s(FolderRenameBuffer, FolderName.c_str(), _TRUNCATE);
}

void FEditorOutlinerWidget::CommitActorRename()
{
	if (!RenamingActor)
	{
		CancelActorRename();
		return;
	}

	FString NewName = RenameBuffer;
	if (NewName.empty())
	{
		NewName = RenamingActor->GetClass()->GetName();
	}

	if (EditorEngine)
	{
		EditorEngine->BeginTrackedSceneChange();
		RenamingActor->SetFName(FName(NewName));
		EditorEngine->CommitTrackedSceneChange();
	}

	CancelActorRename();
}

void FEditorOutlinerWidget::CommitFolderRename()
{
	if (!EditorEngine || RenamingFolder.empty())
	{
		CancelFolderRename();
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	ULevel* PersistentLevel = World ? World->GetPersistentLevel() : nullptr;
	if (!PersistentLevel)
	{
		CancelFolderRename();
		return;
	}

	FString NewFolderName = FolderRenameBuffer;
	if (NewFolderName.empty() || NewFolderName == RenamingFolder || IsFolderNameUsed(World, NewFolderName, RenamingFolder))
	{
		CancelFolderRename();
		return;
	}

	EditorEngine->BeginTrackedSceneChange();
	PersistentLevel->RenameOutlinerFolder(RenamingFolder, NewFolderName);
	for (AActor* Actor : World->GetActors())
	{
		if (Actor && Actor->GetFolderPath() == RenamingFolder)
		{
			Actor->SetFolderPath(NewFolderName);
		}
	}
	EditorEngine->CommitTrackedSceneChange();

	CancelFolderRename();
}

void FEditorOutlinerWidget::CancelActorRename()
{
	RenamingActor = nullptr;
	bFocusRenameInput = false;
	RenameBuffer[0] = '\0';
}

void FEditorOutlinerWidget::CancelFolderRename()
{
	RenamingFolder.clear();
	bFocusRenameInput = false;
	FolderRenameBuffer[0] = '\0';
}

bool FEditorOutlinerWidget::DrawVisibilityToggle(const char* Id, bool bVisible) const
{
	const ImVec2 Size(20.0f, 16.0f);
	if (!ImGui::InvisibleButton(Id, Size))
	{
	}

	const bool bClicked = ImGui::IsItemClicked();
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	if (!bVisible)
	{
		const ImU32 Stroke = IM_COL32(255, 255, 255, 255);
		const ImU32 Fill = IM_COL32(255, 255, 255, 32);
		DrawList->AddEllipseFilled(Center, ImVec2(6.0f, 4.0f), Fill);
		DrawList->AddEllipse(Center, ImVec2(6.0f, 4.0f), Stroke, 0.0f, 24, 1.5f);
		DrawList->AddCircleFilled(Center, 1.8f, Stroke, 12);
		DrawList->AddLine(ImVec2(Min.x + 4.0f, Max.y - 3.0f), ImVec2(Max.x - 4.0f, Min.y + 3.0f), Stroke, 1.5f);
	}

	return bClicked;
}

void FEditorOutlinerWidget::RenderActorOutliner()
{
	SCOPE_STAT_CAT("OutlinerWidget::ActorOutliner", "5_UI");

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}

	const TArray<AActor*> Actors = World->GetActors().ToArray();
	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	const float VisibilityCellWidth = 20.0f;
	const float LockCellWidth = 20.0f;
	const float TableInsetX = 8.0f;
	const float ChildItemIndentX = 30.0f;
	std::unordered_map<FString, TArray<int32>> FolderToActors;
	TArray<int32> RootActorIndices;
	TArray<FString> FolderNames;
	ULevel* PersistentLevel = World->GetPersistentLevel();
	ValidActorIndices.clear();

	if (PersistentLevel)
	{
		FolderNames = PersistentLevel->GetOutlinerFolders();
	}

	for (int32 i = 0; i < static_cast<int32>(Actors.size()); ++i)
	{
		AActor* Actor = Actors[i];
		if (!Actor)
		{
			continue;
		}

		const FString Label = Actor->GetFName().ToString().empty() ? Actor->GetClass()->GetName() : Actor->GetFName().ToString();
		const FString Type = Actor->GetClass()->GetName();
		const FString Folder = Actor->GetFolderPath();

		if (TypeFilter != "All Types" && Type != TypeFilter)
		{
			continue;
		}

		if (SearchBuffer[0] != '\0')
		{
			const bool bMatchesLabel = strstr(Label.c_str(), SearchBuffer) != nullptr;
			const bool bMatchesType = strstr(Type.c_str(), SearchBuffer) != nullptr;
			const bool bMatchesFolder = !Folder.empty() && strstr(Folder.c_str(), SearchBuffer) != nullptr;
			if (!bMatchesLabel && !bMatchesType && !bMatchesFolder)
			{
				continue;
			}
		}

		if (Folder.empty())
		{
			RootActorIndices.push_back(i);
		}
		else
		{
			if (!FolderToActors.contains(Folder))
			{
				if (std::find(FolderNames.begin(), FolderNames.end(), Folder) == FolderNames.end())
				{
					FolderNames.push_back(Folder);
				}
			}
			FolderToActors[Folder].push_back(i);
		}

		ValidActorIndices.push_back(i);
	}

	TArray<AActor*> VisibleActors;
	VisibleActors.reserve(ValidActorIndices.size());
	for (int32 ActorIndex : ValidActorIndices)
	{
		if (ActorIndex >= 0 && ActorIndex < static_cast<int32>(Actors.size()) && Actors[ActorIndex])
		{
			VisibleActors.push_back(Actors[ActorIndex]);
		}
	}

	ImGui::Indent(TableInsetX);
	if (!ImGui::BeginTable("##OutlinerTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_PadOuterX, ImVec2(0.0f, 0.0f)))
	{
		ImGui::Unindent(TableInsetX);
		return;
	}

	ImGui::TableSetupColumn("##Visibility", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 28.0f);
	ImGui::TableSetupColumn("##Lock", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 28.0f);
	ImGui::TableSetupColumn("Item Label", ImGuiTableColumnFlags_WidthStretch, 260.0f);
	ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 140.0f);
	ImGui::TableHeadersRow();
	ImGui::TableSetColumnIndex(0);
	{
	const ImVec2 HeaderMin = ImGui::GetCursorScreenPos();
	const float HeaderWidth = ImGui::GetColumnWidth();
		const float IconX = HeaderMin.x + floorf((HeaderWidth - VisibilityCellWidth) * 0.5f);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Center(IconX + VisibilityCellWidth * 0.5f, HeaderMin.y + ImGui::GetTextLineHeight() * 0.5f + 2.0f);
		const ImU32 Stroke = IM_COL32(255, 255, 255, 255);
		DrawList->AddEllipse(Center, ImVec2(6.0f, 4.0f), Stroke, 0.0f, 24, 1.3f);
		DrawList->AddCircleFilled(Center, 1.8f, Stroke, 12);
	}
	ImGui::TableSetColumnIndex(1);
	{
		const ImVec2 HeaderMin = ImGui::GetCursorScreenPos();
		const float HeaderWidth = ImGui::GetColumnWidth();
		const float IconX = HeaderMin.x + floorf((HeaderWidth - LockCellWidth) * 0.5f);
		if (ID3D11ShaderResourceView* LockIcon = GetEditorIcon("Editor.Icon.Locked"))
		{
			ImGui::GetWindowDrawList()->AddImage(
				reinterpret_cast<ImTextureID>(LockIcon),
				ImVec2(IconX + 3.0f, HeaderMin.y + 2.0f),
				ImVec2(IconX + 17.0f, HeaderMin.y + 16.0f),
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f),
				IM_COL32(255, 255, 255, 255));
		}
	}

	auto RenderActorRow = [&](AActor* Actor, int32 StableIndex, bool bIndented)
	{
		if (!Actor)
		{
			return;
		}

		const FString Label = Actor->GetFName().ToString().empty() ? Actor->GetClass()->GetName() : Actor->GetFName().ToString();
		const FString Type = Actor->GetClass()->GetName();
		const bool bIsSelected = Selection.IsSelected(Actor);
		const bool bIsRenaming = RenamingActor == Actor;

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + floorf((ImGui::GetColumnWidth() - VisibilityCellWidth) * 0.5f));
		const FString VisibilityId = "##Vis" + std::to_string(StableIndex);
		if (DrawVisibilityToggle(VisibilityId.c_str(), Actor->IsVisible()))
		{
			Actor->SetVisible(!Actor->IsVisible());
		}

		ImGui::TableSetColumnIndex(1);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + floorf((ImGui::GetColumnWidth() - LockCellWidth) * 0.5f));
		const FString LockId = "##Lock" + std::to_string(StableIndex);
		if (DrawLockToggle(LockId.c_str(), Actor->IsActorMovementLocked()))
		{
			Actor->SetActorMovementLocked(!Actor->IsActorMovementLocked());
		}

		ImGui::TableSetColumnIndex(2);
		if (bIndented)
		{
			ImGui::Indent(ChildItemIndentX);
		}
		ImGui::PushStyleColor(ImGuiCol_Text, OutlinerItemLabelColor);
		ImGui::PushStyleColor(ImGuiCol_Header, OutlinerSelectionHeaderColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, OutlinerSelectionHeaderHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, OutlinerSelectionHeaderActiveColor);
		if (bIsRenaming)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
			ImGui::SetNextItemWidth((std::max)(120.0f, ImGui::GetContentRegionAvail().x));
			if (bFocusRenameInput)
			{
				ImGui::SetKeyboardFocusHere();
				bFocusRenameInput = false;
			}

			const bool bSubmitted = ImGui::InputText(
				("##OutlinerRename" + std::to_string(StableIndex)).c_str(),
				RenameBuffer,
				sizeof(RenameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (bSubmitted || ImGui::IsItemDeactivatedAfterEdit())
			{
				CommitActorRename();
			}
			else if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				CancelActorRename();
			}
			ImGui::PopStyleVar();
		}
		else if (ImGui::Selectable((Label + "##OutlinerRow" + std::to_string(StableIndex)).c_str(), bIsSelected, ImGuiSelectableFlags_SpanAllColumns))
		{
			if (ImGui::GetIO().KeyShift)
			{
				Selection.AddSelect(Actor);
			}
			else if (ImGui::GetIO().KeyCtrl)
			{
				Selection.ToggleSelect(Actor);
			}
			else
			{
				Selection.Select(Actor);
			}
		}
		ImGui::PopStyleColor(3);
		ImGui::PopStyleColor();
		if (ImGui::BeginPopupContextItem((Label + "##OutlinerContext" + std::to_string(StableIndex)).c_str()))
		{
			if (!Selection.IsSelected(Actor))
			{
				Selection.Select(Actor);
			}

			if (DrawIconLabelButton("##FocusActorContext", "Editor.Icon.Search", "Focus", ImVec2(120.0f, 0.0f)))
			{
				EditorEngine->FocusActorInViewport(Actor);
				ImGui::CloseCurrentPopup();
			}

			if (DrawIconLabelButton("##RenameActorContext", "Editor.Icon.Actor", "Rename", ImVec2(120.0f, 0.0f)))
			{
				StartActorRename(Actor);
				ImGui::CloseCurrentPopup();
			}

			if (DrawIconLabelButton("##DeleteActorContext", "Editor.Icon.Delete", "Delete", ImVec2(120.0f, 0.0f), IM_COL32(255, 225, 225, 255)))
			{
				EditorEngine->BeginTrackedSceneChange();
				Selection.DeleteSelectedActors();
				EditorEngine->InvalidateOcclusionResults();
				EditorEngine->CommitTrackedSceneChange();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			FOutlinerDragPayload Payload;
			Payload.ItemType = FOutlinerDragPayload::EItemType::Actor;
			Payload.Actor = Actor;
			ImGui::SetDragDropPayload("OUTLINER_ACTOR_ROW", &Payload, sizeof(Payload));
			ImGui::TextUnformatted(Label.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR_ROW"))
			{
				const FOutlinerDragPayload* DragPayload = static_cast<const FOutlinerDragPayload*>(Payload->Data);
				if (DragPayload && DragPayload->ItemType == FOutlinerDragPayload::EItemType::Actor && DragPayload->Actor)
				{
					HandleActorDrop(DragPayload->Actor, Actor);
				}
			}
			ImGui::EndDragDropTarget();
		}
		if (bIndented)
		{
			ImGui::Unindent(ChildItemIndentX);
		}

		ImGui::TableSetColumnIndex(3);
		if (bIsSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
			ImGui::TextUnformatted(Type.c_str());
			ImGui::PopStyleColor();
		}
		else
		{
			ImGui::TextDisabled("%s", Type.c_str());
		}
	};

	for (const FString& FolderName : FolderNames)
	{
		const TArray<int32>& FolderActorIndices = FolderToActors[FolderName];
		const bool bAllActorsVisible = std::all_of(FolderActorIndices.begin(), FolderActorIndices.end(), [&](int32 ActorIndex)
		{
			return ActorIndex >= 0
				&& ActorIndex < static_cast<int32>(Actors.size())
				&& Actors[ActorIndex]
				&& Actors[ActorIndex]->IsVisible();
		});
		const bool bAllActorsSelected = !FolderActorIndices.empty()
			&& std::all_of(FolderActorIndices.begin(), FolderActorIndices.end(), [&](int32 ActorIndex)
		{
			return ActorIndex >= 0
				&& ActorIndex < static_cast<int32>(Actors.size())
				&& Actors[ActorIndex]
				&& Selection.IsSelected(Actors[ActorIndex]);
		});

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + floorf((ImGui::GetColumnWidth() - VisibilityCellWidth) * 0.5f));
		const FString FolderVisibilityId = "##FolderVis" + FolderName;
		if (DrawVisibilityToggle(FolderVisibilityId.c_str(), bAllActorsVisible))
		{
			const bool bNewVisibility = !bAllActorsVisible;
			for (int32 ActorIndex : FolderActorIndices)
			{
				if (ActorIndex >= 0 && ActorIndex < static_cast<int32>(Actors.size()) && Actors[ActorIndex])
				{
					Actors[ActorIndex]->SetVisible(bNewVisibility);
				}
			}
		}
		ImGui::TableSetColumnIndex(1);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + floorf((ImGui::GetColumnWidth() - LockCellWidth) * 0.5f));
		const bool bAllActorsLocked = !FolderActorIndices.empty()
			&& std::all_of(FolderActorIndices.begin(), FolderActorIndices.end(), [&](int32 ActorIndex)
		{
			return ActorIndex >= 0
				&& ActorIndex < static_cast<int32>(Actors.size())
				&& Actors[ActorIndex]
				&& Actors[ActorIndex]->IsActorMovementLocked();
		});
		const FString FolderLockId = "##FolderLock" + FolderName;
		if (DrawLockToggle(FolderLockId.c_str(), bAllActorsLocked))
		{
			const bool bNewLocked = !bAllActorsLocked;
			for (int32 ActorIndex : FolderActorIndices)
			{
				if (ActorIndex >= 0 && ActorIndex < static_cast<int32>(Actors.size()) && Actors[ActorIndex])
				{
					Actors[ActorIndex]->SetActorMovementLocked(bNewLocked);
				}
			}
		}
		ImGui::TableSetColumnIndex(2);
		ImGuiTreeNodeFlags FolderFlags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (bAllActorsSelected)
		{
			FolderFlags |= ImGuiTreeNodeFlags_Selected;
		}
		const bool bIsRenamingFolder = RenamingFolder == FolderName;
		bool bOpen = false;

		if (bIsRenamingFolder)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
			ImGui::SetNextItemWidth((std::max)(120.0f, ImGui::GetContentRegionAvail().x));
			if (bFocusRenameInput)
			{
				ImGui::SetKeyboardFocusHere();
				bFocusRenameInput = false;
			}

			const bool bSubmitted = ImGui::InputText(
				("##FolderRename" + FolderName).c_str(),
				FolderRenameBuffer,
				sizeof(FolderRenameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (bSubmitted || ImGui::IsItemDeactivatedAfterEdit())
			{
				CommitFolderRename();
			}
			else if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				CancelFolderRename();
			}
			ImGui::PopStyleVar();
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, OutlinerItemLabelColor);
			ImGui::PushStyleColor(ImGuiCol_Text, OutlinerFolderArrowColor);
			ImGui::PushStyleColor(ImGuiCol_Header, OutlinerSelectionHeaderColor);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, OutlinerSelectionHeaderHoveredColor);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, OutlinerSelectionHeaderActiveColor);
			bOpen = ImGui::TreeNodeEx((FolderName + "##Folder").c_str(), FolderFlags, "%s", FolderName.c_str());
			ImGui::PopStyleColor(3);
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				TArray<AActor*> FolderActors;
				FolderActors.reserve(FolderActorIndices.size());
				for (int32 ActorIndex : FolderActorIndices)
				{
					if (ActorIndex >= 0 && ActorIndex < static_cast<int32>(Actors.size()) && Actors[ActorIndex])
					{
						FolderActors.push_back(Actors[ActorIndex]);
					}
				}

				if (ImGui::GetIO().KeyCtrl)
				{
					for (AActor* FolderActor : FolderActors)
					{
						Selection.ToggleSelect(FolderActor);
					}
				}
				else
				{
					Selection.SelectActors(FolderActors);
				}
			}

			if (ID3D11ShaderResourceView* FolderIcon = GetEditorIcon(bOpen ? "Editor.Icon.FolderOpen" : "Editor.Icon.FolderClosed"))
			{
				const ImVec2 Min = ImGui::GetItemRectMin();
				const float IconSize = 14.0f;
				const float LabelStartX = Min.x + ImGui::GetTreeNodeToLabelSpacing();
				const float X = LabelStartX - IconSize - 4.0f;
				const float Y = Min.y + (ImGui::GetItemRectSize().y - IconSize) * 0.5f;
				ImGui::GetWindowDrawList()->AddImage(
					reinterpret_cast<ImTextureID>(FolderIcon),
					ImVec2(X, Y),
					ImVec2(X + IconSize, Y + IconSize),
					ImVec2(0.0f, 0.0f),
					ImVec2(1.0f, 1.0f),
					OutlinerFolderIconTint);
			}

			if (ImGui::BeginPopupContextItem((FolderName + "##FolderContext").c_str()))
			{
				if (DrawIconLabelButton("##RenameFolderContext", "Editor.Icon.Actor", "Rename", ImVec2(120.0f, 0.0f)))
				{
					StartFolderRename(FolderName);
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
			{
				FOutlinerDragPayload Payload;
				Payload.ItemType = FOutlinerDragPayload::EItemType::Folder;
				strncpy_s(Payload.FolderName, FolderName.c_str(), _TRUNCATE);
				ImGui::SetDragDropPayload("OUTLINER_ACTOR_ROW", &Payload, sizeof(Payload));
				ImGui::TextUnformatted(FolderName.c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR_ROW"))
				{
					const FOutlinerDragPayload* DragPayload = static_cast<const FOutlinerDragPayload*>(Payload->Data);
					if (DragPayload)
					{
						if (DragPayload->ItemType == FOutlinerDragPayload::EItemType::Actor && DragPayload->Actor)
						{
							HandleFolderDrop(DragPayload->Actor, FolderName);
						}
						else if (DragPayload->ItemType == FOutlinerDragPayload::EItemType::Folder && DragPayload->FolderName[0] != '\0')
						{
							HandleFolderDrop(DragPayload->FolderName, FolderName);
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
		}
		ImGui::TableSetColumnIndex(3);
		ImGui::TextDisabled("%d Actors", static_cast<int32>(FolderActorIndices.size()));
		if (!bIsRenamingFolder && bOpen)
		{
			for (int32 ActorIndex : FolderActorIndices)
			{
				RenderActorRow(Actors[ActorIndex], ActorIndex, true);
			}
			ImGui::TreePop();
		}
	}

	for (int32 ActorIndex : RootActorIndices)
	{
		RenderActorRow(Actors[ActorIndex], ActorIndex, false);
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR_ROW"))
		{
			const FOutlinerDragPayload* DragPayload = static_cast<const FOutlinerDragPayload*>(Payload->Data);
			if (DragPayload)
			{
				if (DragPayload->ItemType == FOutlinerDragPayload::EItemType::Actor && DragPayload->Actor)
				{
					HandleRootDrop(DragPayload->Actor);
				}
				else if (DragPayload->ItemType == FOutlinerDragPayload::EItemType::Folder && DragPayload->FolderName[0] != '\0')
				{
					HandleRootDrop(DragPayload->FolderName);
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::EndTable();
	ImGui::Unindent(TableInsetX);
}

void FEditorOutlinerWidget::HandleActorDrop(AActor* DraggedActor, AActor* TargetActor) const
{
	if (!EditorEngine || !DraggedActor || !TargetActor || DraggedActor == TargetActor)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}

	EditorEngine->BeginTrackedSceneChange();
	DraggedActor->SetFolderPath(TargetActor->GetFolderPath());
	World->MoveActorBefore(DraggedActor, TargetActor);
	EditorEngine->CommitTrackedSceneChange();
}

void FEditorOutlinerWidget::HandleFolderDrop(AActor* DraggedActor, const FString& FolderName) const
{
	if (!EditorEngine || !DraggedActor)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}

	ULevel* PersistentLevel = World->GetPersistentLevel();

	const TArray<AActor*> Actors = World->GetActors().ToArray();
	size_t InsertIndex = Actors.size();
	bool bFoundFolder = false;

	for (size_t Index = 0; Index < Actors.size(); ++Index)
	{
		AActor* Actor = Actors[Index];
		if (!Actor)
		{
			continue;
		}

		if (Actor->GetFolderPath() == FolderName)
		{
			InsertIndex = Index + 1;
			bFoundFolder = true;
		}
	}

	EditorEngine->BeginTrackedSceneChange();
	if (PersistentLevel)
	{
		PersistentLevel->AddOutlinerFolder(FolderName);
	}
	DraggedActor->SetFolderPath(FolderName);
	if (bFoundFolder)
	{
		World->MoveActorToIndex(DraggedActor, InsertIndex);
	}
	EditorEngine->CommitTrackedSceneChange();
}

void FEditorOutlinerWidget::HandleFolderDrop(const FString& DraggedFolder, const FString& TargetFolder) const
{
	if (!EditorEngine || DraggedFolder.empty() || TargetFolder.empty() || DraggedFolder == TargetFolder)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	ULevel* PersistentLevel = World ? World->GetPersistentLevel() : nullptr;
	if (!PersistentLevel)
	{
		return;
	}

	EditorEngine->BeginTrackedSceneChange();
	PersistentLevel->MoveOutlinerFolderBefore(DraggedFolder, TargetFolder);
	EditorEngine->CommitTrackedSceneChange();
}

void FEditorOutlinerWidget::HandleRootDrop(AActor* DraggedActor) const
{
	if (!EditorEngine || !DraggedActor)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}

	const TArray<AActor*> Actors = World->GetActors().ToArray();
	size_t RootInsertIndex = 0;

	for (size_t Index = 0; Index < Actors.size(); ++Index)
	{
		AActor* Actor = Actors[Index];
		if (!Actor)
		{
			continue;
		}

		if (Actor->GetFolderPath().empty())
		{
			RootInsertIndex = Index + 1;
		}
	}

	EditorEngine->BeginTrackedSceneChange();
	DraggedActor->SetFolderPath("");
	World->MoveActorToIndex(DraggedActor, RootInsertIndex);
	EditorEngine->CommitTrackedSceneChange();
}

void FEditorOutlinerWidget::HandleRootDrop(const FString& DraggedFolder) const
{
	if (!EditorEngine || DraggedFolder.empty())
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	ULevel* PersistentLevel = World ? World->GetPersistentLevel() : nullptr;
	if (!PersistentLevel)
	{
		return;
	}

	EditorEngine->BeginTrackedSceneChange();
	PersistentLevel->MoveOutlinerFolderToIndex(DraggedFolder, PersistentLevel->GetOutlinerFolders().size());
	EditorEngine->CommitTrackedSceneChange();
}
