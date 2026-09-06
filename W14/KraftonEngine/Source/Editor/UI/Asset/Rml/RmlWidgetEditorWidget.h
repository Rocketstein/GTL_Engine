#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Editor/UI/Asset/Rml/RmlLivePreview.h"

#include "ImGui/imgui.h"

#include <memory>
#include <unordered_map>

class URmlWidgetAsset;

class FRmlWidgetEditorWidget : public FAssetEditorWidget
{
public:
	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Render(float DeltaTime) override;
	void RenderDocument(float DeltaTime) override;

	FString GetDocumentTitle() const override;
	FString GetDocumentPayloadId() const override;
	EEditorDocumentTabKind GetDocumentTabKind() const override;

private:
	struct FElementNode
	{
		FString Id;
		FString ParentId;
		TArray<FString> Children;
	};

	struct FStyleProperty
	{
		FString Name;
		FString Value;
		size_t ValueStart = FString::npos;
		size_t ValueEnd = FString::npos;
	};

	struct FStyleRule
	{
		FString Id;
		size_t BlockStart = FString::npos;
		size_t BlockEnd = FString::npos;
		TArray<FStyleProperty> Properties;
	};

	struct FComputedRect
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Width = 0.0f;
		float Height = 0.0f;
		bool bValid = false;
	};

	void Reload();
	bool SaveStyleSheet();

	void ParseRml();
	void ParseStyleSheet();
	void RebuildElementTree();

	void RenderToolbar(URmlWidgetAsset* Asset);
	void RenderElementList();
	void RenderPreviewCanvas();
	void RenderWireframePreviewCanvas();
	void RenderLivePreviewCanvas();
	void RenderDetailsPanel();
	void RenderStylePropertyFloat(const char* Label, const char* PropertyName);
	void RenderStylePropertyText(const char* Label, const char* PropertyName);

	bool IsElementVisible(const FString& ElementId) const;
	void SetAllElementVisibility(bool bVisible);

	bool SetStyleProperty(const FString& ElementId, const FString& PropertyName, const FString& Value);
	const FStyleRule* FindStyleRule(const FString& ElementId) const;
	FStyleRule* FindStyleRule(const FString& ElementId);
	const FStyleProperty* FindProperty(const FStyleRule& Rule, const FString& PropertyName) const;
	const FStyleProperty* FindCascadedProperty(const FString& ElementId, const FString& PropertyName) const;

	FComputedRect ComputeElementRect(const FString& ElementId);
	FComputedRect ComputeElementRectRecursive(const FString& ElementId, TSet<FString>& Visiting);
	void DrawElementRecursive(const FString& ElementId, const ImVec2& CanvasMin, float Zoom, ImDrawList* DrawList, bool bFilled);
	void HandlePreviewInput(const ImVec2& CanvasMin, float Zoom, bool bHovered);
	FString PickElementAt(const ImVec2& MouseCanvasPosition);

	void ApplyDragDelta(const FString& ElementId, float DeltaX, float DeltaY);

private:
	FString RawRmlText;
	FString RawStyleText;
	FString SelectedElementId;
	FString DraggingElementId;
	FString StatusMessage;
	TArray<FElementNode> Elements;
	TArray<FStyleRule> StyleRules;
	TMap<FString, int32> ElementIndexById;
	TMap<FString, FComputedRect> ComputedRectCache;
	TSet<FString> VisibleElementIds;
	std::unique_ptr<FRmlLivePreview> LivePreview;
	float PreviewWidth = 1280.0f;
	float PreviewHeight = 720.0f;
	float PreviewZoom = 0.75f;
	bool bDragging = false;
	bool bLivePreviewMode = false;
	bool bShowLiveOverlay = true;
};
