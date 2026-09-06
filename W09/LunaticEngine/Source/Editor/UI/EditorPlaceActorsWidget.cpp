#include "Editor/UI/EditorPlaceActorsWidget.h"

#include "Component/CameraComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/UI/EditorPanelTitleUtils.h"
#include "Editor/Settings/EditorSettings.h"
#include "Core/AsciiUtils.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Resource/ResourceManager.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <string>

namespace
{
	using EPlaceType = FLevelViewportLayout::EViewportPlaceActorType;
	using ECategory = FEditorPlaceActorsWidget::EPlaceActorCategory;
	using FEntry = FEditorPlaceActorsWidget::FPlaceActorEntry;

	constexpr ImVec4 PlacePanelBg = ImVec4(28.0f / 255.0f, 28.0f / 255.0f, 28.0f / 255.0f, 1.0f);
	constexpr ImVec4 PlaceSidebarButton = ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
	constexpr ImVec4 PlaceSidebarButtonHover = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
	constexpr ImVec4 PlaceSidebarButtonActive = ImVec4(68.0f / 255.0f, 68.0f / 255.0f, 68.0f / 255.0f, 1.0f);
	constexpr ImVec4 PlaceCategorySelected = EditorAccentColor::Value;
	constexpr ImVec4 PlaceCategorySelectedHover = EditorAccentColor::Value;
	constexpr ImVec4 PlaceCategorySelectedActive = EditorAccentColor::Value;
	constexpr ImVec4 PlaceEntryBg = ImVec4(63.0f / 255.0f, 63.0f / 255.0f, 63.0f / 255.0f, 1.0f);
	constexpr ImVec4 PlaceEntryHover = ImVec4(78.0f / 255.0f, 78.0f / 255.0f, 78.0f / 255.0f, 1.0f);
	constexpr ImVec4 PlaceEntryActive = ImVec4(90.0f / 255.0f, 90.0f / 255.0f, 90.0f / 255.0f, 1.0f);
	constexpr ImVec4 PlaceBorder = ImVec4(70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f, 1.0f);
	constexpr ImVec4 PlaceSearchBg = ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f);

	FString GetEditorPathResource(const char* Key)
	{
		return FResourceManager::Get().ResolvePath(FName(Key));
	}

	ID3D11ShaderResourceView* GetPlaceActorsCategoryIcon(FEditorPlaceActorsWidget::EPlaceActorCategory Category)
	{
		switch (Category)
		{
		case FEditorPlaceActorsWidget::EPlaceActorCategory::Basic:
			return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.PlaceActors.Basic")).Get();
		case FEditorPlaceActorsWidget::EPlaceActorCategory::Text:
			return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.ScreenText")).Get();
		case FEditorPlaceActorsWidget::EPlaceActorCategory::UI:
			return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.ToolIcon.WorldSpace")).Get();
		case FEditorPlaceActorsWidget::EPlaceActorCategory::Lights:
			return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.PlaceActors.Lights")).Get();
		case FEditorPlaceActorsWidget::EPlaceActorCategory::Shapes:
			return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.PlaceActors.Shapes")).Get();
		case FEditorPlaceActorsWidget::EPlaceActorCategory::VFX:
			return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.PlaceActors.VFX")).Get();
		default:
			return nullptr;
		}
	}

	ID3D11ShaderResourceView* GetPlaceActorEntryIcon(const FEntry& Entry)
	{
		if (!Entry.IconKey || Entry.IconKey[0] == '\0')
		{
			return nullptr;
		}
		return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource(Entry.IconKey)).Get();
	}

	bool DrawSearchInputWithIcon(const char* Id, const char* Hint, char* Buffer, size_t BufferSize)
	{
		ImGuiStyle& Style = ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 11.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Style.FramePadding.x + 26.0f, Style.FramePadding.y));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, PlaceSearchBg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, PlaceSearchBg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, PlaceSearchBg);
		ImGui::PushStyleColor(ImGuiCol_Border, PlaceBorder);
		const std::string PaddedHint = std::string("   ") + Hint;
		const bool bChanged = ImGui::InputTextWithHint(Id, PaddedHint.c_str(), Buffer, BufferSize);
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(3);

		if (ID3D11ShaderResourceView* SearchIcon = FResourceManager::Get().FindLoadedTexture(
			GetEditorPathResource("Editor.Icon.Search")).Get())
		{
			const ImVec2 Min = ImGui::GetItemRectMin();
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

	const FEntry GPlaceActorEntries[] = {
		{ "Actor", "actor empty basic", "Editor.Icon.Actor", EPlaceType::Actor, ECategory::Basic },
		{ "Pawn", "pawn actor basic", "Editor.Icon.Pawn", EPlaceType::Pawn, ECategory::Basic },
		{ "Runner", "runner player pawn pod basic", "Editor.Icon.Pawn", EPlaceType::Runner, ECategory::Basic },
		{ "Character", "character actor basic", "Editor.Icon.Character", EPlaceType::Character, ECategory::Basic },
		{ "Static Mesh", "static mesh actor mesh basic", "Editor.Icon.StaticMeshActor", EPlaceType::StaticMeshActor, ECategory::Basic },
		{ "Ambient Light", "ambient light", "Editor.Icon.AmbientLight", EPlaceType::AmbientLight, ECategory::Lights },
		{ "Directional Light", "directional light sun", "Editor.Icon.DirectionalLight", EPlaceType::DirectionalLight, ECategory::Lights },
		{ "Point Light", "point light bulb", "Editor.Icon.PointLight", EPlaceType::PointLight, ECategory::Lights },
		{ "Spot Light", "spot light cone", "Editor.Icon.SpotLight", EPlaceType::SpotLight, ECategory::Lights },
		{ "Cube", "cube box shape", "Editor.Icon.Cube", EPlaceType::Cube, ECategory::Shapes },
		{ "Sphere", "sphere ball shape", "Editor.Icon.Sphere", EPlaceType::Sphere, ECategory::Shapes },
		{ "Cylinder", "cylinder shape", "Editor.Icon.Cylinder", EPlaceType::Cylinder, ECategory::Shapes },
		{ "Cone", "cone shape", "Editor.Icon.Cone", EPlaceType::Cone, ECategory::Shapes },
		{ "Plane", "plane quad floor shape", "Editor.Icon.Plane", EPlaceType::Plane, ECategory::Shapes },
		{ "Decal", "decal projection vfx", "Editor.Icon.Decal", EPlaceType::Decal, ECategory::VFX },
		{ "Height Fog", "height fog vfx atmosphere", "Editor.Icon.HeightFog", EPlaceType::HeightFog, ECategory::VFX },
		{ "World Text", "world text 3d billboard font label", "Editor.Icon.ScreenText", EPlaceType::WorldText, ECategory::Text },
		{ "Screen Text", "screen text overlay ui hud widget", "Editor.Icon.ScreenText", EPlaceType::ScreenText, ECategory::Text },
		{ "UI Root", "ui root canvas hud menu widget", "Editor.ToolIcon.WorldSpace", EPlaceType::UIRoot, ECategory::UI },
		{ "Map Manager", "Map Manager", "Editor.Icon.Cube", EPlaceType::MapManager, ECategory::Basic },
	};

	const char* GetCategoryLabel(ECategory Category)
	{
		switch (Category)
		{
		case ECategory::Basic: return "Basic";
		case ECategory::Text: return "Text";
		case ECategory::UI: return "UI";
		case ECategory::Lights: return "Lights";
		case ECategory::Shapes: return "Shapes";
		case ECategory::VFX: return "VFX";
		default: return "Basic";
		}
	}
}

void FEditorPlaceActorsWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(420.0f, 640.0f), ImGuiCond_Once);
	FEditorSettings& Settings = FEditorSettings::Get();
	if (!Settings.UI.bPlaceActors)
	{
		return;
	}

	constexpr const char* PanelIconKey = "Editor.Icon.Panel.PlaceActors";
	const std::string WindowTitle = EditorPanelTitleUtils::MakeClosablePanelTitle("Place Actors", PanelIconKey);
	const bool bIsOpen = ImGui::Begin(WindowTitle.c_str());
	EditorPanelTitleUtils::DrawPanelTitleIcon(PanelIconKey);
	EditorPanelTitleUtils::DrawSmallPanelCloseButton("    Place Actors", Settings.UI.bPlaceActors, "x##ClosePlaceActors");
	if (!bIsOpen)
	{
		ImGui::End();
		return;
	}
	EditorPanelTitleUtils::ApplyPanelContentTopInset();

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 6.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, PlacePanelBg);
	ImGui::PushStyleColor(ImGuiCol_TableBorderLight, IM_COL32(5, 5, 5, 255));
	ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, IM_COL32(5, 5, 5, 255));

	DrawSearchInputWithIcon("##PlaceActorSearch", "Search Classes", SearchBuffer, sizeof(SearchBuffer));
	ImGui::Spacing();

	if (ImGui::BeginTable("##PlaceActorsLayout", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("Categories", ImGuiTableColumnFlags_WidthFixed, 92.0f);
		ImGui::TableSetupColumn("Actors", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextColumn();
		RenderCategorySidebar();

		ImGui::TableNextColumn();
		RenderActorGrid();

		ImGui::EndTable();
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(3);
	ImGui::End();
}

void FEditorPlaceActorsWidget::RenderCategorySidebar()
{

	const EPlaceActorCategory Categories[] = {
		EPlaceActorCategory::Basic,
		EPlaceActorCategory::Lights,
		EPlaceActorCategory::Shapes,
		EPlaceActorCategory::VFX,
		EPlaceActorCategory::Text,
		EPlaceActorCategory::UI
	};

	for (EPlaceActorCategory Category : Categories)
	{
		const bool bSelected = (ActiveCategory == Category);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, PlaceSidebarButton);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PlaceSidebarButtonHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, PlaceSidebarButtonActive);
		if (bSelected)
		{
			ImGui::PopStyleColor(3);
			ImGui::PushStyleColor(ImGuiCol_Button, PlaceCategorySelected);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PlaceCategorySelectedHover);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, PlaceCategorySelectedActive);
		}

		if (ImGui::Button((std::string("##Category") + GetCategoryLabel(Category)).c_str(), ImVec2(-1.0f, 62.0f)))
		{
			ActiveCategory = Category;
		}
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Size = ImGui::GetItemRectSize();
		if (ID3D11ShaderResourceView* Icon = GetPlaceActorsCategoryIcon(Category))
		{
			const float IconSize = 18.0f;
			const float X = Min.x + (Size.x - IconSize) * 0.5f;
			const float Y = Min.y + 10.0f;
			ImGui::GetWindowDrawList()->AddImage(
				reinterpret_cast<ImTextureID>(Icon),
				ImVec2(X, Y),
				ImVec2(X + IconSize, Y + IconSize));
		}
		const char* Label = GetCategoryLabel(Category);
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		ImGui::GetWindowDrawList()->AddText(
			ImVec2(Min.x + (Size.x - TextSize.x) * 0.5f, Min.y + 33.0f),
			ImGui::GetColorU32(ImGuiCol_Text),
			Label);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
	}
}

