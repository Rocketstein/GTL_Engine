#pragma once
#include <filesystem>
#include <memory>
#include <Sol/forward.hpp>

#include "Core/CoreTypes.h"

class FLuaScriptAsset;
struct FLuaScriptPropertyOverride;

class FScriptSystem
{
public:
	FScriptSystem();
	~FScriptSystem();

	bool Initialize();
	void Shutdown();

	sol::state& GetLua() const { return *Lua; }

	void ScanScripts();
	void LoadAllScripts();
	void ReloadChangedScripts() const;
	void TickEditor(float DeltaTime);
	
	FLuaScriptAsset* GetScriptAsset(const FString& RelativePath);
	const TArray<FString>& GetAvailableScriptPaths() const { return AvailableScriptPaths; }

	sol::table CreateScriptInstance(const FString& RelativePath, const TArray<FLuaScriptPropertyOverride>* PropertyOverrides = nullptr);
private:
	FString MakeScriptRelativePath(const std::filesystem::path& FullPath) const;

	void RegisterEngineAPI() const;

	void BindPackagePath() const;
	
private:
	std::unique_ptr<sol::state> Lua;

	TMap<FString, std::unique_ptr<FLuaScriptAsset>> ScriptAssets;
	TArray<FString> AvailableScriptPaths;
	float EditorRefreshAccumulator = 0.0f;
};
