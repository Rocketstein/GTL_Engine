#include "Core/ProjectSettings.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>

namespace PSKey
{
	constexpr const char* Shadow = "Shadow";
	constexpr const char* bShadows = "bShadows";
	constexpr const char* CSMResolution = "CSMResolution";
	constexpr const char* SpotAtlasResolution = "SpotAtlasResolution";
	constexpr const char* PointAtlasResolution = "PointAtlasResolution";
	constexpr const char* MaxSpotAtlasPages = "MaxSpotAtlasPages";
	constexpr const char* MaxPointAtlasPages = "MaxPointAtlasPages";
	constexpr const char* LightCulling = "LightCulling";
	constexpr const char* LightCullingMode = "LightCullingMode";
	constexpr const char* HeatMapMax = "HeatMapMax";
	constexpr const char* Enable25DCulling = "Enable25DCulling";
	constexpr const char* SceneDepth = "SceneDepth";
	constexpr const char* SceneDepthMode = "SceneDepthMode";
	constexpr const char* SceneDepthExponent = "SceneDepthExponent";
	constexpr const char* Performance = "Performance";
	constexpr const char* LimitFPS = "LimitFPS";
	constexpr const char* MaxFPS = "MaxFPS";
	constexpr const char* Game = "Game";
	constexpr const char* GameInstanceClass = "GameInstanceClass";
	constexpr const char* DefaultGameModeClass = "DefaultGameModeClass";
	constexpr const char* DefaultScene = "DefaultScene";
	constexpr const char* WindowWidth = "WindowWidth";
	constexpr const char* WindowHeight = "WindowHeight";
	constexpr const char* LockWindowResolution = "LockWindowResolution";
}

void FProjectSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	JSON ShadowObj = Object();
	ShadowObj[PSKey::bShadows] = Shadow.bEnabled;
	ShadowObj[PSKey::CSMResolution] = static_cast<int>(Shadow.CSMResolution);
	ShadowObj[PSKey::SpotAtlasResolution] = static_cast<int>(Shadow.SpotAtlasResolution);
	ShadowObj[PSKey::PointAtlasResolution] = static_cast<int>(Shadow.PointAtlasResolution);
	ShadowObj[PSKey::MaxSpotAtlasPages] = static_cast<int>(Shadow.MaxSpotAtlasPages);
	ShadowObj[PSKey::MaxPointAtlasPages] = static_cast<int>(Shadow.MaxPointAtlasPages);
	Root[PSKey::Shadow] = ShadowObj;

	JSON LightCullingObj = Object();
	LightCullingObj[PSKey::LightCullingMode] = static_cast<int>(LightCulling.Mode);
	LightCullingObj[PSKey::HeatMapMax] = LightCulling.HeatMapMax;
	LightCullingObj[PSKey::Enable25DCulling] = LightCulling.bEnable25DCulling;
	Root[PSKey::LightCulling] = LightCullingObj;

	JSON SceneDepthObj = Object();
	SceneDepthObj[PSKey::SceneDepthMode] = static_cast<int>(SceneDepth.Mode);
	SceneDepthObj[PSKey::SceneDepthExponent] = SceneDepth.Exponent;
	Root[PSKey::SceneDepth] = SceneDepthObj;

	JSON PerformanceObj = Object();
	PerformanceObj[PSKey::LimitFPS] = Performance.bLimitFPS;
	PerformanceObj[PSKey::MaxFPS] = static_cast<int>(Performance.MaxFPS);
	Root[PSKey::Performance] = PerformanceObj;

	JSON GameObj = Object();
	GameObj[PSKey::GameInstanceClass] = Game.GameInstanceClass;
	GameObj[PSKey::DefaultGameModeClass] = Game.DefaultGameModeClass;
	GameObj[PSKey::DefaultScene] = Game.DefaultScene;
	GameObj[PSKey::WindowWidth] = static_cast<int>(Game.WindowWidth);
	GameObj[PSKey::WindowHeight] = static_cast<int>(Game.WindowHeight);
	GameObj[PSKey::LockWindowResolution] = Game.bLockWindowResolution;
	Root[PSKey::Game] = GameObj;

	std::filesystem::path FilePath(FPaths::ToWide(Path));
	if (FilePath.has_parent_path())
		std::filesystem::create_directories(FilePath.parent_path());

	std::ofstream File(FilePath);
	if (File.is_open())
		File << Root;
}

