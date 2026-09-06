#include "Scripting/LuaCoroutineScheduler.h"

#include "Core/AsciiUtils.h"
#include "Core/Log.h"
#include "Engine/Input/InputManager.h"
#include "Scripting/LuaActorProxy.h"
#include "Scripting/LuaScriptInstance.h"

#include <algorithm>

namespace
{
#if defined(_DEBUG)
	struct FLuaStackGuard
	{
		lua_State* L = nullptr;
		int Top = 0;
		const char* Name = nullptr;

		FLuaStackGuard(lua_State* InL, const char* InName)
			: L(InL)
			, Top(InL ? lua_gettop(InL) : 0)
			, Name(InName)
		{
		}

		~FLuaStackGuard()
		{
			if (!L)
			{
				return;
			}

			const int Now = lua_gettop(L);
			if (Now != Top)
			{
				UE_LOG_CATEGORY(
					LuaScript,
					Warning,
					"[LuaStackGuard] Stack changed in %s: before=%d after=%d",
					Name ? Name : "<unknown>",
					Top,
					Now);

				lua_settop(L, Top);
			}
		}
	};
#else
	struct FLuaStackGuard
	{
		FLuaStackGuard(lua_State*, const char*) {}
	};
#endif

	bool TryParseVirtualKey(const FString& KeyName, int& OutVirtualKey)
	{
		if (KeyName.empty())
		{
			return false;
		}

		FString UpperKey = KeyName;
		AsciiUtils::ToUpperInPlace(UpperKey);

		if (UpperKey.size() == 1)
		{
			unsigned char KeyChar = static_cast<unsigned char>(UpperKey[0]);
			if ((KeyChar >= 'A' && KeyChar <= 'Z') || (KeyChar >= '0' && KeyChar <= '9'))
			{
				OutVirtualKey = KeyChar;
				return true;
			}
		}

		if (UpperKey == "SPACE")
		{
			OutVirtualKey = VK_SPACE;
			return true;
		}

		if (UpperKey == "ESC" || UpperKey == "ESCAPE")
		{
			OutVirtualKey = VK_ESCAPE;
			return true;
		}

		if (UpperKey == "LEFT")
		{
			OutVirtualKey = VK_LEFT;
			return true;
		}

		if (UpperKey == "RIGHT")
		{
			OutVirtualKey = VK_RIGHT;
			return true;
		}

		if (UpperKey == "UP")
		{
			OutVirtualKey = VK_UP;
			return true;
		}

		if (UpperKey == "DOWN")
		{
			OutVirtualKey = VK_DOWN;
			return true;
		}

		if (UpperKey == "CTRL" || UpperKey == "CONTROL" || UpperKey == "LCONTROL" || UpperKey == "LCTRL" || UpperKey == "RCONTROL" || UpperKey == "RCTRL")
		{
			OutVirtualKey = VK_CONTROL;
			return true;
		}

		if (UpperKey == "SHIFT" || UpperKey == "LSHIFT" || UpperKey == "RSHIFT")
		{
			OutVirtualKey = VK_SHIFT;
			return true;
		}

		if (UpperKey == "ALT" || UpperKey == "MENU" || UpperKey == "LALT" || UpperKey == "RALT")
		{
			OutVirtualKey = VK_MENU;
			return true;
		}

		return false;
	}

	sol::object FindLuaObjectByPath(sol::environment& Env, const FString& Path)
	{
		if (Path.empty())
		{
			return sol::lua_nil;
		}

		size_t SegmentStart = 0;
		size_t Dot = Path.find('.');
		FString Segment = Path.substr(0, Dot);
		sol::object Current = Env[Segment];

		while (Dot != FString::npos)
		{
			if (!Current.valid() || Current.get_type() != sol::type::table)
			{
				return sol::lua_nil;
			}

			sol::table Table = Current.as<sol::table>();
			SegmentStart = Dot + 1;
			Dot = Path.find('.', SegmentStart);
			Segment = Path.substr(
				SegmentStart,
				Dot == FString::npos ? FString::npos : Dot - SegmentStart);
			Current = Table[Segment];
		}

		return Current;
	}
}

