#pragma once

#include "World.h"
#include "Engine/Scene/Camera.h"
#include "World/Gizmo/GizmoManager.h"
#include "FileManager/UISettingInit.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Scene/RenderBus.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/UI/EditorMainPanel.h"

#include "Engine/FileManager/SceneSaveManager.h"

class FEditorEngine
{
private:
	struct FRuntimeSettings
	{
		bool bLimitUpdateRate = true;
		int UpdateRate = 60;
	};

	UWorld* EditorWorld = nullptr;
	HWND HWindow = nullptr;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	// Initial Camera Dimensions
	FVector InitViewPos = FVector(10, 0, 5);
	FVector InitLookAt = FVector(0, 0, 0);

	FRenderer Renderer;
	FRenderBus RenderBus;
	FEditorMainPanel MainPanel;
	FEditorViewportClient ViewportClient;
	float MainLoopFPS = 0.0f;
	FRuntimeSettings RuntimeSettings;

private:
	void UpdateWorld(float DeltaTime);
	void BuildRenderCommands();


public:
	void Create(HWND InHWindow);
	void Release();
	void OnWindowResized(uint32 Width, uint32 Height);
	void BeginPlay();
	void BeginFrame(float DeltaTime);
	void Update(float DeltaTime);
	void Render(float DeltaTime);
	void EndFrame();

	UWorld* GetWorld() const { return EditorWorld; }
	void NewScene();
	void ResetViewportScene();
	void SetMainLoopFPS(float InFPS) { MainLoopFPS = InFPS; }
	float GetMainLoopFPS() const { return MainLoopFPS; }

	bool IsUpdateRateLimited() const { return RuntimeSettings.bLimitUpdateRate; }
	void SetUpdateRateLimited(bool bLimited) { RuntimeSettings.bLimitUpdateRate = bLimited; }
	int GetUpdateRate() const { return RuntimeSettings.UpdateRate; }
	void SetUpdateRate(int NewRate) { RuntimeSettings.UpdateRate = (NewRate < 1) ? 1 : NewRate; }

	template <typename T>
	AActor* SpawnNewPrimitiveActor(const FVector& Location) {
		if (!EditorWorld) return nullptr;
		return EditorWorld->SpawnPrimitiveActor<T>(Location);
	}

	template <typename T>
	AActor* SpawnNewUtilActor(const FVector& Location) {
		if (!EditorWorld) return nullptr;
		return EditorWorld->SpawnUtilActor<T>(Location);
	}
};
