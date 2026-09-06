#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

// ============================================================
// FWorldSettings — UWorld 단위 (= Scene 파일 단위) 의 게임 설정.
//
// 의도: ProjectSettings 가 "프로젝트 전역 설정" 이라면 WorldSettings 는 "이 씬 한정 설정".
// 예: 이 씬은 어떤 GameMode 를 쓸지 (Intro.Scene = AGameModeIntro / Map.Scene =
//     AGameModeCarGame). 향후 spawn 포지션, 기본 fog, 매치 시간 등도 여기에 누적.
//
// SceneSaveManager 가 scene JSON 의 "WorldSettings" 객체로 직렬화. 비어있으면
// 호출자 측 default (UGameEngine 의 ProjectSettings → AGameModeCarGame fallback) 가 적용.
// ============================================================
struct FNavigationWorldSettings
{
	float CellSize = 0.75f;
	int32 MaxSearchNodes = 32768;
	float AgentRadius = 0.42f;
	float AgentHeight = 2.10f;
	float AgentStepHeight = 0.60f;
	float AgentMaxClimbHeight = 0.60f;
	float AgentMaxDropHeight = 0.60f;
	float AgentMaxSlopeDegrees = 50.0f;
	float ProjectionUp = 5.0f;
	float ProjectionDown = 10.0f;
	float DirectPathSegmentLength = 0.75f;
	float ObstaclePadding = 0.05f;
	bool bUsePhysicsProjectionFallback = false;
	bool bUseNavigationData = true;
	bool bAutoRebuildOnPathRequest = true;
	bool bAllowRuntimeFallback = false;
	bool bEnableRuntimeAutoRebuild = true;
	float RuntimeRebuildDelay = 0.35f;
	float RuntimeRebuildMinInterval = 0.50f;
	bool bDrawDebugOnBuild = true;
	bool bDrawBlockedCells = true;
	bool bDrawHeightColors = true;
	bool bDrawHeightContours = true;
	float DebugHeightContourInterval = 0.50f;
	float DebugDrawDuration = 30.0f;
	int32 DebugDrawMaxCells = 20000;
};

struct FWorldSettings
{
	// 비우면 ProjectSettings.GameModeClassName 또는 코드 default 가 fallback.
	// 채우면 LoadSceneFromPath 가 UClass::FindByName 으로 resolve.
	FString GameModeClassName;

	// World-space gravity in m/s^2. Legacy scenes without this field use earth gravity.
	FVector Gravity = FVector(0.0f, 0.0f, -9.81f);

	// Scene-local navigation build/runtime settings. These are copied into the
	// world's UNavigationSystem when the scene is loaded or edited.
	FNavigationWorldSettings Navigation;
};