void FLuaCoroutineScheduler::Initialize(
	FLuaScriptInstance* InOwner,
	sol::state* InLua,
	sol::environment* InEnv,
	FLuaActorProxy* InOwnerProxy)
{
	Owner = InOwner;
	Lua = InLua;
	Env = InEnv;
	OwnerProxy = InOwnerProxy;

	StopAll();
}

void FLuaCoroutineScheduler::BindToEnvironment()
{
	if (!Env)
	{
		return;
	}

	auto StartCoroutineEntry = [this](sol::object Entry)
	{
		if (!Entry.valid() || Entry == sol::lua_nil)
		{
			if (Owner)
			{
				Owner->SetError("StartCoroutine failed: entry must be a function name or function.");
			}
			return false;
		}

		if (Entry.get_type() == sol::type::function)
		{
			sol::protected_function Function = Entry.as<sol::protected_function>();

			if (!Function.lua_state() || !Function.valid())
			{
				if (Owner)
				{
					Owner->SetError("StartCoroutine failed: detached anonymous function.");
				}
				return false;
			}

			return StartCoroutine("<anonymous>", std::move(Function));
		}

		if (Entry.get_type() == sol::type::string)
		{
			const FString FunctionName = Entry.as<FString>();
			sol::object FunctionObject = FindLuaObjectByPath(*Env, FunctionName);
			if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
			{
				if (Owner)
				{
					Owner->SetError("StartCoroutine failed: missing Lua function '" + FunctionName + "'.");
				}
				return false;
			}

			return StartCoroutine(FunctionName, FunctionObject.as<sol::protected_function>());
		}

		if (Owner)
		{
			Owner->SetError("StartCoroutine failed: entry must be a function name or function.");
		}

		return false;
	};

	Env->set_function("StartCoroutine", StartCoroutineEntry);
	Env->set_function("start_coroutine", StartCoroutineEntry);

	Env->set_function("wait", sol::yielding([this](sol::this_state ThisState, float Seconds)
	{
		sol::table Command = MakeWaitCommand(ThisState, "time");
		Command["seconds"] = (std::max)(0.0f, Seconds);
		return Command;
	}));

	Env->set_function("Wait", sol::yielding([this](sol::this_state ThisState, float Seconds)
	{
		sol::table Command = MakeWaitCommand(ThisState, "time");
		Command["seconds"] = (std::max)(0.0f, Seconds);
		return Command;
	}));

	Env->set_function("wait_real", sol::yielding([this](sol::this_state ThisState, float Seconds)
	{
		sol::table Command = MakeWaitCommand(ThisState, "real_time");
		Command["seconds"] = (std::max)(0.0f, Seconds);
		return Command;
	}));

	Env->set_function("WaitReal", sol::yielding([this](sol::this_state ThisState, float Seconds)
	{
		sol::table Command = MakeWaitCommand(ThisState, "real_time");
		Command["seconds"] = (std::max)(0.0f, Seconds);
		return Command;
	}));

	Env->set_function("wait_frames", sol::yielding([this](sol::this_state ThisState, int Frames)
	{
		sol::table Command = MakeWaitCommand(ThisState, "frames");
		Command["frames"] = (std::max)(0, Frames);
		return Command;
	}));

	Env->set_function("wait_until_move_done", sol::yielding([this](sol::this_state ThisState)
	{
		return MakeWaitCommand(ThisState, "move_done");
	}));

	Env->set_function("wait_key_down", sol::yielding([this](sol::this_state ThisState, const FString& KeyName)
	{
		sol::table Command = MakeWaitCommand(ThisState, "key_down");
		Command["key"] = KeyName;
		return Command;
	}));

	Env->set_function("wait_signal", sol::yielding([this](sol::this_state ThisState, const FString& Name)
	{
		sol::table Command = MakeWaitCommand(ThisState, "signal");
		Command["name"] = Name;
		return Command;
	}));

	Env->set_function("signal", [this](const FString& Name)
	{
		InsertPendingSignal(Name);
	});
}

