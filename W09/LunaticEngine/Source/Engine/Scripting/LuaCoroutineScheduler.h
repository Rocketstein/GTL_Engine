#pragma once

#include "Core/CoreTypes.h"

// Sol.hpp에 있는 Check 매크로 겹침 방지 목적 제거
#pragma region SolInclude
#ifdef check
#pragma push_macro("check")
#undef check
#define LUA_COROUTINE_SCHEDULER_RESTORE_CHECK_MACRO
#endif

#ifdef checkf
#pragma push_macro("checkf")
#undef checkf
#define LUA_COROUTINE_SCHEDULER_RESTORE_CHECKF_MACRO
#endif

#include "sol/sol.hpp"

// Sol.hpp include 완료 후 복구
#ifdef LUA_COROUTINE_SCHEDULER_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef LUA_COROUTINE_SCHEDULER_RESTORE_CHECKF_MACRO
#endif

#ifdef LUA_COROUTINE_SCHEDULER_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef LUA_COROUTINE_SCHEDULER_RESTORE_CHECK_MACRO
#endif
#pragma endregion

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

struct FLuaActorProxy;
class FLuaScriptInstance;

class FLuaCoroutineScheduler
{
public:
	// Lua coroutine은 OS thread가 아니라 한 Lua VM 안에서 협력적으로 yield/resume되는 실행 흐름이다.
	// StartCoroutine은 등록만 수행하고, 실제 resume은 Tick에서만 처리해서 이벤트 호출 중 nested yield를 막는다.
	enum class EWaitKind
	{
		None,
		Time,
		RealTime,
		Frames,
		MoveDone,
		KeyDown,
		Signal
	};

	struct FWaitCondition
	{
		EWaitKind Kind = EWaitKind::None;
		float TimeRemaining = 0.0f;
		int FramesRemaining = 0;
		FString Name;
		FString KeyName;
	};

	struct FRunningCoroutine
	{
		FString DebugName;
		uint64 Id = 0;

		// sol::thread는 Lua VM 안의 별도 runnable state/stack이다.
		// EntryFunction과 sol::coroutine을 함께 보관해 coroutine lifetime 동안 Lua reference가 살아 있게 한다.
		sol::thread Thread;
		sol::protected_function EntryFunction;
		sol::coroutine Coroutine;

		FWaitCondition Wait;
		bool bCancelRequested = false;
	};

public:
	void Initialize(
		FLuaScriptInstance* InOwner,
		sol::state* InLua,
		sol::environment* InEnv,
		FLuaActorProxy* InOwnerProxy);

	void BindToEnvironment();

	// StartCoroutine은 coroutine을 즉시 resume하지 않고 실행 대기열에만 등록한다.
	bool StartCoroutine(const FString& DebugName, sol::protected_function Function);

	// 여러 coroutine을 동시에 실행하는 것은 정상이며, Tick이 각 wait 조건을 확인해 순차적으로 resume한다.
	// GlobalTimeDilation 같은 전역 리소스는 token 또는 단일 관리자 함수로 충돌을 막아야 한다.
	void Tick(float DeltaTime, float RawDeltaTime);

	void StopAll();

	void InsertPendingSignal(const FString& Name);

private:
	void Enqueue(std::unique_ptr<FRunningCoroutine> Entry);
	void FlushPending();

	bool ShouldResume(FRunningCoroutine& Entry, float DeltaTime, float RawDeltaTime);
	bool ApplyYieldResult(FRunningCoroutine& Entry, sol::protected_function_result& Result);
	bool HandleCoroutineError(FRunningCoroutine& Entry, sol::protected_function_result&& Result);

	// wait command table은 현재 실행 중인 Lua thread 기준으로 만들어야 한다.
	sol::table MakeWaitCommand(sol::this_state ThisState, const FString& Type);

private:
	FLuaScriptInstance* Owner = nullptr;
	sol::state* Lua = nullptr;
	sol::environment* Env = nullptr;
	FLuaActorProxy* OwnerProxy = nullptr;

	std::vector<std::unique_ptr<FRunningCoroutine>> Running;
	std::vector<std::unique_ptr<FRunningCoroutine>> Pending;

	std::unordered_set<std::string> ActiveSignals;
	std::unordered_set<std::string> PendingSignals;

	bool bTicking = false;
	uint64 NextCoroutineId = 1;
};
