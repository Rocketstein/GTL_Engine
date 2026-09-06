#include "ClothAssetEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/ClothComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/Util/EditorFileUtils.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "Mesh/MeshManager.h"
#include "Physics/Cloth/ClothAsset.h"
#include "Physics/Cloth/ClothAssetBuilder.h"
#include "Physics/Cloth/ClothAssetManager.h"
#include "Platform/Paths.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cstdio>
#include <imgui.h>

namespace
{
	static uint32 GNextClothAssetEditorInstanceId = 0;

	FString FormatClothStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}
}

FClothAssetEditorWidget::FClothAssetEditorWidget()
	: InstanceId(GNextClothAssetEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("ClothAssetEditorPreview_" + Id);
	WindowIdSuffix = "###ClothAssetEditor_" + Id;
	PendingImportOptions = FImportOptions::Default();
}

bool FClothAssetEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UClothAsset>();
}

bool FClothAssetEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const UClothAsset* CurrentAsset = Cast<UClothAsset>(EditedObject);
	const UClothAsset* RequestedAsset = Cast<UClothAsset>(Object);
	if (!IsOpen() || !CurrentAsset || !RequestedAsset)
	{
		return false;
	}

	const FString& CurrentPath = CurrentAsset->GetSourcePath();
	return !CurrentPath.empty()
		&& CurrentPath != "None"
		&& CurrentPath == RequestedAsset->GetSourcePath();
}

void FClothAssetEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	UClothAsset* ClothAsset = Cast<UClothAsset>(EditedObject);
	if (FClothAssetManager::Get().UpgradeLegacyDefaultQuadTo32x32(ClothAsset))
	{
		MarkDirty();
	}

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	UClothComponent* ClothComponent = Actor->AddComponent<UClothComponent>();
	ClothComponent->SetClothAsset(ClothAsset);
	//ClothComponent->SetSimulationEnabled(false);
	Actor->SetRootComponent(ClothComponent);
	Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	if (UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>())
	{
		LightComp->SetShadowBias(0.0f);
		LightComp->PushToScene();
	}

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
	if (ViewportSize.x <= 0.0f || ViewportSize.y <= 0.0f)
	{
		ViewportSize = ImVec2(800.0f, 600.0f);
	}

	ViewportClient.Initialize(
		GEngine->GetRenderer().GetFD3DDevice().GetDevice(),
		static_cast<uint32>(ViewportSize.x),
		static_cast<uint32>(ViewportSize.y));
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewClothComponent(ClothComponent);
	ViewportClient.SetEditedAsset(ClothAsset);
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	FSlateApplication::Get().RegisterViewport(&ViewportClient);
	FMeshManager::ScanMeshSourceFiles();
}

void FClothAssetEditorWidget::Close()
{
	FAssetEditorWidget::Close();

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
}

void FClothAssetEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
}

void FClothAssetEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FClothAssetEditorViewportClient*>(&ViewportClient));
	}
}

void FClothAssetEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	static float DetailsWidth = 330.0f;
	UClothAsset* ClothAsset = Cast<UClothAsset>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Cloth Asset Editor";
	const FString AssetPath = ClothAsset ? ClothAsset->GetSourcePath() : FString();
	if (!AssetPath.empty() && AssetPath != "None")
	{
		VisibleTitle += " - ";
		VisibleTitle += AssetPath;
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	ImGui::BeginGroup();
	{
		const float AvailableWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
		const ImVec2 Size(AvailableWidth, ImGui::GetContentRegionAvail().y);
		RenderViewportPanel(Size);
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("Details", ImVec2(DetailsWidth, 0), true);
	RenderDetailsPanel(ClothAsset);
	ImGui::EndChild();

	RenderImportOptionsPopup(ClothAsset);

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FClothAssetEditorWidget::RenderViewportPanel(ImVec2 Size)
{
	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

	FViewport* VP = ViewportClient.GetViewport();
	if (!VP || Size.x <= 0.0f || Size.y <= 0.0f)
	{
		return;
	}

	VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));

	if (VP->GetSRV())
	{
		ImGui::Image((ImTextureID)VP->GetSRV(), Size);
		FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
	}

	constexpr float ToolbarHeight = 28.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(
		ViewportPos,
		ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight),
		IM_COL32(40, 40, 40, 255));

	FViewportToolbarContext Context;
	Context.Renderer = &GEngine->GetRenderer();
	Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
	Context.RenderOptions = &ViewportClient.GetRenderOptions();
	Context.ToolbarLeft = ViewportPos.x;
	Context.ToolbarTop = ViewportPos.y;
	Context.ToolbarWidth = Size.x;
	Context.bReservePlayStopSpace = false;
	Context.bShowAddActor = false;
	Context.bShowGizmoControls = false;

	FViewportToolbar::Render(Context);
	RenderMeshStatsOverlay(DrawList, ViewportPos);
	ViewportClient.DrawVertexOverlay(DrawList, ViewportPos, Size);
}