// StartCoroutine은 coroutine을 즉시 resume하지 않는다.
// Lua 이벤트 실행 중 nested resume/yield가 발생하면 stack/lifetime 관리가 복잡해지므로
// 여기서는 FRunningCoroutine을 생성해 큐에 등록만 하고 실제 실행은 Tick()에서만 한다.
bool FLuaCoroutineScheduler::StartCoroutine(const FString& DebugName, sol::protected_function Function)
{
	if (!Owner || !Lua || !Env)
	{
		return false;
	}

	lua_State* MainLuaState = Lua->lua_state();
	FLuaStackGuard MainGuard(MainLuaState, "LuaCoroutineScheduler::StartCoroutine");
	if (!MainLuaState)
	{
		Owner->SetError("StartCoroutine failed: null main lua_State for '" + DebugName + "'.");
		return false;
	}

	if (!Function.lua_state() || !Function.valid())
	{
		Owner->SetError("StartCoroutine failed: invalid or detached Lua function '" + DebugName + "'.");
		return false;
	}

	auto Entry = std::make_unique<FRunningCoroutine>();
	Entry->DebugName = DebugName;
	Entry->Id = NextCoroutineId++;
	Entry->Wait = FWaitCondition{};

	Entry->Thread = sol::thread::create(MainLuaState);
	if (!Entry->Thread.valid())
	{
		Owner->SetError("StartCoroutine failed: invalid sol::thread for '" + DebugName + "'.");
		return false;
	}

	sol::state_view ThreadState = Entry->Thread.state();
	lua_State* ThreadLuaState = ThreadState.lua_state();
	FLuaStackGuard ThreadGuard(ThreadLuaState, "LuaCoroutineScheduler::StartCoroutineThread");

	if (!ThreadLuaState)
	{
		Owner->SetError("StartCoroutine failed: null coroutine lua_State for '" + DebugName + "'.");
		return false;
	}

	const std::string EntryName =
		std::string("__script_coroutine_entry_") + std::to_string(Entry->Id);

	ThreadState[EntryName.c_str()] = Function;

	sol::object ThreadFunctionObject = ThreadState[EntryName.c_str()];
	if (!ThreadFunctionObject.valid() || ThreadFunctionObject.get_type() != sol::type::function)
	{
		ThreadState[EntryName.c_str()] = sol::lua_nil;
		Owner->SetError("StartCoroutine failed: could not move function to coroutine thread.");
		return false;
	}

	Entry->EntryFunction = ThreadFunctionObject.as<sol::protected_function>();
	if (!Entry->EntryFunction.valid())
	{
		ThreadState[EntryName.c_str()] = sol::lua_nil;
		Owner->SetError("StartCoroutine failed: invalid entry function for '" + DebugName + "'.");
		return false;
	}

	sol::reference NoTracebackHandler(ThreadState.lua_state(), sol::lua_nil);
	Entry->Coroutine = sol::coroutine(Entry->EntryFunction, NoTracebackHandler);

	ThreadState[EntryName.c_str()] = sol::lua_nil;

	if (!Entry->Coroutine.valid())
	{
		Owner->SetError("StartCoroutine failed: invalid sol::coroutine for '" + DebugName + "'.");
		return false;
	}

	Enqueue(std::move(Entry));
	return true;
}

