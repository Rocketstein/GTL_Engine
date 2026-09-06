#pragma once

#include "Render/Common/RenderTypes.h"
#include "FileManager/UISettingInit.h"
#include "CursorOverlayState.h"
#include <windows.h>
#include <string>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/Common.h"
#include "World/Gizmo/GizmoManager.h"

class UScene;
class UCamera;

using namespace common::structs;

class FEditorViewportClient
{
public:
	void Initialize(HWND InHWindow);
	UCamera* GetCamera() { return Camera; }
	void SetScene(UScene* InScene) { Scene = InScene; }
	void SetCamera(UCamera* InCamera) { Camera = InCamera; }
	void SetViewportSize(float InWidth, float InHeight);

	FGizmoManager& GetGizmoManager() { return GizmoManager; }

	void Tick(float DeltaTime);

	void ResetCamera(UCamera* Camera);
	void SyncCameraFromRenderHandler();
	void ResetViewport();
	void CloseViewport();

	const FCursorOverlayState& GetCursorOverlayState() const { return CursorOverlayState; }
	FViewOutput& GetViewOutput() { return ViewOutput;  }

private:
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void TickCursorOverlay(float DeltaTime);

	void HandleDragStart(const FRay& Ray);

private:
	HWND HWindow = nullptr;
	UScene*   Scene  = nullptr;
	UCamera*  Camera = nullptr;
	FGizmoManager      GizmoManager;

	FVector InitViewPos = FVector(10, 0, 5);
	FVector InitLookAt  = FVector(0, 0, 0);
	float WindowWidth  = 1920.f;
	float WindowHeight = 1080.f;

	bool bIsCursorVisible = true;

	FCursorOverlayState CursorOverlayState;
	FViewOutput         ViewOutput;
};
