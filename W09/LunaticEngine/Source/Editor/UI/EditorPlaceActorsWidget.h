#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Editor/Viewport/FLevelViewportLayout.h"

class FEditorPlaceActorsWidget : public FEditorWidget
{
public:
	enum class EPlaceActorCategory : uint8
	{
		Basic,
		Text,
		UI,
		Lights,
		Shapes,
		VFX
	};

	struct FPlaceActorEntry
	{
		const char* Label = "";
		const char* SearchKey = "";
		const char* IconKey = "";
		FLevelViewportLayout::EViewportPlaceActorType Type = FLevelViewportLayout::EViewportPlaceActorType::Cube;
		EPlaceActorCategory Category = EPlaceActorCategory::Basic;
	};

	void Render(float DeltaTime) override;

private:

	void RenderCategorySidebar();
	void RenderActorGrid();
	bool MatchesSearch(const FPlaceActorEntry& Entry) const;
	void SpawnActor(const FPlaceActorEntry& Entry);

	EPlaceActorCategory ActiveCategory = EPlaceActorCategory::Basic;
	char SearchBuffer[128] = {};
};
