#include "ClothAssetEditorViewportClient.h"

#include "Component/Primitive/ClothComponent.h"
#include "Input/InputSystem.h"
#include "Math/MathUtils.h"
#include "Physics/Cloth/ClothAsset.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

void FClothAssetEditorViewportClient::Initialize(ID3D11Device* Device, uint32 Width, uint32 Height)
{
	Viewport = new FViewport();
	Viewport->Initialize(Device, Width, Height);
	Viewport->SetClient(this);

	bIsRenderable = true;
}

void FClothAssetEditorViewportClient::Release()
{
	if (Viewport)
	{
		Viewport->Release();
		delete Viewport;
		Viewport = nullptr;
	}

	PreviewWorld = nullptr;
	PreviewActor = nullptr;
	PreviewClothComponent = nullptr;
	EditedAsset = nullptr;
	SelectedVertexIndices.clear();
	SelectedVertexSet.clear();
	bIsRenderable = false;
}

void FClothAssetEditorViewportClient::SetEditedAsset(UClothAsset* InAsset)
{
	EditedAsset = InAsset;
	ClearSelection();
}

void FClothAssetEditorViewportClient::RefreshPreview()
{
	if (PreviewClothComponent)
	{
		PreviewClothComponent->ResetSimulation();
	}
}

void FClothAssetEditorViewportClient::ResetCameraToPreviewBounds()
{
	FBoundingBox Bounds = PreviewClothComponent
		? PreviewClothComponent->GetWorldBoundingBox()
		: FBoundingBox(FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f));

	FVector Center = Bounds.GetCenter();
	float Radius = Bounds.GetExtent().Length();
	if (Radius < 0.1f)
	{
		Radius = 1.0f;
	}

	const float FovRadians = ViewTransform.FOV;
	const float Distance = Radius / std::tan(FovRadians * 0.5f) * 1.35f;
	const FVector ViewDir = FVector(-1.0f, -1.0f, -0.6f).Normalized();

	ViewTransform.ViewLocation = Center - ViewDir * Distance;
	ViewTransform.LookAt(Center);

	TargetLocation = ViewTransform.ViewLocation;
	LastAppliedCameraLocation = ViewTransform.ViewLocation;
	bTargetLocationInitialized = true;
	bLastAppliedCameraLocationInitialized = true;
}

