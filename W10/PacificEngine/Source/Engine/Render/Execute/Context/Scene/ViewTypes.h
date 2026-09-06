#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

/*
    뷰 모드, ShowFlag, 뷰포트 카메라 프리셋, 뷰포트 렌더 옵션을 정의합니다.
    에디터 뷰포트와 렌더 파이프라인이 공통으로 참조합니다.
*/

enum class EViewMode : int32
{
    Unlit       = 0,
    Wireframe   = 1,
    SceneDepth  = 2,
    Lit_Lambert = 3,
    Lit_Phong   = 4,
    WorldNormal = 5,
    Lit         = Lit_Phong,
    Count
};

enum class ERenderShadingPath : int32
{
    Forward = 0,
};

// FShowFlags는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FShowFlags
{
    bool  bPrimitives                = true;
    bool  bUI                        = true;
    bool  bText                      = true;
    bool  bGrid                      = true;
    bool  bWorldAxis                 = true;
    bool  bGizmo                     = true;
    bool  bUUIDText                  = true;
    bool  bSceneBVH                  = false;
    bool  bSceneOctree               = false;
    bool  bBoundingVolume            = false;
    bool  bWorldBound                = false;
    bool  bLightDebugLines           = true;
	bool  bSkeletonMesh				 = true;
	bool  bSkeletalDebug			 = false;
    float DirectionalLightDebugScale = 1.0f;
    float PointLightDebugScale       = 1.0f;
    float SpotLightDebugScale        = 1.0f;

	// Viewport용 post-process 토글.
    bool  bFog                       = true;
    bool  bFXAA                      = true;
    bool  bVignetting                = true;
    bool  bGammaCorrection           = true;
    bool  bLightHitMap               = false;
    bool  b25DCulling                = true; // false는 tile_based_culling
};

struct FVignettingSettings
{
    bool    bEnabled  = false;
    float   Intensity = 0.5f;
    float   Radius    = 0.75f;
    float   Softness  = 0.25f;
    FVector Color     = FVector(0.0f, 0.0f, 0.0f);
};

struct FGammaCorrectionSettings
{
    bool  bEnabled     = true;
    float DisplayGamma = 2.2f;
};

struct FLetterboxSettings
{
    bool    bEnabled          = false;
    float   TargetAspectRatio = 2.39f;
    float   Opacity           = 1.0f;
    FVector Color             = FVector(0.0f, 0.0f, 0.0f);
};

struct FFadeSettings
{
    bool    bEnabled = false;
    float   Alpha    = 0.0f;
    FVector Color    = FVector(0.0f, 0.0f, 0.0f);
};

struct FPostProcessSettings
{
    FVignettingSettings       Vignetting;
    FGammaCorrectionSettings  GammaCorrection;
    FFadeSettings             Fade;
    FLetterboxSettings        Letterbox;
};

// 뷰포트 카메라 프리셋 (Perspective / 6방향 Orthographic)
enum class ELevelViewportType : uint8
{
    Perspective,
    Top,             // +Z → -Z
    Bottom,          // -Z → +Z
    Left,            // -Y → +Y
    Right,           // +Y → -Y
    Front,           // +X → -X
    Back,            // -X → +X
    FreeOrthographic // 자유 각도 Orthographic
};

// 뷰포트별 렌더 옵션 — 각 뷰포트 클라이언트가 독립적으로 소유
struct FViewportRenderOptions
{
    EViewMode  ViewMode = EViewMode::Lit_Phong;
    FShowFlags ShowFlags;
    FVignettingSettings      Vignetting;
    FGammaCorrectionSettings GammaCorrection;

    float GridSpacing       = 1.0f;
    int32 GridHalfLineCount = 100;

    float              CameraMoveSensitivity   = 1.0f;
    float              CameraRotateSensitivity = 1.0f;
    ELevelViewportType ViewportType            = ELevelViewportType::Perspective;

    // Scene Depth 전용 설정
    int32 SceneDepthVisMode = 1;
    float Exponent          = 1.0f;
    float Range             = 16.0f;

    // FXAA 전용 설정
    float EdgeThreshold    = 0.125f;
    float EdgeThresholdMin = 0.0625f;
};
