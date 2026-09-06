#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Editor/Viewport/Asset/ClothAssetEditorViewportClient.h"
#include "Mesh/Importer/MeshImportOptions.h"
#include "Object/FName.h"

class UClothAsset;
class UClothComponent;
struct ImDrawList;
struct ImVec2;

class FClothAssetEditorWidget : public FAssetEditorWidget
{
public:
	FClothAssetEditorWidget();

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

	bool AllowsMultipleInstances() const override { return true; }

	void Render(float DeltaTime) override;

private:
	void RenderViewportPanel(ImVec2 Size);
	void RenderDetailsPanel(UClothAsset* ClothAsset);
	void RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const;
	void RenderImportOptionsPopup(UClothAsset* ClothAsset);

	void BeginImportFromFileDialog();
	void BeginImportFromSourcePath(const FString& SourcePath);
	void ImportPendingSource(UClothAsset* ClothAsset);
	void RenderProjectSourceList();

	void ApplyPinToSelectedVertices(bool bPinned);
	void ClearAllPins();
	void RebuildTethersFromPins(UClothAsset* ClothAsset);
	uint32 CountPinnedVertices(const UClothAsset* ClothAsset) const;

private:
	FClothAssetEditorViewportClient ViewportClient;

	uint32 InstanceId;
	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;

	FString PendingImportSourcePath;
	FImportOptions PendingImportOptions;
	bool bShowImportOptionsPopup = false;
	bool bAutoPinSelection = false;
	bool bVertexSelectionEnabled = true;
	int32 SelectedSourceIndex = -1;
	FString LastImportSourcePath;
	FString LastImportError;
};