bool FClothAssetEditorViewportClient::IsMouseOverViewport() const
{
	if (!bIsRenderable || ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
	{
		return false;
	}

	ImVec2 MousePos = ImGui::GetMousePos();
	return MousePos.x >= ViewportScreenRect.X && MousePos.x <= (ViewportScreenRect.X + ViewportScreenRect.Width) &&
		MousePos.y >= ViewportScreenRect.Y && MousePos.y <= (ViewportScreenRect.Y + ViewportScreenRect.Height);
}

void FClothAssetEditorViewportClient::NotifyViewportResized(int32 NewWidth, int32 NewHeight)
{
	if (Viewport && NewHeight > 0)
	{
		ViewTransform.AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
	}
}

bool FClothAssetEditorViewportClient::GetCameraView(FMinimalViewInfo& OutPOV) const
{
	OutPOV.Location = ViewTransform.ViewLocation;
	OutPOV.Rotation = ViewTransform.ViewRotation;
	OutPOV.FOV = ViewTransform.FOV;
	OutPOV.AspectRatio = ViewTransform.AspectRatio;
	OutPOV.OrthoWidth = ViewTransform.OrthoZoom;
	OutPOV.NearClip = ViewTransform.NearClip;
	OutPOV.FarClip = ViewTransform.FarClip;
	OutPOV.bIsOrtho = ViewTransform.bIsOrtho;
	return true;
}

void FClothAssetEditorViewportClient::Tick(float DeltaTime)
{
	SyncCameraSmoothingTarget();
	ApplySmoothedCameraLocation(DeltaTime);
	TickShortcuts();
	TickInput(DeltaTime);
	TickVertexSelection();
}

void FClothAssetEditorViewportClient::TickShortcuts()
{
	if (!FSlateApplication::Get().DoesClientOwnKeyboardInput(this))
	{
		return;
	}

	if (InputSystem::Get().GetKeyDown('F'))
	{
		ResetCameraToPreviewBounds();
	}
}

void FClothAssetEditorViewportClient::TickInput(float DeltaTime)
{
	if (!FSlateApplication::Get().DoesClientOwnMouseInput(this))
	{
		return;
	}
	if (ImGui::GetIO().WantTextInput)
	{
		return;
	}

	FViewportCameraControlSettings& ControlSettings = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls;
	InputSystem& Input = InputSystem::Get();

	FVector LocalMove = FVector::ZeroVector;
	float WorldVerticalMove = 0.0f;
	const float CameraSpeed = ControlSettings.MoveSpeed;

	if (Input.GetKey('W')) LocalMove.X += CameraSpeed;
	if (Input.GetKey('S')) LocalMove.X -= CameraSpeed;
	if (Input.GetKey('D')) LocalMove.Y += CameraSpeed;
	if (Input.GetKey('A')) LocalMove.Y -= CameraSpeed;
	if (Input.GetKey('Q')) WorldVerticalMove -= CameraSpeed;
	if (Input.GetKey('E')) WorldVerticalMove += CameraSpeed;

	const FVector Forward = ViewTransform.ViewRotation.GetForwardVector();
	const FVector Right = ViewTransform.ViewRotation.GetRightVector();

	FVector DeltaMove = (Forward * LocalMove.X + Right * LocalMove.Y) * DeltaTime;
	DeltaMove.Z += WorldVerticalMove * DeltaTime;
	TargetLocation += DeltaMove;

	if (Input.GetKey(VK_RBUTTON))
	{
		const float MouseRotationSpeed = 0.15f * ControlSettings.RotationSpeed;
		const float DeltaYaw = static_cast<float>(Input.MouseDeltaX()) * MouseRotationSpeed;
		const float DeltaPitch = static_cast<float>(Input.MouseDeltaY()) * MouseRotationSpeed;
		ViewTransform.Rotate(DeltaYaw, DeltaPitch);
	}

	const float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (ScrollNotches != 0.0f)
	{
		if (InputSystem::Get().GetKey(VK_RBUTTON))
		{
			float& MoveSpeed = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls.MoveSpeed;
			MoveSpeed = ScrollNotches < 0.0f ? MoveSpeed * 0.9f : MoveSpeed * 1.1f;
			MoveSpeed = Clamp(MoveSpeed, 0.001f, 1000.0f);
		}
		else if (ViewTransform.bIsOrtho)
		{
			const float NewWidth = ViewTransform.OrthoZoom - ScrollNotches * ControlSettings.ZoomSpeed * DeltaTime;
			ViewTransform.OrthoZoom = Clamp(NewWidth, 0.1f, 1000.0f);
		}
		else
		{
			TargetLocation += ViewTransform.ViewRotation.GetForwardVector() * (ScrollNotches * ControlSettings.ZoomSpeed * 0.015f);
		}
	}
}

void FClothAssetEditorViewportClient::TickVertexSelection()
{
	if (!bVertexSelectionEnabled || !FSlateApplication::Get().DoesClientOwnMouseInput(this))
	{
		return;
	}
	if (ImGui::GetIO().WantTextInput)
	{
		return;
	}

	InputSystem& Input = InputSystem::Get();
	const ImVec2 MousePos = ImGui::GetIO().MousePos;

	if (Input.GetKeyDown(VK_LBUTTON))
	{
		bIsSelectingVertices = true;
		SelectionStartScreen = FVector(MousePos.x, MousePos.y, 0.0f);
		SelectionCurrentScreen = SelectionStartScreen;
	}
	else if (bIsSelectingVertices && Input.GetKey(VK_LBUTTON))
	{
		SelectionCurrentScreen = FVector(MousePos.x, MousePos.y, 0.0f);
	}

	if (bIsSelectingVertices && (Input.GetLeftDragEnd() || Input.GetKeyUp(VK_LBUTTON)))
	{
		SelectionCurrentScreen = FVector(MousePos.x, MousePos.y, 0.0f);
		FinishVertexSelection();
		bIsSelectingVertices = false;
	}
}

void FClothAssetEditorViewportClient::FinishVertexSelection()
{
	if (!EditedAsset)
	{
		SetSelectedVertices({});
		return;
	}

	const float MinX = (std::min)(SelectionStartScreen.X, SelectionCurrentScreen.X);
	const float MaxX = (std::max)(SelectionStartScreen.X, SelectionCurrentScreen.X);
	const float MinY = (std::min)(SelectionStartScreen.Y, SelectionCurrentScreen.Y);
	const float MaxY = (std::max)(SelectionStartScreen.Y, SelectionCurrentScreen.Y);
	const float Width = MaxX - MinX;
	const float Height = MaxY - MinY;

	TArray<uint32> NewSelection;
	const uint32 ParticleCount = EditedAsset->GetParticleCount();

	if (Width <= 4.0f && Height <= 4.0f)
	{
		uint32 BestIndex = 0;
		float BestDistSq = 9.0f * 9.0f;
		bool bFound = false;
		for (uint32 Index = 0; Index < ParticleCount; ++Index)
		{
			float ScreenX = 0.0f;
			float ScreenY = 0.0f;
			if (!ProjectVertexToScreen(Index, ScreenX, ScreenY))
			{
				continue;
			}

			const float DX = ScreenX - SelectionCurrentScreen.X;
			const float DY = ScreenY - SelectionCurrentScreen.Y;
			const float DistSq = DX * DX + DY * DY;
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestIndex = Index;
				bFound = true;
			}
		}

		if (bFound)
		{
			NewSelection.push_back(BestIndex);
		}
		SetSelectedVertices(std::move(NewSelection));
		return;
	}

	NewSelection.reserve(64);
	for (uint32 Index = 0; Index < ParticleCount; ++Index)
	{
		float ScreenX = 0.0f;
		float ScreenY = 0.0f;
		if (!ProjectVertexToScreen(Index, ScreenX, ScreenY))
		{
			continue;
		}

		if (ScreenX >= MinX && ScreenX <= MaxX && ScreenY >= MinY && ScreenY <= MaxY)
		{
			NewSelection.push_back(Index);
		}
	}

	SetSelectedVertices(std::move(NewSelection));
}

