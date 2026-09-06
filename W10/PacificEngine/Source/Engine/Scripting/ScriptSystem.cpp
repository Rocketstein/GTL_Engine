#include "ScriptSystem.h"

#include <Sol/sol.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ranges>

#include "SimpleJSON/json.hpp"
#include "LuaScriptAsset.h"
#include "Core/Logging/LogMacros.h"
#include "Input/GameInput.h"
#include "Platform/Paths.h"
#include "LuaEngineBinding.h"
#include "GameFramework/World.h"
#include "Runtime/Engine.h"
#include "Sound/SoundManager.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"

namespace
{
constexpr float EditorScriptRefreshInterval = 0.5f;
constexpr int32 MaxScoreBoardRecords = 100;

struct FScoreBoardRecord
{
	FString ResultType;
	double RemainingTime = 0.0;
	int32 KillCount = 0;
	FString RecordedAt;
	int32 Rank = 0;
};

std::filesystem::path GetScoreBoardPath()
{
	return std::filesystem::path(FPaths::SavedDir()) / L"ScoreBoard.json";
}

FString MakeScoreBoardTimestamp()
{
	std::time_t Now = std::time(nullptr);
	std::tm LocalTime{};
	localtime_s(&LocalTime, &Now);

	char Buffer[32] = {};
	std::strftime(Buffer, sizeof(Buffer), "%Y-%m-%d %H:%M:%S", &LocalTime);
	return Buffer;
}

bool IsScoreRecordHigher(const FScoreBoardRecord& A, const FScoreBoardRecord& B)
{
	const double TimeDelta = A.RemainingTime - B.RemainingTime;
	if (std::fabs(TimeDelta) > 0.0001)
	{
		return A.RemainingTime < B.RemainingTime;
	}

	if (A.KillCount != B.KillCount)
	{
		return A.KillCount > B.KillCount;
	}

	return A.RecordedAt < B.RecordedAt;
}

void SortScoreBoardRecords(TArray<FScoreBoardRecord>& Records)
{
	std::stable_sort(Records.begin(), Records.end(), IsScoreRecordHigher);

	for (int32 Index = 0; Index < static_cast<int32>(Records.size()); ++Index)
	{
		Records[Index].Rank = Index + 1;
	}
}

TArray<FScoreBoardRecord> LoadScoreBoardRecordsFromDisk()
{
	TArray<FScoreBoardRecord> Records;
	const std::filesystem::path Path = GetScoreBoardPath();

	std::error_code Ec;
	if (!std::filesystem::exists(Path, Ec))
	{
		return Records;
	}

	std::ifstream File(Path, std::ios::binary);
	if (!File.is_open())
	{
		return Records;
	}

	const FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	if (Content.empty())
	{
		return Records;
	}

	json::JSON Root = json::JSON::Load(Content);
	if (!Root.hasKey("Records"))
	{
		return Records;
	}

	for (auto& Entry : Root["Records"].ArrayRange())
	{
		FScoreBoardRecord Record;
		Record.ResultType = Entry.hasKey("ResultType") ? Entry["ResultType"].ToString() : "GameOver";
		Record.RemainingTime = Entry.hasKey("RemainingTime") ? Entry["RemainingTime"].ToFloat() : 0.0;
		Record.KillCount = Entry.hasKey("KillCount") ? Entry["KillCount"].ToInt() : 0;
		Record.RecordedAt = Entry.hasKey("RecordedAt") ? Entry["RecordedAt"].ToString() : "";
		Records.push_back(Record);
	}

	SortScoreBoardRecords(Records);
	return Records;
}

void SaveScoreBoardRecordsToDisk(const TArray<FScoreBoardRecord>& Records)
{
	using namespace json;

	FPaths::CreateDir(FPaths::SavedDir());

	JSON Root = Object();
	JSON RecordArray = Array();
	for (const FScoreBoardRecord& Record : Records)
	{
		JSON Entry = Object();
		Entry["ResultType"] = Record.ResultType;
		Entry["RemainingTime"] = Record.RemainingTime;
		Entry["KillCount"] = Record.KillCount;
		Entry["RecordedAt"] = Record.RecordedAt;
		RecordArray.append(Entry);
	}

	Root["Records"] = RecordArray;

	std::ofstream File(GetScoreBoardPath(), std::ios::binary | std::ios::trunc);
	if (File.is_open())
	{
		File << Root.dump();
	}
}

int32 FindScoreBoardRecordRank(const TArray<FScoreBoardRecord>& Records, const FScoreBoardRecord& Needle)
{
	for (const FScoreBoardRecord& Record : Records)
	{
		if (Record.ResultType == Needle.ResultType &&
			Record.KillCount == Needle.KillCount &&
			Record.RecordedAt == Needle.RecordedAt &&
			std::fabs(Record.RemainingTime - Needle.RemainingTime) <= 0.0001)
		{
			return Record.Rank;
		}
	}

	return 0;
}

sol::table MakeLuaScoreRecord(sol::state_view Lua, const FScoreBoardRecord& Record)
{
	sol::table Table = Lua.create_table();
	Table["ResultType"] = Record.ResultType;
	Table["RemainingTime"] = Record.RemainingTime;
	Table["KillCount"] = Record.KillCount;
	Table["RecordedAt"] = Record.RecordedAt;
	Table["Rank"] = Record.Rank;
	return Table;
}

sol::table MakeLuaScoreRecordArray(sol::state_view Lua, const TArray<FScoreBoardRecord>& Records)
{
	sol::table Table = Lua.create_table();
	int32 LuaIndex = 1;
	for (const FScoreBoardRecord& Record : Records)
	{
		Table[LuaIndex++] = MakeLuaScoreRecord(Lua, Record);
	}
	return Table;
}

ESoundBus ParseSoundBus(const FString& BusName)
{
	FString Lower = BusName;
	std::transform(Lower.begin(), Lower.end(), Lower.begin(),
		[](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});

	if (Lower == "master")
	{
		return ESoundBus::Master;
	}
	if (Lower == "bgm")
	{
		return ESoundBus::BGM;
	}
	if (Lower == "ui")
	{
		return ESoundBus::UI;
	}
	if (Lower == "player")
	{
		return ESoundBus::Player;
	}
	if (Lower == "ambience" || Lower == "ambient")
	{
		return ESoundBus::Ambience;
	}

	return ESoundBus::SFX;
}

void ReadSoundPlayArgs(const sol::variadic_args& Args, FString& OutBusName, float& OutVolume)
{
	bool bBusRead = false;
	bool bVolumeRead = false;

	for (const sol::object& Arg : Args)
	{
		if (!bBusRead && Arg.is<FString>())
		{
			OutBusName = Arg.as<FString>();
			bBusRead = true;
			continue;
		}

		if (!bVolumeRead && Arg.is<float>())
		{
			OutVolume = Arg.as<float>();
			bVolumeRead = true;
			continue;
		}

		if (!bVolumeRead && Arg.is<double>())
		{
			OutVolume = static_cast<float>(Arg.as<double>());
			bVolumeRead = true;
			continue;
		}

		if (!bVolumeRead && Arg.is<int>())
		{
			OutVolume = static_cast<float>(Arg.as<int>());
			bVolumeRead = true;
		}
	}
}

float ReadOptionalScaleArg(const sol::variadic_args& Args, float DefaultScale)
{
	for (const sol::object& Arg : Args)
	{
		if (Arg.is<float>())
		{
			return Arg.as<float>();
		}
		if (Arg.is<double>())
		{
			return static_cast<float>(Arg.as<double>());
		}
		if (Arg.is<int>())
		{
			return static_cast<float>(Arg.as<int>());
		}
	}

	return DefaultScale;
}

FString ResolveCameraShakePath(const FString& Path)
{
	if (Path.empty())
	{
		return FString();
	}

	if (Path.find("Asset/") == 0 || Path.find("Asset\\") == 0 || Path.find(':') != FString::npos)
	{
		return Path;
	}

	return FPaths::ContentRelativePath(Path);
}
}

