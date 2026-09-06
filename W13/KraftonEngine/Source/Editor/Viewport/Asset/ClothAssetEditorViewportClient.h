#pragma once

#include "Viewport/EditorPreviewViewportClient.h"
#include "Viewport/ViewportClient.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "Editor/Slate/SWindow.h"

#include <d3d11.h>

class AActor;
class FWindowsWindow;
class UClothAsset;
class UClothComponent;
class UWorld;
struct ImDrawList;
struct ImVec2;

class FClothAssetEditorViewportClient : public FViewportClient, public IEditorPreviewViewportClient
{
public:
	void Initialize(ID3D11Device* Device, uint32 Width, uint32 Height);
	void Release();

	void ResetCameraToPreviewBounds();
	void RefreshPreview();

	void SetPreviewWorld(UWorld* InWorld) { PreviewWorld = InWorld; }
	void SetPreviewActor(AActor* InActor) { PreviewActor = InActor; }
	void SetPreviewClothComponent(UClothComponent* InComp) { PreviewClothComponent = InComp; }
	void SetViewportRect(float X, float Y, float Width, float Height) { ViewportScreenRect = { X, Y, Width, Height }; }
	void SetEditedAsset(UClothAsset* InAsset);

	bool IsRenderable() const override { return bIsRenderable; }
	bool IsMouseOverViewport() const override;

	FViewport* GetViewport() const override { return Viewport; }
	UWorld* GetPreviewWorld() const override { return PreviewWorld; }
	UClothComponent* GetPreviewClothComponent() const { return PreviewClothComponent; }
	UClothAsset* GetEditedAsset() const { return EditedAsset; }

	FViewportRenderOptions& GetRenderOptions() override { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const override { return RenderOptions; }

	void NotifyViewportResized(int32 NewWidth, int32 NewHeight) override;
	bool GetCameraView(FMinimalViewInfo& OutPOV) const override;

	void Tick(float DeltaTime);

	void SetVertexSelectionEnabled(bool bEnabled) { bVertexSelectionEnabled = bEnabled; }
	bool IsVertexSelectionEnabled() const { return bVertexSelectionEnabled; }
	const TArray<uint32>& GetSelectedVertexIndices() const { return SelectedVertexIndices; }
	int32 GetSelectedVertexCount() const { return static_cast<int32>(SelectedVertexIndices.size()); }
	void ClearSelection();
	void SelectPinnedVertices();
	bool ConsumeSelectionChanged();

	void DrawVertexOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize) const;

private:
	void TickShortcuts();
	void TickInput(float DeltaTime);
	void TickVertexSelection();
	void FinishVertexSelection();
	void SetSelectedVertices(TArray<uint32> NewSelection);
	bool ProjectVertexToScreen(uint32 VertexIndex, float& OutScreenX, float& OutScreenY) const;
	bool IsVertexPinned(uint32 VertexIndex) const;
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);

private:
	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	FViewportRenderOptions RenderOptions;

	UWorld* PreviewWorld = nullptr;
	AActor* PreviewActor = nullptr;
	UClothComponent* PreviewClothComponent = nullptr;
	UClothAsset* EditedAsset = nullptr;

	bool bIsRenderable = false;

	FViewportCameraTransform ViewTransform;
	FRect ViewportScreenRect;

	bool bVertexSelectionEnabled = true;
	bool bIsSelectingVertices = false;
	bool bSelectionChanged = false;
	FVector SelectionStartScreen = FVector::ZeroVector;
	FVector SelectionCurrentScreen = FVector::ZeroVector;
	TArray<uint32> SelectedVertexIndices;
	TSet<uint32> SelectedVertexSet;

	FVector TargetLocation;
	bool bTargetLocationInitialized = false;
	FVector LastAppliedCameraLocation;
	bool bLastAppliedCameraLocationInitialized = false;
	const float SmoothLocationSpeed = 10.0f;
};