// Scheduler Tick은 coroutine resume이 발생하는 유일한 지점이다.
// wait 조건이 끝난 coroutine만 resume하고, yield 결과를 다시 WaitCondition으로 변환한다.
void FLuaCoroutineScheduler::Tick(float DeltaTime, float RawDeltaTime)
{
	ActiveSignals.swap(PendingSignals);
	PendingSignals.clear();

	bTicking = true;

	for (size_t Index = 0; Index < Running.size();)
	{
		FRunningCoroutine* Entry = Running[Index].get();

		if (!Entry || Entry->bCancelRequested)
		{
			Running.erase(Running.begin() + Index);
			continue;
		}

		if (!ShouldResume(*Entry, DeltaTime, RawDeltaTime))
		{
			++Index;
			continue;
		}

		if (!Entry->Thread.lua_state() || !Entry->Thread.valid())
		{
			if (Owner)
			{
				Owner->SetError("Coroutine '" + Entry->DebugName + "' has invalid sol::thread.");
			}
			Running.erase(Running.begin() + Index);
			continue;
		}

		if (!Entry->EntryFunction.lua_state() || !Entry->EntryFunction.valid())
		{
			if (Owner)
			{
				Owner->SetError("Coroutine '" + Entry->DebugName + "' has invalid entry function.");
			}
			Running.erase(Running.begin() + Index);
			continue;
		}

		if (!Entry->Coroutine.lua_state() || !Entry->Coroutine.valid())
		{
			if (Owner)
			{
				Owner->SetError("Coroutine '" + Entry->DebugName + "' has invalid sol::coroutine.");
			}
			Running.erase(Running.begin() + Index);
			continue;
		}

		lua_State* ThreadLuaState = Entry->Thread.state().lua_state();
		lua_State* CoroutineLuaState = Entry->Coroutine.lua_state();
		if (!ThreadLuaState || !CoroutineLuaState)
		{
			if (Owner)
			{
				Owner->SetError("Coroutine '" + Entry->DebugName + "' has null lua_State.");
			}
			Running.erase(Running.begin() + Index);
			continue;
		}

		if (ThreadLuaState != CoroutineLuaState)
		{
			if (Owner)
			{
				Owner->SetError("Coroutine '" + Entry->DebugName + "' has mismatched lua_State.");
			}
			Running.erase(Running.begin() + Index);
			continue;
		}

		if (!Entry->Coroutine.runnable())
		{
			if (Owner)
			{
				Owner->SetError("Coroutine '" + Entry->DebugName + "' is not runnable.");
			}
			Running.erase(Running.begin() + Index);
			continue;
		}

		bool bRemove = false;
		{
			FLuaStackGuard ResumeGuard(ThreadLuaState, "LuaCoroutineScheduler::Resume");
			sol::protected_function_result Result = Entry->Coroutine();
			const sol::call_status Status = Result.status();

			if (Status == sol::call_status::yielded)
			{
				if (!ApplyYieldResult(*Entry, Result))
				{
					bRemove = true;
				}
			}
			else if (!Result.valid())
			{
				HandleCoroutineError(*Entry, std::move(Result));
				bRemove = true;
			}
			else
			{
				bRemove = true;
			}
		}

		if (bRemove)
		{
			Running.erase(Running.begin() + Index);
			continue;
		}

		++Index;
	}

	bTicking = false;
	FlushPending();

	ActiveSignals.clear();
}

void FLuaCoroutineScheduler::StopAll()
{
	Running.clear();
	Pending.clear();
	ActiveSignals.clear();
	PendingSignals.clear();
	bTicking = false;
}

void FLuaCoroutineScheduler::InsertPendingSignal(const FString& Name)
{
	if (Name.empty())
	{
		return;
	}

	PendingSignals.insert(Name);
}

void FLuaCoroutineScheduler::Enqueue(std::unique_ptr<FRunningCoroutine> Entry)
{
	if (!Entry)
	{
		return;
	}

	if (bTicking)
	{
		Pending.push_back(std::move(Entry));
		return;
	}

	Running.push_back(std::move(Entry));
}

void FLuaCoroutineScheduler::FlushPending()
{
	if (Pending.empty())
	{
		return;
	}

	for (auto& Entry : Pending)
	{
		Running.push_back(std::move(Entry));
	}

	Pending.clear();
}

bool FLuaCoroutineScheduler::ShouldResume(FRunningCoroutine& Entry, float DeltaTime, float RawDeltaTime)
{
	switch (Entry.Wait.Kind)
	{
	case EWaitKind::None:
		return true;

	case EWaitKind::Time:
		Entry.Wait.TimeRemaining -= DeltaTime;
		return Entry.Wait.TimeRemaining <= 0.0f;

	case EWaitKind::RealTime:
		Entry.Wait.TimeRemaining -= RawDeltaTime;
		return Entry.Wait.TimeRemaining <= 0.0f;

	case EWaitKind::Frames:
		--Entry.Wait.FramesRemaining;
		return Entry.Wait.FramesRemaining <= 0;

	case EWaitKind::MoveDone:
		return OwnerProxy ? OwnerProxy->IsMoveDone() : true;

	case EWaitKind::KeyDown:
	{
		int VirtualKey = 0;
		if (!TryParseVirtualKey(Entry.Wait.KeyName, VirtualKey))
		{
			return true;
		}

		FInputManager& Input = FInputManager::Get();
		if (Input.IsGuiUsingKeyboard())
		{
			return false;
		}

		return Input.IsKeyPressed(VirtualKey);
	}

	case EWaitKind::Signal:
		return ActiveSignals.find(Entry.Wait.Name) != ActiveSignals.end();

	default:
		return true;
	}
}

