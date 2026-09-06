#pragma once
#pragma once

#include "Editor/Viewport/CursorOverlayState.h"

class UScene;
class UCamera;
class UGizmoComponent;
class USceneComponent;

struct FRenderCollectorContext
{
	UScene* Scene = nullptr;
	UCamera* Camera = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FCursorOverlayState* CursorOverlayState = nullptr;

	const std::vector<USceneComponent*>* SelectedComponent;

	float ViewportWidth = 0.f;
	float ViewportHeight = 0.f;

	bool  bGridVisible = true;
	int32 GridSize = 5;
};