FScriptSystem::FScriptSystem()
= default;

FScriptSystem::~FScriptSystem()
{
	Shutdown();
}

bool FScriptSystem::Initialize()
{
	Lua = std::make_unique<sol::state>();
	EditorRefreshAccumulator = 0.0f;

	// TODO : 나중에 필요하면 추가하기
	// sol::lib::package
	// sol::lib::debug
	// sol::lib::coroutine
    Lua->open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::math, sol::lib::table, sol::lib::string, sol::lib::package);
    BindPackagePath();

	RegisterEngineAPI();

	ScanScripts();
	LoadAllScripts();

	return true;
}

void FScriptSystem::Shutdown()
{
	ScriptAssets.clear();
	AvailableScriptPaths.clear();
	EditorRefreshAccumulator = 0.0f;
	Lua = nullptr;
}

void FScriptSystem::ScanScripts()
{
	AvailableScriptPaths.clear();

	const std::filesystem::path Root = FPaths::ToPath(FPaths::ScriptsDir());

	std::error_code Ec;
	if (!std::filesystem::exists(Root, Ec))
	{
		ScriptAssets.clear();
		return;
	}

	TSet<FString> Seen;

	for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(Root, Ec))
	{
	    if (Ec)
	    {
			break;
	    }

		if (!Entry.is_regular_file())
		{
			continue;
		}

		if (Entry.path().extension() != L".lua")
		{
			continue;
		}

		FString RelativePath = MakeScriptRelativePath(Entry.path());
		FString FullPath = FPaths::ToUtf8(Entry.path().wstring());
		Seen.insert(RelativePath);
		AvailableScriptPaths.push_back(RelativePath);

		if (!ScriptAssets.contains(RelativePath))
		{
			ScriptAssets.emplace(RelativePath, std::make_unique<FLuaScriptAsset>(RelativePath, FullPath));
		}
	}

	for (auto It = ScriptAssets.begin(); It != ScriptAssets.end();)
	{
	    if (!Seen.contains(It->first))
	    {
			It = ScriptAssets.erase(It);
	    }
		else
		{
			++It;
		}
	}

	std::ranges::sort(AvailableScriptPaths);
}

