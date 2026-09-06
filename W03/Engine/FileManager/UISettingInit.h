#pragma once
#include "Core/CoreTypes.h"
#include "Format_ini.h"

class FUISettingInitializer {
private:
	static IniFile ViewportDump;
	static FString ViewportSavePath;

	// Editor Camera
	static float ViewCamMoveSpeed;
	static float ViewCamRotSpeed;
	static uint32 GridSize;

public:
	// Editor Camera Getters
	static float GetViewCamMoveSpeed() { return ViewCamMoveSpeed; }
	static float GetViewCamRotSpeed() { return ViewCamRotSpeed; }
	static uint32 GetGridSize() { return GridSize; }

	// Editor Viewport Camera Setters
	static void SetViewCamMoveSpeed(float Speed) { ViewCamMoveSpeed = Speed; }
	static void SetViewCamRotSpeed(float Speed) { ViewCamRotSpeed = Speed; }
	static void SetGridSize(uint32 Size) { GridSize = Size; }

	static void SaveEditorViewSettings();
	static void LoadEditorViewSettings(); // Fixed load path
};