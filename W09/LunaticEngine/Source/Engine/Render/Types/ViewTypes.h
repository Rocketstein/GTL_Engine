#pragma once

#include "Core/CoreTypes.h"

enum class EViewMode : int32
{
	Lit_Phong = 0,
	Unlit,
	Lit_Gouraud,
	Lit_Lambert,
	Wireframe,
	SceneDepth,
	WorldNormal,
	LightCulling,
	Count
};

enum class ELightCullingMode : uint32
{
	Off = 0,
	Tile = 1,
	Cluster = 2
};

struct FShowFlags
{
	bool bPrimitives = true;
	bool bGrid = true;
	bool bWorldAxis = true;
	bool bGizmo = true;
	bool bBillboardText = true;
	bool bBoundingVolume = false;
	bool bDebugDraw = true;
	bool bSceneBVH = false;
	bool bOctree = false;
	bool bWorldBound = false;
	bool bLightVisualization = true;
	bool bLightHitMap = false;
	bool bFog = true;
	bool bFXAA = false;
	bool bViewLightCulling = false;
	bool bVisualize25DCulling = false;
	bool bShowShadowFrustum = false;
	bool bGammaCorrection = true;
};

// 뷰포트 카메라 프리셋 (Perspective / 6방향 Orthographic)
enum class ELevelViewportType : uint8
{
	Perspective,
	Top,		// +Z → -Z
	Bottom,		// -Z → +Z
	Left,		// -Y → +Y
	Right,		// +Y → -Y
	Front,		// +X → -X
	Back,		// -X → +X
	FreeOrthographic	// 자유 각도 Orthographic
};

// 뷰포트별 렌더 옵션 — 각 뷰포트 클라이언트가 독립적으로 소유
struct FViewportRenderOptions
{
	EViewMode ViewMode = EViewMode::Lit_Phong;
	FShowFlags ShowFlags;

	float GridSpacing = 1.0f;
	int32 GridHalfLineCount = 100;

	float CameraMoveSensitivity = 1.0f;
	float CameraRotateSensitivity = 1.0f;
	ELevelViewportType ViewportType = ELevelViewportType::Perspective;
	bool bGameView = false;

	// Scene Depth 전용 설정
	int32 SceneDepthVisMode = 0;
	float Exponent = 128.0f;
	float Range = 1000.0f;

	// FXAA 전용 설정
	float EdgeThreshold = 0.125f;
	float EdgeThresholdMin = 0.0625f;

	// Gamma correction output settings
	float DisplayGamma = 2.2f;
	float GammaCorrectionBlend = 1.0f;
	bool bUseSRGBCurve = true;

	// Light Culling 뷰모드 전용 설정
	ELightCullingMode LightCullingMode = ELightCullingMode::Cluster;
	float HeatMapMax = 20.0f;
	bool Enable25DCulling = true;

	// Light debug visualization
	float DebugLineThickness = 1.0f;
	float ActorHelperBillboardScale = 1.0f;
	float DirectionalLightVisualizationScale = 1.0f;
	float PointLightVisualizationScale = 1.0f;
	float SpotLightVisualizationScale = 1.0f;
};
