#pragma once

#include "Core/Types/CoreTypes.h"
#include "Component/Camera/CameraComponent.h"
#include "Component/Camera/FPostProcessSettings.h"

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

enum class ESkinningMode : uint32
{
	CPU = 0,
	GPU = 1,
};

namespace SkinningModeRuntime
{
	inline ESkinningMode Current = ESkinningMode::GPU;

	inline ESkinningMode Get()
	{
		return Current;
	}

	inline void Set(ESkinningMode InMode)
	{
		Current = InMode;
	}
}

struct FShowFlags
{
	bool bStaticMesh = true;
	bool bSkeletalMesh = true;
	bool bGrid = true;
	bool bWorldAxis = true;
	bool bOriginAxisGizmo = false;
	bool bGizmo = true;
	bool bBillboardText = true;
	bool bBoundingVolume = false;
	bool bParticleEditorBounds = false;
	bool bDebugDraw = true;
	bool bOctree = false;
	bool bFog = true;
	bool bBloom = false;
	bool bDOF = false;
	bool bFXAA = false;
	bool bGammaCorrection = true;
	bool bViewLightCulling = false;
	bool bVisualize25DCulling = false;
	bool bShowShadowFrustum = false;
	bool bCollision = true;
	bool bParticle = true;
	bool bShowCollisionShape = false;	// PIE/Game에서 콘솔로 콜리전 shape 와이어프레임 강제 표시
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
	bool bParticleVectorFieldDebug = false;

	// Scene Depth 전용 설정
	int32 SceneDepthVisMode = 0;
	float Exponent = 128.0f;
	float Range = 1000.0f;

	// FXAA 전용 설정
	float EdgeThreshold = 0.125f;
	float EdgeThresholdMin = 0.0625f;

	// Bloom 전용 설정
	float BloomThreshold = 1.0f;
	float BloomSoftKnee = 0.5f;
	float BloomIntensity = 0.6f;
	float BloomBlurRadius = 1.0f;

	// Post Process 전용 설정
	FPostProcessSettings PostProcessSettings = {};

	// Tone Mapping / Gamma Correction 전용 설정
	float Gamma = 2.4f;
	float Exposure = 1.0f;

	// Light Culling 뷰모드 전용 설정
	ELightCullingMode LightCullingMode = ELightCullingMode::Cluster;
	float HeatMapMax = 20.0f;
	bool Enable25DCulling = true;

	// Mesh editor bone weight visualization
	bool bWeightBoneHeatMap = false;
	int32 WeightBoneHeatMapBoneIndex = -1;


public:
	void OverridePostProcess(const UCameraComponent* Camera)
	{
		if (Camera) PostProcessSettings = Camera->GetPostProcessSettings();
	}

	void SetPostProcess(const FPostProcessSettings& InSettings) { PostProcessSettings = InSettings; } 
};