void FEditorPlaceActorsWidget::RenderActorGrid()
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, PlacePanelBg);
	if (!ImGui::BeginChild("##PlaceActorGrid", ImVec2(0.0f, 0.0f), true))
	{
		ImGui::EndChild();
		ImGui::PopStyleColor();
		return;
	}

	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	const float MinCardWidth = 122.0f;
	const int32 ColumnCount = (std::max)(1, static_cast<int32>(AvailableWidth / MinCardWidth));

	bool bAnyVisible = false;
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 4.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, PlaceEntryBg);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PlaceEntryHover);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, PlaceEntryActive);
	if (ImGui::BeginTable("##PlaceActorButtons", ColumnCount, ImGuiTableFlags_SizingStretchSame))
	{
		for (const FPlaceActorEntry& Entry : GPlaceActorEntries)
		{
			if (Entry.Category != ActiveCategory || !MatchesSearch(Entry))
			{
				continue;
			}

			bAnyVisible = true;
			ImGui::TableNextColumn();
			ImGui::PushID(Entry.Label);

			if (ImGui::Button((std::string("##PlaceActorEntry") + Entry.Label).c_str(), ImVec2(-FLT_MIN, 54.0f)))
			{
				SpawnActor(Entry);
			}
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Size = ImGui::GetItemRectSize();
			float LabelX = Min.x + 32.0f;
			if (ID3D11ShaderResourceView* Icon = GetPlaceActorEntryIcon(Entry))
			{
				const float IconSize = 18.0f;
				const float IconY = Min.y + (Size.y - IconSize) * 0.5f;
				ImGui::GetWindowDrawList()->AddImage(
					reinterpret_cast<ImTextureID>(Icon),
					ImVec2(Min.x + 10.0f, IconY),
					ImVec2(Min.x + 10.0f + IconSize, IconY + IconSize));
			}
			const ImVec2 LabelSize = ImGui::CalcTextSize(Entry.Label);
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(LabelX, Min.y + (Size.y - LabelSize.y) * 0.5f),
				ImGui::GetColorU32(ImGuiCol_Text),
				Entry.Label);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", Entry.Label);
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	if (!bAnyVisible)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("No actors match the current search.");
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

bool FEditorPlaceActorsWidget::MatchesSearch(const FPlaceActorEntry& Entry) const
{
	if (SearchBuffer[0] == '\0')
	{
		return true;
	}

	FString Query = SearchBuffer;
	AsciiUtils::ToLowerInPlace(Query);

	FString SearchKey = Entry.SearchKey;
	AsciiUtils::ToLowerInPlace(SearchKey);

	return SearchKey.find(Query) != FString::npos;
}

void FEditorPlaceActorsWidget::SpawnActor(const FPlaceActorEntry& Entry)
{
	if (!EditorEngine)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	UCameraComponent* Camera = EditorEngine->GetCamera();
	if (!World || !Camera)
	{
		return;
	}

	const FVector RayOrigin = Camera->GetWorldLocation();
	const FVector RayDirection = Camera->GetForwardVector().Normalized();
	FVector SpawnLocation = RayOrigin + RayDirection * 10.0f;

	FRayHitResult HitResult{};
	AActor* HitActor = nullptr;
	if (World->RaycastPrimitives(FRay{ RayOrigin, RayDirection }, HitResult, HitActor))
	{
		SpawnLocation = RayOrigin + RayDirection * HitResult.Distance;
	}

	EditorEngine->SpawnPlaceActor(Entry.Type, SpawnLocation);
}
