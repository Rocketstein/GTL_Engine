#pragma once
#include "Core/CoreTypes.h"

#include <filesystem>
#include <cstdint>

#include "LuaScriptTypes.h"
#include "sol.hpp"

enum class ELuaScriptAssetState : uint8
{
    NotLoaded,
	Loaded,
	Error,
	Missing,
};

class FLuaScriptAsset
{
public:
	FLuaScriptAsset(FString InRelativePath, FString InFullPath);

	const FString& GetRelativePath() const { return RelativePath; }
	const FString& GetFullPath() const { return FullPath; }
	const FString& GetLastError() const { return LastError; }
	ELuaScriptAssetState GetState() const { return State; }
	uint64 GetVersion() const { return Version; }
	const TArray<FLuaScriptPropertyDesc>& GetPropertyDescriptors() const { return PropertyDescriptors; }

	bool IsUsable() const { return Prototype.valid(); }
	bool HasLoadError() const { return State == ELuaScriptAssetState::Error || State == ELuaScriptAssetState::Missing; }
	bool IsMissing() const { return State == ELuaScriptAssetState::Missing; }
    
	bool Load(sol::state& Lua);
	bool ReloadIfChange(sol::state& Lua);
	bool HasFileChanged() const;

	// UScriptComponent가 자기 인스턴스용 table/environment를 만들 때 사용.
	sol::table CreateInstance(sol::state& Lua, const TArray<FLuaScriptPropertyOverride>* PropertyOverrides = nullptr) const;

private:
	bool LoadInternal(sol::state& Lua);
	bool UpdateObservedFileState();
	bool ParsePropertyDescriptors(sol::table ScriptTable, TArray<FLuaScriptPropertyDesc>& OutDescriptors);


private:
	FString RelativePath;
	FString FullPath;
	FString SourceCode;
	FString LastError;

	std::filesystem::file_time_type LastWriteTime {};
	bool bHasObservedFileState = false;
	bool bLastFileExists = false;
	uint64 Version = 0;

	ELuaScriptAssetState State = ELuaScriptAssetState::NotLoaded;

	sol::table Prototype;
	TArray<FLuaScriptPropertyDesc> PropertyDescriptors;
};