void FScriptSystem::LoadAllScripts()
{
	if (!Lua)
	{
		return;
	}

	for (const auto& Asset : ScriptAssets | std::views::values)
	{
		Asset->Load(*Lua);
	}
}

void FScriptSystem::ReloadChangedScripts() const
{
	if (!Lua)
	{
		return;
	}

	for (const auto& Val : ScriptAssets | std::views::values)
	{
		Val->ReloadIfChange(*Lua);
	}
}

void FScriptSystem::TickEditor(float DeltaTime)
{
	if (!Lua)
	{
		return;
	}

	if (DeltaTime > 0.0f)
	{
		EditorRefreshAccumulator += DeltaTime;
	}

	if (EditorRefreshAccumulator < EditorScriptRefreshInterval)
	{
		return;
	}

	EditorRefreshAccumulator = 0.0f;
	ScanScripts();
	ReloadChangedScripts();
}

FLuaScriptAsset* FScriptSystem::GetScriptAsset(const FString& RelativePath)
{
	auto It = ScriptAssets.find(RelativePath);
	if (It == ScriptAssets.end())
	{
		return nullptr;
	}

	return It->second.get();
}

sol::table FScriptSystem::CreateScriptInstance(const FString& RelativePath, const TArray<FLuaScriptPropertyOverride>* PropertyOverrides)
{
	if (!Lua)
	{
		return {};
	}

	FLuaScriptAsset* Asset = GetScriptAsset(RelativePath);
	if (!Asset || !Asset->IsUsable())
	{
		return {};
	}

	return Asset->CreateInstance(*Lua, PropertyOverrides);
}

FString FScriptSystem::MakeScriptRelativePath(const std::filesystem::path& FullPath) const
{
	const std::filesystem::path Root = FPaths::ToPath(FPaths::ScriptsDir());

	std::error_code Ec;
	std::filesystem::path Relative = std::filesystem::relative(FullPath, Root, Ec);
	if (Ec)
	{
		Relative = FullPath.filename();
	}

	return FPaths::ToUtf8(Relative.generic_wstring());
}