void FProjectSettings::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
		return;

	FString Content((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);

	if (Root.hasKey(PSKey::Shadow))
	{
		JSON S = Root[PSKey::Shadow];
		if (S.hasKey(PSKey::bShadows))
			Shadow.bEnabled = S[PSKey::bShadows].ToBool();
		if (S.hasKey(PSKey::CSMResolution))
		{
			int v = S[PSKey::CSMResolution].ToInt();
			Shadow.CSMResolution = static_cast<uint32>((std::max)(64, (std::min)(v, 8192)));
		}
		if (S.hasKey(PSKey::SpotAtlasResolution))
		{
			int v = S[PSKey::SpotAtlasResolution].ToInt();
			Shadow.SpotAtlasResolution = static_cast<uint32>((std::max)(64, (std::min)(v, 8192)));
		}
		if (S.hasKey(PSKey::PointAtlasResolution))
		{
			int v = S[PSKey::PointAtlasResolution].ToInt();
			Shadow.PointAtlasResolution = static_cast<uint32>((std::max)(64, (std::min)(v, 8192)));
		}
		if (S.hasKey(PSKey::MaxSpotAtlasPages))
		{
			int v = S[PSKey::MaxSpotAtlasPages].ToInt();
			Shadow.MaxSpotAtlasPages = static_cast<uint32>(v > 1 ? v : 1);
		}
		if (S.hasKey(PSKey::MaxPointAtlasPages))
		{
			int v = S[PSKey::MaxPointAtlasPages].ToInt();
			Shadow.MaxPointAtlasPages = static_cast<uint32>(v > 1 ? v : 1);
		}
	}

	if (Root.hasKey(PSKey::LightCulling))
	{
		JSON L = Root[PSKey::LightCulling];
		if (L.hasKey(PSKey::LightCullingMode))
		{
			int v = L[PSKey::LightCullingMode].ToInt();
			LightCulling.Mode = static_cast<uint32>((std::max)(0, (std::min)(v, 2)));
		}
		if (L.hasKey(PSKey::HeatMapMax))
		{
			float v = static_cast<float>(L[PSKey::HeatMapMax].ToFloat());
			LightCulling.HeatMapMax = (std::max)(1.0f, (std::min)(v, 100.0f));
		}
		if (L.hasKey(PSKey::Enable25DCulling))
		{
			LightCulling.bEnable25DCulling = L[PSKey::Enable25DCulling].ToBool();
		}
	}

	if (Root.hasKey(PSKey::SceneDepth))
	{
		JSON D = Root[PSKey::SceneDepth];
		if (D.hasKey(PSKey::SceneDepthMode))
		{
			int v = D[PSKey::SceneDepthMode].ToInt();
			SceneDepth.Mode = static_cast<uint32>((std::max)(0, (std::min)(v, 1)));
		}
		if (D.hasKey(PSKey::SceneDepthExponent))
		{
			float v = static_cast<float>(D[PSKey::SceneDepthExponent].ToFloat());
			SceneDepth.Exponent = (std::max)(1.0f, (std::min)(v, 512.0f));
		}
	}

	if (Root.hasKey(PSKey::Performance))
	{
		JSON P = Root[PSKey::Performance];
		if (P.hasKey(PSKey::LimitFPS))
		{
			Performance.bLimitFPS = P[PSKey::LimitFPS].ToBool();
		}
		if (P.hasKey(PSKey::MaxFPS))
		{
			int v = P[PSKey::MaxFPS].ToInt();
			Performance.MaxFPS = static_cast<uint32>((std::max)(1, (std::min)(v, 1000)));
		}
	}

	if (Root.hasKey(PSKey::Game))
	{
		JSON G = Root[PSKey::Game];
		if (G.hasKey(PSKey::GameInstanceClass))
		{
			Game.GameInstanceClass = G[PSKey::GameInstanceClass].ToString();
		}
		if (G.hasKey(PSKey::DefaultGameModeClass))
		{
			Game.DefaultGameModeClass = G[PSKey::DefaultGameModeClass].ToString();
		}
		if (G.hasKey(PSKey::DefaultScene))
		{
			Game.DefaultScene = G[PSKey::DefaultScene].ToString();
		}
		if (G.hasKey(PSKey::WindowWidth))
		{
			int v = G[PSKey::WindowWidth].ToInt();
			Game.WindowWidth = static_cast<uint32>((std::max)(320, (std::min)(v, 7680)));
		}
		if (G.hasKey(PSKey::WindowHeight))
		{
			int v = G[PSKey::WindowHeight].ToInt();
			Game.WindowHeight = static_cast<uint32>((std::max)(240, (std::min)(v, 4320)));
		}
		if (G.hasKey(PSKey::LockWindowResolution))
		{
			Game.bLockWindowResolution = G[PSKey::LockWindowResolution].ToBool();
		}
	}
}