bool FLuaCoroutineScheduler::ApplyYieldResult(FRunningCoroutine& Entry, sol::protected_function_result& Result)
{
	FLuaStackGuard Guard(Result.lua_state(), "LuaCoroutineScheduler::ApplyYieldResult");

	Entry.Wait = FWaitCondition{};

	if (Result.return_count() <= 0)
	{
		Entry.Wait.Kind = EWaitKind::None;
		return true;
	}

	const sol::type YieldType = Result.get_type(0);

	if (YieldType == sol::type::number)
	{
		Entry.Wait.Kind = EWaitKind::Time;
		Entry.Wait.TimeRemaining = (std::max)(0.0f, Result.get<float>());
		return true;
	}

	if (YieldType != sol::type::table)
	{
		if (Owner)
		{
			Owner->SetError("Coroutine '" + Entry.DebugName + "' yielded unsupported value.");
		}
		return false;
	}

	sol::table Command = Result.get<sol::table>();
	const FString Type = Command["type"].get_or(FString());

	if (Type == "time")
	{
		Entry.Wait.Kind = EWaitKind::Time;
		Entry.Wait.TimeRemaining = (std::max)(0.0f, Command["seconds"].get_or(0.0f));
		return true;
	}

	if (Type == "real_time")
	{
		Entry.Wait.Kind = EWaitKind::RealTime;
		Entry.Wait.TimeRemaining = (std::max)(0.0f, Command["seconds"].get_or(0.0f));
		return true;
	}

	if (Type == "frames")
	{
		Entry.Wait.Kind = EWaitKind::Frames;
		Entry.Wait.FramesRemaining = (std::max)(0, Command["frames"].get_or(0));
		return true;
	}

	if (Type == "move_done")
	{
		Entry.Wait.Kind = EWaitKind::MoveDone;
		return true;
	}

	if (Type == "key_down")
	{
		Entry.Wait.Kind = EWaitKind::KeyDown;
		Entry.Wait.KeyName = Command["key"].get_or(FString());
		return true;
	}

	if (Type == "signal")
	{
		Entry.Wait.Kind = EWaitKind::Signal;
		Entry.Wait.Name = Command["name"].get_or(FString());
		return true;
	}

	if (Owner)
	{
		Owner->SetError("Coroutine '" + Entry.DebugName + "' yielded unknown wait command: " + Type);
	}

	return false;
}

bool FLuaCoroutineScheduler::HandleCoroutineError(
	FRunningCoroutine& Entry,
	sol::protected_function_result&& Result)
{
	if (Result.valid())
	{
		return true;
	}

	if (!Owner)
	{
		return false;
	}

	const sol::optional<sol::error> MaybeError =
		Result.get<sol::optional<sol::error>>();

	const FString ErrorMessage =
		MaybeError ? MaybeError->what() : FString("Unknown Lua coroutine error.");

	Owner->SetError("Coroutine(" + Entry.DebugName + "): " + ErrorMessage);
	return false;
}

// wait/wait_real/wait_frames가 yield할 command table은 현재 실행 중인 Lua thread 기준으로 만든다.
// 메인 Lua state에서 table을 만들면 coroutine thread stack과 섞일 수 있으므로 sol::this_state를 사용한다.
sol::table FLuaCoroutineScheduler::MakeWaitCommand(sol::this_state ThisState, const FString& Type)
{
	sol::state_view State(ThisState);
	FLuaStackGuard Guard(State.lua_state(), "LuaCoroutineScheduler::MakeWaitCommand");
	sol::table Command = State.create_table();
	Command["type"] = Type;
	return Command;
}