void FScriptSystem::RegisterEngineAPI() const
{
	Lua->set_function("Log", [](const FString& Message)
		{
			UE_LOG([Lua], Info, "%s", Message.c_str());
		});

	Lua->new_usertype<FSoundHandle>(
		"SoundHandle",
		sol::constructors<FSoundHandle()>(),
		"Id", &FSoundHandle::Id,
		"Generation", &FSoundHandle::Generation,
		"IsValid", &FSoundHandle::IsValid);

	Lua->set_function("PlaySound", [](const FString& Key, const sol::variadic_args& Args) -> FSoundHandle
		{
			FString BusName = "SFX";
			float Volume = 1.0f;
			ReadSoundPlayArgs(Args, BusName, Volume);
			return FSoundManager::Get().Play(FName(Key), ParseSoundBus(BusName), Volume);
		});

	Lua->set_function("PlayLoopSound", [](const FString& Key, const sol::variadic_args& Args) -> FSoundHandle
		{
			FString BusName = "SFX";
			float Volume = 1.0f;
			ReadSoundPlayArgs(Args, BusName, Volume);
			return FSoundManager::Get().PlayLoop(FName(Key), ParseSoundBus(BusName), Volume);
		});

	Lua->set_function("StopSound", [](const FSoundHandle& Handle)
		{
			FSoundManager::Get().Stop(Handle);
		});

	Lua->set_function("StopSoundBus", [](const FString& BusName)
		{
			FSoundManager::Get().StopBus(ParseSoundBus(BusName));
		});

	Lua->set_function("SetSoundBusVolume", [](const FString& BusName, float Volume)
		{
			FSoundManager::Get().SetBusVolume(ParseSoundBus(BusName), Volume);
		});

	Lua->set_function("SetMasterSoundVolume", [](float Volume)
		{
			FSoundManager::Get().SetMasterVolume(Volume);
		});

	Lua->set_function("SetSoundVolume", [](const FSoundHandle& Handle, float Volume)
		{
			FSoundManager::Get().SetSoundVolume(Handle, Volume);
		});

	Lua->set_function("SetSoundPosition", [](const FSoundHandle& Handle, float PositionMs)
		{
			const float ClampedPositionMs = PositionMs < 0.0f ? 0.0f : PositionMs;
			FSoundManager::Get().SetSoundPosition(Handle, static_cast<uint32>(ClampedPositionMs));
		});

	Lua->set_function("GetSoundPosition", [](const FSoundHandle& Handle) -> float
		{
			return static_cast<float>(FSoundManager::Get().GetSoundPosition(Handle));
		});

	Lua->set_function("GetSoundLength", [](const FString& Key) -> float
		{
			return static_cast<float>(FSoundManager::Get().GetSoundLength(FName(Key)));
		});

	Lua->set_function("IsSoundPlaying", [](const FSoundHandle& Handle) -> bool
		{
			return FSoundManager::Get().IsPlaying(Handle);
		});

	Lua->set_function("PlayCameraShake", [](const FString& Path, const sol::variadic_args& Args) -> bool
		{
			if (!GEngine || !GEngine->GetGameViewportClient())
			{
				return false;
			}

			const FString ResolvedPath = ResolveCameraShakePath(Path);
			if (ResolvedPath.empty())
			{
				return false;
			}

			const float Scale = ReadOptionalScaleArg(Args, 1.0f);
			return GEngine->GetGameViewportClient()->StartCameraShakeFromAsset(ResolvedPath, Scale) != nullptr;
		});

	Lua->set_function("GetViewportAspectRatio", []() -> float
		{
			if (!GEngine || !GEngine->GetGameViewportClient())
			{
				return 16.0f / 9.0f;
			}

			FViewport* Viewport = GEngine->GetGameViewportClient()->GetViewport();
			if (!Viewport || Viewport->GetHeight() == 0)
			{
				return 16.0f / 9.0f;
			}

			return static_cast<float>(Viewport->GetWidth()) / static_cast<float>(Viewport->GetHeight());
		});
    
    // 입력 처리
    Lua->new_usertype<FGameInput>("Input",
        "GetKey", &FGameInput::GetKey,
        "GetKeyDown", &FGameInput::GetKeyDown,
        "GetKeyUp", &FGameInput::GetKeyUp,
        "GetAxis", &FGameInput::GetAxis,
        "GetMouseButton", &FGameInput::GetMouseButton,
        "GetMouseButtonDown", &FGameInput::GetMouseButtonDown,
        "GetMouseButtonUp", &FGameInput::GetMouseButtonUp,
        "GetMousePosition", []() {
            int x, y;
            FGameInput::GetMousePosition(x, y);
            return std::make_tuple(x, y);
        }
    );    

	Lua->set_function("GetActorPoolManager", []() {
		UWorld* World = GEngine->GetWorld();
		return FLuaActorPoolManagerHandle(World ? World->GetPoolManager() : nullptr);
	});

	Lua->set_function("ScoreBoard_LoadRecords", [](sol::this_state State) -> sol::table {
		sol::state_view Lua(State);
		return MakeLuaScoreRecordArray(Lua, LoadScoreBoardRecordsFromDisk());
	});

	Lua->set_function("ScoreBoard_AddRecord", [](sol::this_state State, const FString& ResultType, double RemainingTime, int32 KillCount) -> sol::table {
		sol::state_view Lua(State);

		FScoreBoardRecord AddedRecord;
		AddedRecord.ResultType = ResultType.empty() ? FString("GameOver") : ResultType;
		AddedRecord.RemainingTime = (std::max)(0.0, RemainingTime);
		AddedRecord.KillCount = (std::max)(0, KillCount);
		AddedRecord.RecordedAt = MakeScoreBoardTimestamp();

		TArray<FScoreBoardRecord> Records = LoadScoreBoardRecordsFromDisk();
		Records.push_back(AddedRecord);
		SortScoreBoardRecords(Records);

		const int32 Rank = FindScoreBoardRecordRank(Records, AddedRecord);

		if (Records.size() > MaxScoreBoardRecords)
		{
			Records.resize(MaxScoreBoardRecords);
		}

		SaveScoreBoardRecordsToDisk(Records);

		AddedRecord.Rank = Rank;
		sol::table Result = MakeLuaScoreRecord(Lua, AddedRecord);
		Result["Records"] = MakeLuaScoreRecordArray(Lua, Records);
		return Result;
	});

	RegisterLuaEngineBindings(*Lua);
}

void FScriptSystem::BindPackagePath() const
{
    sol::table Package = (*Lua)["package"];

    std::string OldPath = Package["path"].get_or<std::string>("");

    std::string ExtraPath =
        ";Asset/Scripts/?.lua"
        ";Asset/Scripts/?/init.lua";

    Package["path"] = OldPath + ExtraPath;
}