void FClothAssetEditorWidget::RenderDetailsPanel(UClothAsset* ClothAsset)
{
	ImGui::TextUnformatted("Cloth Asset Details");
	ImGui::Separator();

	const FString SaveLabel = IsDirty() ? "Save *" : "Save";
	if (!ClothAsset)
	{
		ImGui::TextDisabled("No ClothAsset.");
		return;
	}

	if (ImGui::Button(SaveLabel.c_str()))
	{
		RebuildTethersFromPins(ClothAsset);
		if (FClothAssetManager::Get().Save(ClothAsset))
		{
			ClearDirty();
			if (EditorEngine)
			{
				EditorEngine->RefreshContentBrowser();
			}
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Focus"))
	{
		ViewportClient.ResetCameraToPreviewBounds();
	}

	ImGui::Separator();
	ImGui::Text("Particles: %s", FormatClothStatCount(ClothAsset->GetParticleCount()).c_str());
	ImGui::Text("Triangles: %s", FormatClothStatCount(ClothAsset->GetIndexCount() / 3).c_str());
	ImGui::Text("Pinned: %s", FormatClothStatCount(CountPinnedVertices(ClothAsset)).c_str());
	ImGui::Text("Tethers: %s", FormatClothStatCount(ClothAsset->GetFabricData().TetherLengths.size()).c_str());
	ImGui::Text("Selected: %s", FormatClothStatCount(ViewportClient.GetSelectedVertexCount()).c_str());

	ImGui::Separator();
	ImGui::TextUnformatted("Import");
	if (ImGui::Button("Import OBJ/FBX...", ImVec2(-1.0f, 0.0f)))
	{
		BeginImportFromFileDialog();
	}

	RenderProjectSourceList();

	if (!LastImportSourcePath.empty())
	{
		ImGui::TextWrapped("Last import: %s", LastImportSourcePath.c_str());
	}
	if (!LastImportError.empty())
	{
		ImGui::TextWrapped("Import error: %s", LastImportError.c_str());
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Pin Points");
	if (ImGui::Checkbox("Vertex Selection", &bVertexSelectionEnabled))
	{
		ViewportClient.SetVertexSelectionEnabled(bVertexSelectionEnabled);
	}
	ViewportClient.SetVertexSelectionEnabled(bVertexSelectionEnabled);
	ImGui::Checkbox("Auto Pin Drag Selection", &bAutoPinSelection);

	if (ViewportClient.ConsumeSelectionChanged() && bAutoPinSelection)
	{
		ApplyPinToSelectedVertices(true);
	}

	const bool bHasSelection = ViewportClient.GetSelectedVertexCount() > 0;
	if (!bHasSelection) ImGui::BeginDisabled();
	if (ImGui::Button("Pin Selected", ImVec2(-1.0f, 0.0f)))
	{
		ApplyPinToSelectedVertices(true);
	}
	if (ImGui::Button("Unpin Selected", ImVec2(-1.0f, 0.0f)))
	{
		ApplyPinToSelectedVertices(false);
	}
	if (!bHasSelection) ImGui::EndDisabled();

	if (ImGui::Button("Select Pinned", ImVec2(-1.0f, 0.0f)))
	{
		ViewportClient.SelectPinnedVertices();
	}
	if (ImGui::Button("Clear Selection", ImVec2(-1.0f, 0.0f)))
	{
		ViewportClient.ClearSelection();
	}
	if (ImGui::Button("Clear All Pins", ImVec2(-1.0f, 0.0f)))
	{
		ClearAllPins();
	}
}

void FClothAssetEditorWidget::RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const
{
	if (!DrawList || !EditedObject)
	{
		return;
	}

	size_t ParticleCount = 0;
	size_t TriangleCount = 0;
	size_t PinnedCount = 0;
	if (const UClothAsset* ClothAsset = Cast<UClothAsset>(EditedObject))
	{
		ParticleCount = ClothAsset->GetParticleCount();
		TriangleCount = ClothAsset->GetIndexCount() / 3;
		PinnedCount = CountPinnedVertices(ClothAsset);
	}

	const FString Text =
		"Triangles: " + FormatClothStatCount(TriangleCount) + "\n" +
		"Particles: " + FormatClothStatCount(ParticleCount) + "\n" +
		"Pinned: " + FormatClothStatCount(PinnedCount);

	const ImVec2 TextPos(ViewportPos.x + 8.0f, ViewportPos.y + 36.0f);
	DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Text.c_str());
	DrawList->AddText(TextPos, IM_COL32(235, 238, 242, 255), Text.c_str());
}

void FClothAssetEditorWidget::RenderImportOptionsPopup(UClothAsset* ClothAsset)
{
	if (!bShowImportOptionsPopup)
	{
		return;
	}

	ImGui::OpenPopup("Cloth Import Options");
	const ImVec2 Center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(Center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Cloth Import Options", &bShowImportOptionsPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("Source: %s", PendingImportSourcePath.c_str());

		ImGui::InputFloat("Scale", &PendingImportOptions.Scale, 0.01f, 1.0f, "%.4f");
		ImGui::SameLine();
		if (ImGui::SmallButton("cm->m"))
		{
			PendingImportOptions.Scale = 0.01f;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("1:1"))
		{
			PendingImportOptions.Scale = 1.0f;
		}

		const char* AxisLabels[] = { "X", "-X", "Y", "-Y", "Z", "-Z" };
		int AxisIndex = static_cast<int>(PendingImportOptions.ForwardAxis);
		if (ImGui::Combo("Forward Axis", &AxisIndex, AxisLabels, IM_ARRAYSIZE(AxisLabels)))
		{
			PendingImportOptions.ForwardAxis = static_cast<EForwardAxis>(AxisIndex);
		}

		const char* WindingLabels[] = { "CCW -> CW (DirectX)", "Keep Original" };
		int WindingIndex = static_cast<int>(PendingImportOptions.WindingOrder);
		if (ImGui::Combo("Winding Order", &WindingIndex, WindingLabels, IM_ARRAYSIZE(WindingLabels)))
		{
			PendingImportOptions.WindingOrder = static_cast<EWindingOrder>(WindingIndex);
		}

		const char* SkinnedFbxLabels[] = { "Skip Skinned Mesh", "Bind Pose As Static" };
		int SkinnedPolicyIndex = static_cast<int>(PendingImportOptions.StaticFbxSkinnedMeshPolicy);
		if (ImGui::Combo("Skinned FBX", &SkinnedPolicyIndex, SkinnedFbxLabels, IM_ARRAYSIZE(SkinnedFbxLabels)))
		{
			PendingImportOptions.StaticFbxSkinnedMeshPolicy = static_cast<EStaticFbxSkinnedMeshPolicy>(SkinnedPolicyIndex);
		}

		ImGui::Separator();
		if (ImGui::Button("Import", ImVec2(120.0f, 0.0f)))
		{
			ImportPendingSource(ClothAsset);
			bShowImportOptionsPopup = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
		{
			bShowImportOptionsPopup = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void FClothAssetEditorWidget::BeginImportFromFileDialog()
{
	const std::wstring InitialDirectory = FPaths::RootDir();
	FEditorFileDialogOptions Options;
	Options.Filter = L"Mesh Files (*.obj;*.fbx)\0*.obj;*.fbx\0OBJ Files (*.obj)\0*.obj\0FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
	Options.Title = L"Import Cloth Mesh";
	Options.InitialDirectory = InitialDirectory.c_str();
	Options.bReturnRelativeToProjectRoot = true;

	const FString SourcePath = FEditorFileUtils::OpenFileDialog(Options);
	if (!SourcePath.empty())
	{
		BeginImportFromSourcePath(SourcePath);
	}
}

void FClothAssetEditorWidget::BeginImportFromSourcePath(const FString& SourcePath)
{
	PendingImportSourcePath = FPaths::MakeProjectRelative(SourcePath);
	PendingImportOptions = FImportOptions::Default();
	bShowImportOptionsPopup = true;
	LastImportError.clear();
}

void FClothAssetEditorWidget::ImportPendingSource(UClothAsset* ClothAsset)
{
	if (!ClothAsset)
	{
		LastImportError = "No ClothAsset is open.";
		return;
	}
	if (PendingImportSourcePath.empty())
	{
		LastImportError = "No source file selected.";
		return;
	}

	FClothAssetBuildOptions BuildOptions;
	BuildOptions.bBuildDefaultPinnedGrid32x32 = false;

	FString Error;
	if (FClothAssetManager::Get().ReplaceFromMeshSourceFile(ClothAsset, PendingImportSourcePath, PendingImportOptions, &Error, BuildOptions))
	{
		LastImportSourcePath = PendingImportSourcePath;
		LastImportError.clear();
		ViewportClient.SetEditedAsset(ClothAsset);
		ViewportClient.RefreshPreview();
		ViewportClient.ResetCameraToPreviewBounds();
		ClearDirty();
		if (EditorEngine)
		{
			EditorEngine->RefreshContentBrowser();
		}
	}
	else
	{
		LastImportError = Error.empty() ? FString("Unknown ClothAsset import error.") : Error;
	}
}

void FClothAssetEditorWidget::RenderProjectSourceList()
{
	const TArray<FAssetListItem>& Sources = FMeshManager::GetAvailableObjFiles();
	if (SelectedSourceIndex >= static_cast<int32>(Sources.size()))
	{
		SelectedSourceIndex = -1;
	}

	if (ImGui::Button("Refresh Sources", ImVec2(-1.0f, 0.0f)))
	{
		FMeshManager::ScanMeshSourceFiles();
	}

	ImGui::BeginChild("ClothMeshSources", ImVec2(0.0f, 130.0f), true);
	for (int32 Index = 0; Index < static_cast<int32>(Sources.size()); ++Index)
	{
		const bool bSelected = Index == SelectedSourceIndex;
		FString Label = Sources[Index].DisplayName + "##ClothSource_" + std::to_string(Index);
		if (ImGui::Selectable(Label.c_str(), bSelected))
		{
			SelectedSourceIndex = Index;
		}
	}
	ImGui::EndChild();

	const bool bHasSource = SelectedSourceIndex >= 0 && SelectedSourceIndex < static_cast<int32>(Sources.size());
	if (!bHasSource) ImGui::BeginDisabled();
	if (ImGui::Button("Import Selected Source", ImVec2(-1.0f, 0.0f)) && bHasSource)
	{
		BeginImportFromSourcePath(Sources[SelectedSourceIndex].FullPath);
	}
	if (!bHasSource) ImGui::EndDisabled();
}

void FClothAssetEditorWidget::ApplyPinToSelectedVertices(bool bPinned)
{
	UClothAsset* ClothAsset = Cast<UClothAsset>(EditedObject);
	if (!ClothAsset)
	{
		return;
	}

	const uint32 ParticleCount = ClothAsset->GetParticleCount();
	if (ClothAsset->PinMask.size() != ParticleCount)
	{
		ClothAsset->PinMask.assign(ParticleCount, 0.0f);
	}
	if (ClothAsset->InvMasses.size() != ParticleCount)
	{
		ClothAsset->InvMasses.assign(ParticleCount, 1.0f);
	}

	for (uint32 VertexIndex : ViewportClient.GetSelectedVertexIndices())
	{
		if (VertexIndex >= ParticleCount)
		{
			continue;
		}

		ClothAsset->PinMask[VertexIndex] = bPinned ? 1.0f : 0.0f;
		ClothAsset->InvMasses[VertexIndex] = bPinned ? 0.0f : 1.0f;
	}

	RebuildTethersFromPins(ClothAsset);
	ViewportClient.RefreshPreview();
	MarkDirty();
}

void FClothAssetEditorWidget::ClearAllPins()
{
	UClothAsset* ClothAsset = Cast<UClothAsset>(EditedObject);
	if (!ClothAsset)
	{
		return;
	}

	const uint32 ParticleCount = ClothAsset->GetParticleCount();
	ClothAsset->PinMask.assign(ParticleCount, 0.0f);
	ClothAsset->InvMasses.assign(ParticleCount, 1.0f);
	RebuildTethersFromPins(ClothAsset);
	ViewportClient.RefreshPreview();
	MarkDirty();
}

void FClothAssetEditorWidget::RebuildTethersFromPins(UClothAsset* ClothAsset)
{
	if (!ClothAsset)
	{
		return;
	}

	FClothAssetBuilder::RebuildTethersFromPins(*ClothAsset);
}

uint32 FClothAssetEditorWidget::CountPinnedVertices(const UClothAsset* ClothAsset) const
{
	if (!ClothAsset)
	{
		return 0;
	}

	const TArray<float>& PinMask = ClothAsset->GetPinMask();
	const TArray<float>& InvMasses = ClothAsset->GetInvMasses();
	const uint32 ParticleCount = ClothAsset->GetParticleCount();
	uint32 Count = 0;
	for (uint32 Index = 0; Index < ParticleCount; ++Index)
	{
		const bool bPinnedByMask = Index < PinMask.size() && PinMask[Index] > 0.0f;
		const bool bPinnedByMass = Index < InvMasses.size() && InvMasses[Index] <= 0.0f;
		if (bPinnedByMask || bPinnedByMass)
		{
			++Count;
		}
	}
	return Count;
}
