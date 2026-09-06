#pragma once

#include "Core/Types/CoreTypes.h"
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

	// --- Game ---
	struct FGameOption
	{
		FString StartLevelName;     // Scene 파일 이름 (확장자 제외)
		FString GameModeClassName;  // ""면 GameEngine이 코드로 지정한 디폴트 사용.
		                            // 잘못된 이름이거나 AGameModeBase 파생이 아니면 디폴트 fallback.
	};

	// --- Physics ---
	struct FPhysicsOption
	{
		uint32 WorkerThreadCount = 0;   // 0 = auto (hardware_concurrency - 2, 최소 1)
		bool bEnableCCD = true;
		bool bEnablePCM = true;
		bool bEnableActiveActors = true;
		bool bUseRagdollAggregate = true;   // 랙돌 바디를 PxAggregate(selfCollision=false)로 묶을지(성능 A/B 토글). 끄면 씬에 직접 추가(self-collision 은 filter word3 로 계속 차단).
		bool bEnablePVD = false;
		FString PvdHost = "127.0.0.1";
		uint32 PvdPort = 5425;
		uint32 PvdTimeoutMs = 10;
		bool bPvdTransmitContacts = true;
		bool bPvdTransmitConstraints = true;
		bool bPvdTransmitSceneQueries = false;
	};

public:
	FShadowOption Shadow;
	FGameOption Game;
	FPhysicsOption Physics;

	// --- 직렬화 ---
	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultPath() { return FPaths::ToUtf8(FPaths::ProjectSettingsFilePath()); }
};
