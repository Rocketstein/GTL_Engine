#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Scripting/LuaActorProxy.h"
#include "Scripting/LuaComponentProxy.h"

#include <memory>

class UScriptComponent;

namespace sol
{
	class state;
}

// =================================================================
// Lua 의 Runtime 수명 주기를 관리하는 Manager 클래스
// =================================================================
class FLuaScriptRuntime : public TSingleton<FLuaScriptRuntime>
{
	friend class TSingleton<FLuaScriptRuntime>;
public:
	FLuaScriptRuntime();
	~FLuaScriptRuntime();

	// 전역 Lua VM, 기본 타입 바인딩, Scripts watcher를 준비한다.
	bool Initialize();

	// watcher 구독과 등록된 component 목록, Lua VM을 모두 정리한다.
	void Shutdown();

	// 모든 ScriptInstance가 공유하는 전역 Lua 상태를 반환한다.
	sol::state& GetLuaState();

	const FString& GetLastError() const;

	// 플레이 중인 ScriptComponent만 등록 대상으로 관리한다.
	void RegisterScriptComponent(UScriptComponent* Component);
	void UnregisterScriptComponent(UScriptComponent* Component);

	bool bIsInitialized() const { return bInitialized; }

private:
	// Lua에 노출할 공용 타입/함수를 한 곳에서 등록한다.
	void RegisterBindings();

	void BindVectorType();
	void BindRotatorType();
	void BindComponentProxyType();
	void BindActorProxyType();
	void BindColorType();

	// Scripts/ 폴더 watcher를 붙였다가 떼는 수명 관리 함수
	void InitializeHotReload();
	void ShutdownHotReload();

	// DirectoryWatcher가 모아준 변경 파일 집합을 받아
	// 해당 스크립트를 사용하는 컴포넌트만 선별해서 ReloadScript()를 호출한다.
	void OnScriptsChanged(const TSet<FString>& ChangedFiles);

private:
	struct FRuntimeImpl;
	std::unique_ptr<FRuntimeImpl> Impl;
	bool bInitialized = false;
	FString LastError;
	uint32 WatchSub = 0;

	// UObject 수명은 외부에서 관리하므로 소유하지 않는다.
	// 죽은 객체는 hot-reload 처리 중 정리한다.
	TSet<UScriptComponent*> ScriptComponents;
};