void FClothAssetEditorViewportClient::SetSelectedVertices(TArray<uint32> NewSelection)
{
	std::sort(NewSelection.begin(), NewSelection.end());
	NewSelection.erase(std::unique(NewSelection.begin(), NewSelection.end()), NewSelection.end());

	if (NewSelection == SelectedVertexIndices)
	{
		return;
	}

	SelectedVertexIndices = std::move(NewSelection);
	SelectedVertexSet.clear();
	for (uint32 Index : SelectedVertexIndices)
	{
		SelectedVertexSet.insert(Index);
	}
	bSelectionChanged = true;
}

void FClothAssetEditorViewportClient::ClearSelection()
{
	SetSelectedVertices({});
}

void FClothAssetEditorViewportClient::SelectPinnedVertices()
{
	if (!EditedAsset)
	{
		SetSelectedVertices({});
		return;
	}

	TArray<uint32> Pinned;
	for (uint32 Index = 0; Index < EditedAsset->GetParticleCount(); ++Index)
	{
		if (IsVertexPinned(Index))
		{
			Pinned.push_back(Index);
		}
	}
	SetSelectedVertices(std::move(Pinned));
}

bool FClothAssetEditorViewportClient::ConsumeSelectionChanged()
{
	const bool bChanged = bSelectionChanged;
	bSelectionChanged = false;
	return bChanged;
}

