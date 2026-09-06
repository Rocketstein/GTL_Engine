#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Platform/Paths.h"

/*
	FProjectSettings — 프로젝트 전역 설정 (per-viewport가 아닌 전체 공유).
	Settings/ProjectSettings.ini에 독립 직렬화됩니다.
*/
class FProjectSettings : public TSingleton<FProjectSettings>
{
	friend class TSingleton<FProjectSettings>;

	// --- Shadow ---
	struct FShadowOption
	{
		bool bEnabled = true;
		uint32 CSMResolution       = 2048;	// Directional Light CSM cascade 해상도
		uint32 SpotAtlasResolution = 4096;	// Spot Light Atlas page 해상도
		uint32 PointAtlasResolution = 4096;	// Point Light Atlas page 해상도
		uint32 MaxSpotAtlasPages   = 4;		// Spot Light Atlas 최대 page 수
		uint32 MaxPointAtlasPages  = 4;		// Point Light Atlas 최대 page 수
	};

	struct FLightCullingOption
	{
		uint32 Mode = 2; // ELightCullingMode::Cluster
		float HeatMapMax = 20.0f;
		bool bEnable25DCulling = true;
	};

	struct FSceneDepthOption
	{
		uint32 Mode = 0; // 0 = Power, 1 = Linear
		float Exponent = 128.0f;
	};

	struct FPerformanceOption
	{
		bool bLimitFPS = true;
		uint32 MaxFPS = 60;
	};

	struct FGameOption
	{
		FString GameInstanceClass = "UGameInstance";
		FString DefaultGameModeClass = "AGameModeBase";
		FString DefaultScene = "game/title.scene";
		uint32 WindowWidth = 1920;
		uint32 WindowHeight = 1080;
		bool bLockWindowResolution = false;
	};

public:
	FShadowOption Shadow;
	FLightCullingOption LightCulling;
	FSceneDepthOption SceneDepth;
	FPerformanceOption Performance;
	FGameOption Game;

	// --- 직렬화 ---
	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultPath() { return FPaths::ToUtf8(FPaths::ProjectSettingsFilePath()); }
};
