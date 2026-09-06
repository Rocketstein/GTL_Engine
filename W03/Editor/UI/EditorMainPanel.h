#pragma once

#include <windows.h>
#include "FileManager/UISettingInit.h"
#include "Math/Vector.h"
#include "Editor/Core/EditorConsole.h"
#include "Core/Common.h"
#include "Object/Object.h"


class FRenderer;
class FEditorEngine;
class FEditorViewportClient;
class AActor;
class USubUVComponent;
class UTextComponent;

enum class EPrimitiveType;

using namespace common::structs;


class FEditorMainPanel
{
private:
	const char* PrimitiveTypes[5] =
	{
		"Cube",
		"Sphere",
		"Plane",
		"SubUV",
		"Text",
	};
	const char* UtilTypes[1] =
	{
		"Spotlight"
	};
	const char* TextAlignmentTypes[3] =
	{
		"Left",
		"Center",
		"Right",
	};

	FEditorEngine* EditorEngine = nullptr;
	FRenderer* Renderer;
	FEditorViewportClient* Viewport;

	int SelectedUtilityActorType = 0;
	int SelectedPrimitiveType = 0;
	int NumberOfSpawnedActors = 1;
	FVector CurSpawnPoint = { 0.f,0.f,0.f };
	bool bShowConsole = true;

	// Scene stuffs
	float NewSceneNotificationTimer = 0;
	float SceneSaveNotificationTimer = 0;
	float SceneLoadNotificationTimer = 0;
	uint32 SelectedSceneIndex = -1;

	// Scene Manager stuffs
	uint32 SelectedActorIndex = 0;
	uint32 SelectedComponentIndex = 0;

	FEditorConsole ConsoleInstance;

	// Text
	int SelectedAlignment = 0;

private:
	void RenderControlTab(float DeltaTime, FViewOutput& ViewOutput);
	void RenderObjectManager(FViewOutput& ViewOutput);
	void RenderPickedObjectWindow(UObject*& Object);
	void RenderPickedActorWindow(AActor* Actor);
	void RenderPicekdSubUVWindow(USubUVComponent* SubUVComp);
	void DrawTextComponentUI(UTextComponent* TextComp);
public:
	void Create(HWND InHWindow, FRenderer& InRenderer, FEditorEngine* InEditorEngine, FEditorViewportClient* InEditorViewport);
	void Release();
	void Render(float DeltaTime, FViewOutput& ViewOutput);
	void Update();
};
