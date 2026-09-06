#include "Core/ProjectSettings.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>
#include <algorithm>

namespace PSKey
{
	constexpr const char* Shadow = "Shadow";
	constexpr const char* bShadows = "bShadows";
	constexpr const char* CSMResolution = "CSMResolution";
	constexpr const char* SpotAtlasResolution = "SpotAtlasResolution";
	constexpr const char* PointAtlasResolution = "PointAtlasResolution";
	constexpr const char* MaxSpotAtlasPages = "MaxSpotAtlasPages";
	constexpr const char* MaxPointAtlasPages = "MaxPointAtlasPages";

	constexpr const char* GameSection = "Game";
	constexpr const char* StartLevelName = "StartLevelName";
	constexpr const char* GameModeClassName = "GameModeClassName";

	constexpr const char* PhysicsSection = "Physics";
	constexpr const char* WorkerThreadCount = "WorkerThreadCount";
	constexpr const char* bEnableCCD = "bEnableCCD";
	constexpr const char* bEnablePCM = "bEnablePCM";
	constexpr const char* bEnableActiveActors = "bEnableActiveActors";
	constexpr const char* bUseRagdollAggregate = "bUseRagdollAggregate";
	constexpr const char* bEnablePVD = "bEnablePVD";
	constexpr const char* PvdHost = "PvdHost";
	constexpr const char* PvdPort = "PvdPort";
	constexpr const char* PvdTimeoutMs = "PvdTimeoutMs";
	constexpr const char* bPvdTransmitContacts = "bPvdTransmitContacts";
	constexpr const char* bPvdTransmitConstraints = "bPvdTransmitConstraints";
	constexpr const char* bPvdTransmitSceneQueries = "bPvdTransmitSceneQueries";
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

	JSON GameObj = Object();
	GameObj[PSKey::StartLevelName] = Game.StartLevelName;
	GameObj[PSKey::GameModeClassName] = Game.GameModeClassName;
	Root[PSKey::GameSection] = GameObj;

	JSON PhysicsObj = Object();
	PhysicsObj[PSKey::WorkerThreadCount] = static_cast<int>(Physics.WorkerThreadCount);
	PhysicsObj[PSKey::bEnableCCD] = Physics.bEnableCCD;
	PhysicsObj[PSKey::bEnablePCM] = Physics.bEnablePCM;
	PhysicsObj[PSKey::bEnableActiveActors] = Physics.bEnableActiveActors;
	PhysicsObj[PSKey::bUseRagdollAggregate] = Physics.bUseRagdollAggregate;
	PhysicsObj[PSKey::bEnablePVD] = Physics.bEnablePVD;
	PhysicsObj[PSKey::PvdHost] = Physics.PvdHost;
	PhysicsObj[PSKey::PvdPort] = static_cast<int>(Physics.PvdPort);
	PhysicsObj[PSKey::PvdTimeoutMs] = static_cast<int>(Physics.PvdTimeoutMs);
	PhysicsObj[PSKey::bPvdTransmitContacts] = Physics.bPvdTransmitContacts;
	PhysicsObj[PSKey::bPvdTransmitConstraints] = Physics.bPvdTransmitConstraints;
	PhysicsObj[PSKey::bPvdTransmitSceneQueries] = Physics.bPvdTransmitSceneQueries;
	Root[PSKey::PhysicsSection] = PhysicsObj;

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

	if (Root.hasKey(PSKey::GameSection))
	{
		JSON G = Root[PSKey::GameSection];
		if (G.hasKey(PSKey::StartLevelName))
			Game.StartLevelName = G[PSKey::StartLevelName].ToString();
		if (G.hasKey(PSKey::GameModeClassName))
			Game.GameModeClassName = G[PSKey::GameModeClassName].ToString();
	}

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

	if (Root.hasKey(PSKey::PhysicsSection))
	{
		JSON P = Root[PSKey::PhysicsSection];
		if (P.hasKey(PSKey::WorkerThreadCount))
		{
			int v = P[PSKey::WorkerThreadCount].ToInt();
			Physics.WorkerThreadCount = static_cast<uint32>((std::max)(0, (std::min)(v, 32)));  // 0 = auto
		}
		if (P.hasKey(PSKey::bEnableCCD))
			Physics.bEnableCCD = P[PSKey::bEnableCCD].ToBool();
		if (P.hasKey(PSKey::bEnablePCM))
			Physics.bEnablePCM = P[PSKey::bEnablePCM].ToBool();
		if (P.hasKey(PSKey::bEnableActiveActors))
			Physics.bEnableActiveActors = P[PSKey::bEnableActiveActors].ToBool();
		if (P.hasKey(PSKey::bUseRagdollAggregate))
			Physics.bUseRagdollAggregate = P[PSKey::bUseRagdollAggregate].ToBool();
		if (P.hasKey(PSKey::bEnablePVD))
			Physics.bEnablePVD = P[PSKey::bEnablePVD].ToBool();
		if (P.hasKey(PSKey::PvdHost))
			Physics.PvdHost = P[PSKey::PvdHost].ToString();
		if (P.hasKey(PSKey::PvdPort))
		{
			int v = P[PSKey::PvdPort].ToInt();
			Physics.PvdPort = static_cast<uint32>((std::max)(1, (std::min)(v, 65535)));
		}
		if (P.hasKey(PSKey::PvdTimeoutMs))
		{
			int v = P[PSKey::PvdTimeoutMs].ToInt();
			Physics.PvdTimeoutMs = static_cast<uint32>((std::max)(0, (std::min)(v, 60000)));
		}
		if (P.hasKey(PSKey::bPvdTransmitContacts))
			Physics.bPvdTransmitContacts = P[PSKey::bPvdTransmitContacts].ToBool();
		if (P.hasKey(PSKey::bPvdTransmitConstraints))
			Physics.bPvdTransmitConstraints = P[PSKey::bPvdTransmitConstraints].ToBool();
		if (P.hasKey(PSKey::bPvdTransmitSceneQueries))
			Physics.bPvdTransmitSceneQueries = P[PSKey::bPvdTransmitSceneQueries].ToBool();
	}
}