bool FClothAssetEditorViewportClient::ProjectVertexToScreen(uint32 VertexIndex, float& OutScreenX, float& OutScreenY) const
{
	if (!EditedAsset || VertexIndex >= EditedAsset->GetRestPositions().size() || ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
	{
		return false;
	}

	const FVector LocalPosition = EditedAsset->GetRestPositions()[VertexIndex];
	const FVector WorldPosition = PreviewClothComponent
		? PreviewClothComponent->GetWorldMatrix().TransformPositionWithW(LocalPosition)
		: LocalPosition;

	FMinimalViewInfo POV;
	GetCameraView(POV);
	const FMatrix ViewProjection = POV.CalculateViewProjectionMatrix();
	const FVector ClipSpace = ViewProjection.TransformPositionWithW(WorldPosition);
	if (!std::isfinite(ClipSpace.X) || !std::isfinite(ClipSpace.Y))
	{
		return false;
	}

	const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : ViewportScreenRect.Width;
	const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : ViewportScreenRect.Height;
	OutScreenX = (ClipSpace.X * 0.5f + 0.5f) * VPWidth + ViewportScreenRect.X;
	OutScreenY = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * VPHeight + ViewportScreenRect.Y;
	return true;
}

bool FClothAssetEditorViewportClient::IsVertexPinned(uint32 VertexIndex) const
{
	if (!EditedAsset)
	{
		return false;
	}

	const TArray<float>& PinMask = EditedAsset->GetPinMask();
	const TArray<float>& InvMasses = EditedAsset->GetInvMasses();
	const bool bPinnedByMask = VertexIndex < PinMask.size() && PinMask[VertexIndex] > 0.0f;
	const bool bPinnedByMass = VertexIndex < InvMasses.size() && InvMasses[VertexIndex] <= 0.0f;
	return bPinnedByMask || bPinnedByMass;
}

void FClothAssetEditorViewportClient::DrawVertexOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize) const
{
	if (!DrawList || !EditedAsset || ViewportSize.x <= 0.0f || ViewportSize.y <= 0.0f)
	{
		return;
	}

	const uint32 ParticleCount = EditedAsset->GetParticleCount();
	const uint32 MaxOverlayVertices = (std::min<uint32>)(ParticleCount, 50000);
	for (uint32 Index = 0; Index < MaxOverlayVertices; ++Index)
	{
		float ScreenX = 0.0f;
		float ScreenY = 0.0f;
		if (!ProjectVertexToScreen(Index, ScreenX, ScreenY))
		{
			continue;
		}

		if (ScreenX < ViewportPos.x || ScreenX > ViewportPos.x + ViewportSize.x ||
			ScreenY < ViewportPos.y || ScreenY > ViewportPos.y + ViewportSize.y)
		{
			continue;
		}

		const bool bSelected = SelectedVertexSet.find(Index) != SelectedVertexSet.end();
		const bool bPinned = IsVertexPinned(Index);
		const ImU32 Color = bSelected
			? IM_COL32(80, 220, 255, 245)
			: (bPinned ? IM_COL32(255, 95, 70, 235) : IM_COL32(240, 245, 250, 145));
		const float Radius = bSelected ? 4.0f : (bPinned ? 3.0f : 2.0f);
		DrawList->AddCircleFilled(ImVec2(ScreenX, ScreenY), Radius, Color, 10);
	}

	if (bIsSelectingVertices)
	{
		const ImVec2 RectMin(
			(std::min)(SelectionStartScreen.X, SelectionCurrentScreen.X),
			(std::min)(SelectionStartScreen.Y, SelectionCurrentScreen.Y));
		const ImVec2 RectMax(
			(std::max)(SelectionStartScreen.X, SelectionCurrentScreen.X),
			(std::max)(SelectionStartScreen.Y, SelectionCurrentScreen.Y));

		DrawList->AddRectFilled(RectMin, RectMax, IM_COL32(80, 180, 255, 35));
		DrawList->AddRect(RectMin, RectMax, IM_COL32(80, 220, 255, 230), 0.0f, 0, 1.5f);
	}
}

void FClothAssetEditorViewportClient::SyncCameraSmoothingTarget()
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const bool bCameraMovedExternally = bLastAppliedCameraLocationInitialized
		&& FVector::DistSquared(CurrentLocation, LastAppliedCameraLocation) > 0.0001f;

	if (!bTargetLocationInitialized || bCameraMovedExternally)
	{
		TargetLocation = CurrentLocation;
		bTargetLocationInitialized = true;
	}

	LastAppliedCameraLocation = CurrentLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FClothAssetEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const float LerpAlpha = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
	const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
	ViewTransform.ViewLocation = NewLocation;

	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
}
