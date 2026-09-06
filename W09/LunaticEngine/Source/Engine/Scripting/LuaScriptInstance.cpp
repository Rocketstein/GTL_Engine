#include "Scripting/LuaScriptInstance.h"

#include "Component/ScriptComponent.h"
#include "Audio/AudioManager.h"
#include "Core/Log.h"
#include "Core/AsciiUtils.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Runtime/GameEngine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/AActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Platform/Paths.h"
#include "Platform/ScriptPaths.h"
#include "SimpleJSON/json.hpp"
#include "Engine/Viewport/GameViewportClient.h"
#include "GameFramework/GamePlayStatics.h"
#include "Scripting/LuaCoroutineScheduler.h"

// Sol.hpp에 있는 Check 매크로 겹침 방지 목적 제거
#pragma region SolInclude
#ifdef check
#pragma push_macro("check")
#undef check
#define LUA_RESTORE_CHECK_MACRO
#endif

#ifdef checkf
#pragma push_macro("checkf")
#undef checkf
#define LUA_RESTORE_CHECKF_MACRO
#endif

#include "sol/sol.hpp"

// Sol.hpp include 완료 후 복구
#ifdef LUA_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef LUA_RESTORE_CHECKF_MACRO
#endif

#ifdef LUA_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef LUA_RESTORE_CHECK_MACRO
#endif
#pragma endregion

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include "Input/InputAction.h"
#include "sol/types.hpp"

namespace
{
	bool TryParseVirtualKey(const FString& KeyName, int& OutVirtualKey);
}

// ====================================================================
// Lua Script 실행 상태 컨테이너
// - FLuaScriptInstance.h에 sol2, Lua, 코루틴, 환경 같은 세부 구현을 노출X
// ====================================================================
struct FLuaScriptInstance::FInstanceImpl
{
	UScriptComponent* OwnerComponent = nullptr;
	FString ScriptPath;

	// 로딩/에러 상태
	bool bLoaded = false;
	bool bHasError = false;
	FString LastError;

	FLuaActorProxy OwnerProxy;
	TArray<FLuaActorProxy> ManagedProxies;
	float LastDeltaTime = 0.0f;
	float ElapsedTime = 0.0f;

	sol::environment Env;
	// Instance에서 default로 캐싱할 목록
	sol::protected_function FnBeginPlay;
	sol::protected_function FnTick;
	sol::protected_function FnEndPlay;

	FLuaCoroutineScheduler CoroutineScheduler;

	void ClearFunctionCache()
	{
		// 스크립트가 다시 로드되면 이전 environment에서 꺼낸 함수 캐시를 버려야 한다.
		FnBeginPlay = sol::protected_function();
		FnTick = sol::protected_function();
		FnEndPlay = sol::protected_function();
	}

	void ResetEnvironment(sol::state& Lua)
	{
		// 매 스크립트 인스턴스는 전역 Lua VM 위에 독립 environment를 만든다.
		Env = sol::environment(Lua, sol::create, Lua.globals());
		ClearFunctionCache();

		// 중요:
		// require_env로 로드된 모듈 안에서 _G.xxx = ... 를 해도
		// Runtime 전역이 아니라 이 Instance Env에 기록되게 만든다.
		Env["_G"] = Env;

		// require_env 전용 캐시.
		// Lua 기본 package.loaded는 Runtime 전체 공유라서 Instance 격리에 맞지 않는다.
		Env["__env_loaded_modules"] = Lua.create_table();

		ClearFunctionCache();
	}

	FLuaActorProxy TrackProxy(const FLuaActorProxy& Proxy)
	{
		// spawn_actor/find_actor가 반환한 Proxy도 MoveToActor 같은 Lua task를 수행할 수 있다.
		// Lua에는 Proxy 값이 복사되어 전달되므로, C++ 인스턴스가 같은 공유 TaskState를 가진 Proxy를 보관하고 Tick해야 작업이 계속 진행된다.
		if (Proxy.IsValid())
		{
			ManagedProxies.push_back(Proxy);
		}

		return Proxy;
	}

	void TickLuaProxyTasks(float DeltaTime)
	{
		// OwnerProxy는 obj로 노출되는 기본 허브이며 항상 먼저 갱신한다.
		OwnerProxy.TickLuaTasks(DeltaTime);

		for (size_t Index = 0; Index < ManagedProxies.size();)
		{
			FLuaActorProxy& Proxy = ManagedProxies[Index];
			if (!Proxy.IsValid())
			{
				// Lua가 Actor를 소유하지 않으므로 World가 Actor를 파괴하면 Proxy 목록에서도 정리한다.
				ManagedProxies.erase(ManagedProxies.begin() + Index);
				continue;
			}

			Proxy.TickLuaTasks(DeltaTime);
			++Index;
		}
	}

	bool HandleProtectedResult(FLuaScriptInstance* OwnerInstance, const FString& Context, sol::protected_function_result&& Result)
	{
		// 일반 BeginPlay/Tick/EndPlay 호출 경로에서는 yield를 허용하지 않는다.
		// lifecycle 함수가 yield되면 호출자가 재개 시점을 알 수 없어 엔진 생명주기와 Lua 실행 상태가 어긋나므로,
		// 대기가 필요하면 반드시 StartCoroutine/start_coroutine으로 시작한 coroutine 안에서 wait 계열 함수를 호출해야 한다.
		if (Result.status() == sol::call_status::yielded)
		{
			OwnerInstance->SetError(Context + " unexpectedly yielded. Use StartCoroutine(...) for wait/yield APIs.");
			return false;
		}

		if (Result.valid())
		{
			return true;
		}

		const sol::optional<sol::error> MaybeError = Result.get<sol::optional<sol::error>>();
		const FString ErrorMessage = MaybeError ? MaybeError->what() : FString("Unknown Lua error.");
		OwnerInstance->SetError(Context + ": " + ErrorMessage);
		return false;
	}

	template <typename... TArgs>
	bool CallFunction(FLuaScriptInstance* OwnerInstance, const FString& FunctionName, sol::protected_function& Function, TArgs&&... Args)
	{
		// 함수가 정의되지 않은 경우는 스크립트 에러로 보지 않고 조용히 통과시킨다.
		if (!Function.valid())
		{
			return true;
		}

		sol::protected_function_result Result = Function(std::forward<TArgs>(Args)...);
		return HandleProtectedResult(OwnerInstance, FunctionName, std::move(Result));
	}
};

// TODO: Input System 생기면 지워야함.
namespace
{
	// 키매핑
	bool TryParseVirtualKey(const FString& KeyName, int& OutVirtualKey)
	{
		// 입력 바인딩은 단순 문자열 기반 인터페이스를 Lua에 노출하고,
		// 실제 Win32 virtual key 변환은 C++ 쪽에서 처리한다.
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

	void CacheFunction(sol::environment& Env, const char* Name, sol::protected_function& OutFunction)
	{
		// BeginPlay/Tick/EndPlay 같은 선택적 엔트리 포인트를 미리 캐시해
		// 매 프레임 문자열 lookup을 반복하지 않게 한다.
		sol::object Object = Env[Name];
		if (Object.valid() && Object.get_type() == sol::type::function)
		{
			OutFunction = Object.as<sol::protected_function>();
			return;
		}

		OutFunction = sol::protected_function();
	}

	std::filesystem::path ResolveDataFilePath(const FString& Path)
	{
		const std::filesystem::path InputPath(FPaths::ToWide(Path));
		if (InputPath.is_absolute())
		{
			return InputPath.lexically_normal();
		}

		// Root relative (ProjectDir/Asset/Content/...)
		std::filesystem::path FullPath = std::filesystem::path(FPaths::RootDir()) / InputPath;
		if (std::filesystem::exists(FullPath))
		{
			return FullPath.lexically_normal();
		}

		// Content relative fallback
		std::filesystem::path ContentPath = std::filesystem::path(FPaths::ProjectContentDir()) / InputPath;
		if (std::filesystem::exists(ContentPath))
		{
			return ContentPath.lexically_normal();
		}

		// Asset relative fallback
		std::filesystem::path AssetPath = std::filesystem::path(FPaths::AssetDir()) / InputPath;
		if (std::filesystem::exists(AssetPath))
		{
			return AssetPath.lexically_normal();
		}

		return FullPath.lexically_normal();
	}

	FString MakeRequireEnvScriptPath(const FString& ModuleName)
	{
		FString ScriptPath = ModuleName;

		// require_env("Game.HitEffect") -> Scripts/Game/HitEffect.lua
		std::replace(ScriptPath.begin(), ScriptPath.end(), '.', '/');
		std::replace(ScriptPath.begin(), ScriptPath.end(), '\\', '/');

		if (ScriptPath.size() < 4 || ScriptPath.substr(ScriptPath.size() - 4) != ".lua")
		{
			ScriptPath += ".lua";
		}

		return FScriptPaths::NormalizeScriptPath(ScriptPath);
	}

	sol::object MakeLuaObjectFromJson(sol::state_view Lua, const json::JSON& Value)
	{
		using JsonClass = json::JSON::Class;

		switch (Value.JSONType())
		{
		case JsonClass::Null:
			return sol::make_object(Lua, sol::lua_nil);
		case JsonClass::Boolean:
			return sol::make_object(Lua, Value.ToBool());
		case JsonClass::Integral:
			return sol::make_object(Lua, Value.ToInt());
		case JsonClass::Floating:
			return sol::make_object(Lua, Value.ToFloat());
		case JsonClass::String:
			return sol::make_object(Lua, Value.ToString());
		case JsonClass::Array:
		{
			sol::table Result = Lua.create_table();
			int LuaIndex = 1;
			for (auto& Entry : Value.ArrayRange())
			{
				Result[LuaIndex++] = MakeLuaObjectFromJson(Lua, Entry);
			}
			return sol::make_object(Lua, Result);
		}
		case JsonClass::Object:
		{
			sol::table Result = Lua.create_table();
			for (auto& Pair : Value.ObjectRange())
			{
				Result[Pair.first] = MakeLuaObjectFromJson(Lua, Pair.second);
			}
			return sol::make_object(Lua, Result);
		}
		default:
			return sol::make_object(Lua, sol::lua_nil);
		}
	}

	json::JSON MakeJsonFromLuaObject(const sol::object& Value)
	{
		switch (Value.get_type())
		{
		case sol::type::lua_nil:
		case sol::type::none:
			return json::JSON();
		case sol::type::boolean:
			return json::JSON(Value.as<bool>());
		case sol::type::number:
		{
			const double Number = Value.as<double>();
			const double Rounded = std::round(Number);
			if (std::abs(Number - Rounded) <= 0.000001)
			{
				return json::JSON(static_cast<long>(Rounded));
			}
			return json::JSON(Number);
		}
		case sol::type::string:
			return json::JSON(Value.as<FString>());
		case sol::type::table:
		{
			sol::table Table = Value.as<sol::table>();
			bool bIsArray = true;
			int32 MaxIndex = 0;
			int32 EntryCount = 0;

			for (auto& Pair : Table)
			{
				const sol::object Key = Pair.first.as<sol::object>();
				if (Key.get_type() != sol::type::number)
				{
					bIsArray = false;
					break;
				}

				const double NumericKey = Key.as<double>();
				const int32 ArrayIndex = static_cast<int32>(NumericKey);
				if (NumericKey != static_cast<double>(ArrayIndex) || ArrayIndex <= 0)
				{
					bIsArray = false;
					break;
				}

				MaxIndex = (std::max)(MaxIndex, ArrayIndex);
				++EntryCount;
			}

			if (bIsArray && EntryCount == MaxIndex)
			{
				json::JSON Result = json::Array();
				for (int32 Index = 1; Index <= MaxIndex; ++Index)
				{
					Result.append(MakeJsonFromLuaObject(Table[Index].get<sol::object>()));
				}
				return Result;
			}

			json::JSON Result = json::Object();
			for (auto& Pair : Table)
			{
				const sol::object Key = Pair.first.as<sol::object>();
				FString KeyString;
				if (Key.get_type() == sol::type::string)
				{
					KeyString = Key.as<FString>();
				}
				else if (Key.get_type() == sol::type::number)
				{
					KeyString = std::to_string(static_cast<int32>(Key.as<double>()));
				}
				else
				{
					continue;
				}

				Result[KeyString] = MakeJsonFromLuaObject(Pair.second.as<sol::object>());
			}
			return Result;
		}
		default:
			return json::JSON();
		}
	}

	sol::object FindLuaObjectByPath(sol::environment& Env, const FString& Path)
	{
		// Env["EnemyAI.start"]는 Lua table 내부 함수를 찾지 못한다.
		// dot path를 세그먼트별로 따라가야 EnemyAI.start 같은 네임스페이스형 coroutine entry를 지원할 수 있다.
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

	AActor* SpawnActorByClassName(UWorld* World, const FString& ClassName)
	{
		if (!World || ClassName.empty())
		{
			return nullptr;
		}

		// 현재 엔진의 범용 생성 경로는 FObjectFactory + World::AddActor 조합이다.
		// World가 AddActor를 호출해야 BeginPlay/Octree/Picking 등 World 소유 생명주기 등록이 함께 일어난다.
		UObject* NewObject = FObjectFactory::Get().Create(ClassName, World);
		AActor* NewActor = Cast<AActor>(NewObject);
		if (!NewActor)
		{
			if (NewObject)
			{
				UObjectManager::Get().DestroyObject(NewObject);
			}
			return nullptr;
		}

		if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(NewActor))
		{
			// AStaticMeshActor는 생성자에서 RootComponent를 만들지 않으므로,
			// World::AddActor가 PIE에서 BeginPlay를 즉시 호출할 수 있기 전에 기본 컴포넌트를 먼저 만든다.
			// 이 순서를 지켜야 BeginPlay 시점에 스크립트와 렌더링이 유효한 RootComponent를 볼 수 있다.
			StaticMeshActor->InitDefaultComponents();
		}

		World->AddActor(NewActor);
		return NewActor;
	}
}

FLuaScriptInstance::FLuaScriptInstance()
	: Impl(std::make_unique<FInstanceImpl>())
{
}

FLuaScriptInstance::~FLuaScriptInstance()
{
	Shutdown();
}

bool FLuaScriptInstance::Initialize(UScriptComponent* InOwnerComponent)
{
	// 이전 owner/state가 남아 있다면 먼저 완전히 끊고 다시 시작한다.
	Shutdown();

	if (!InOwnerComponent)
	{
		SetError("LuaScriptInstance requires a valid owner component.");
		return false;
	}

	if (!FLuaScriptRuntime::Get().bIsInitialized())
	{
		SetError(FLuaScriptRuntime::Get().GetLastError());
		return false;
	}

	Impl->OwnerComponent = InOwnerComponent;
	Impl->OwnerProxy = MakeActorProxy(GetOwnerActor());
	Impl->ResetEnvironment(FLuaScriptRuntime::Get().GetLuaState());

	// Initialize는 environment 골격과 기본 바인딩만 준비하고,
	// 실제 스크립트 파일 실행은 LoadFromFile에서 한다.
	ClearError();
	BindOwnerObject();
	BindCoroutineFunctions();
	BindInputFunctions();
	BindDebugTimeFunctions();
	BindSoundFunctions();
	BindWorldFunctions();
	BindDataFunctions();
	return true;
}

void FLuaScriptInstance::Shutdown()
{
	if (!Impl)
	{
		return;
	}

	// Owner 참조, environment, coroutine 상태를 모두 끊어
	// 다음 Initialize/LoadFromFile이 완전히 새 상태에서 시작하게 만든다.
	StopAllCoroutines();
	Impl->ClearFunctionCache();
	Impl->Env = sol::environment();
	Impl->OwnerProxy = FLuaActorProxy();
	Impl->ManagedProxies.clear();
	Impl->OwnerComponent = nullptr;
	Impl->ScriptPath.clear();
	Impl->bLoaded = false;
	ClearError();
}

bool FLuaScriptInstance::LoadFromFile(const FString& InScriptPath)
{
	if (!Impl->OwnerComponent)
	{
		SetError("LuaScriptInstance is not initialized.");
		return false;
	}

	if (!FLuaScriptRuntime::Get().bIsInitialized())
	{
		SetError(FLuaScriptRuntime::Get().GetLastError());
		return false;
	}

	ClearError();
	StopAllCoroutines();
	Impl->ManagedProxies.clear();
	Impl->bLoaded = false;

	// Instance는 더 이상 Scripts/ prefix 규칙이나 파일 위치 정책을 직접 알지 않는다.
	// 저장 문자열은 FScriptPaths가 canonical한 내부 표기로 맞춘다.
	Impl->ScriptPath = FScriptPaths::NormalizeScriptPath(InScriptPath);

	// reload 시에도 이전 global, 함수 캐시, coroutine이 남지 않도록
	// 매번 environment를 처음부터 다시 구성한다.
	Impl->OwnerProxy = MakeActorProxy(GetOwnerActor());
	Impl->ResetEnvironment(FLuaScriptRuntime::Get().GetLuaState());
	BindOwnerObject();
	BindCoroutineFunctions();
	BindInputFunctions();
	BindDebugTimeFunctions();
	BindSoundFunctions();
	BindWorldFunctions();
	BindDataFunctions();

	FString ScriptSource;
	FString FileReadError;
	if (!FScriptPaths::ReadScriptFile(Impl->ScriptPath, ScriptSource, FileReadError))
	{
		SetError(FileReadError);
		return false;
	}

	const std::filesystem::path ResolvedPath = FScriptPaths::ResolveScriptPath(Impl->ScriptPath);

	// Lua chunk name은 디버깅과 에러 메시지에서 실제 파일 위치가 보이는 쪽이 유리해서
	// 정규화 상대 경로 대신 resolve된 절대 경로를 사용한다.
	const FString ChunkName = FPaths::ToUtf8(ResolvedPath.generic_wstring());
	sol::protected_function_result LoadResult =
		FLuaScriptRuntime::Get().GetLuaState().safe_script(
			ScriptSource, 
			Impl->Env, 
			// 로드 실패 시 예외를 삼키지 말고 protected result로 받아서
			// component 쪽 에러 UI에 그대로 전달한다.
			sol::script_pass_on_error,
			ChunkName, 
			sol::load_mode::text);

	if (!Impl->HandleProtectedResult(this, "LoadScript(" + Impl->ScriptPath + ")", std::move(LoadResult)))
	{
		StopAllCoroutines();
		return false;
	}

	CacheFunction(Impl->Env, "BeginPlay", Impl->FnBeginPlay);
	CacheFunction(Impl->Env, "Tick", Impl->FnTick);
	CacheFunction(Impl->Env, "EndPlay", Impl->FnEndPlay);

	// 엔트리 포인트 캐시가 끝난 시점부터 이 인스턴스를 로드 완료로 본다.
	Impl->bLoaded = true;
	return true;
}

bool FLuaScriptInstance::Reload()
{
	if (Impl->ScriptPath.empty())
	{
		SetError("ReloadScript failed: ScriptPath is empty.");
		return false;
	}

	StopAllCoroutines();
	// TODO: 필요한 경우 hot-reload에서 Lua environment와 coroutine state를 보존하는 경로를 별도로 설계한다.
	// 지금은 reload 중 이전 Env와 Lua stack을 유지하면 C++ Proxy/Actor 생명주기와 어긋날 수 있어 안전하게 모두 정리한다.
	return LoadFromFile(Impl->ScriptPath);
}

bool FLuaScriptInstance::IsLoaded() const
{
	return Impl && Impl->bLoaded;
}

void FLuaScriptInstance::SetScriptPath(const FString& InScriptPath)
{
	if (!Impl)
	{
		Impl = std::make_unique<FInstanceImpl>();
	}

	Impl->ScriptPath = FScriptPaths::NormalizeScriptPath(InScriptPath);
}

const FString& FLuaScriptInstance::GetScriptPath() const
{
	static const FString EmptyPath;
	return Impl ? Impl->ScriptPath : EmptyPath;
}

bool FLuaScriptInstance::CallBeginPlay()
{
	return Impl->CallFunction(this, "BeginPlay", Impl->FnBeginPlay);
}

bool FLuaScriptInstance::CallTick(float DeltaTime)
{
	if (Impl)
	{
		// delta_time() 전역 함수는 현재 프레임 값을 반환해야 하므로 Lua Tick 호출 전에 최신 값을 저장한다.
		Impl->LastDeltaTime = DeltaTime;
		Impl->ElapsedTime += (std::max)(0.0f, DeltaTime);
	}

	return Impl->CallFunction(this, "Tick", Impl->FnTick, DeltaTime);
}

bool FLuaScriptInstance::CallEndPlay()
{
	return Impl->CallFunction(this, "EndPlay", Impl->FnEndPlay);
}

#pragma region Event+Input 호출 함수
bool FLuaScriptInstance::CallLuaFunction(const FString& FunctionName)
{
	if (!Impl || !Impl->bLoaded)
	{
		return false;
	}

	sol::object FunctionObject = Impl->Env[FunctionName];

	// Lua에 해당 함수가 없는 경우 true 처리(필요없어서 구현하지 않은 경우 고려)
	// TODO: 필수 함수가 없는 경우를 따로 고려하고 싶다면(BeginPlay, Tick, EndPlay) 이 부분만 false로 하는 기능 추가
	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		return true;
	}
	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	sol::protected_function_result Result = Function();
	return Impl->HandleProtectedResult(this, FunctionName, std::move(Result));
}

bool FLuaScriptInstance::CallLuaOverlapEvent(const FString& FunctionName, AActor* OtherActor, UActorComponent* OtherComponent, UActorComponent* SelfComponent)
{
	if (!Impl || !Impl->bLoaded)
	{
		return false;
	}

	sol::object FunctionObject = Impl->Env[FunctionName];

	// 필수 함수가 아니기 때문에 굳이 false를 둘 필요 없음.
	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		return true;
	}

	// Lua ActorProxy 생성
	FLuaActorProxy OtherActorProxy = MakeActorProxy(OtherActor);

	// Lua Component Proxy 생성
	FLuaComponentProxy OtherComponentProxy = MakeComponentProxy(OtherComponent);
	FLuaComponentProxy SelfComponentProxy = MakeComponentProxy(SelfComponent);

	// 스크립트 실행 및 오류 체크
	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	sol::protected_function_result Result =
		Function(OtherActorProxy, OtherComponentProxy, SelfComponentProxy);
	return Impl->HandleProtectedResult(this, FunctionName, std::move(Result));
}

bool FLuaScriptInstance::CallLuaHitEvent(const FString& FunctionName, AActor* OtherActor, UActorComponent* OtherComponent, UActorComponent* SelfComponent, const FVector& ImpactLocation, const FVector& ImpactNormal)
{
	if (!Impl || !Impl->bLoaded)
	{
		return false;
	}

	sol::object FunctionObject = Impl->Env[FunctionName];

	// 필수 함수가 아니기 때문에 굳이 false를 둘 필요 없음.
	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		return true;
	}

	// Lua Actor Proxy 생성
	FLuaActorProxy OtherActorProxy = MakeActorProxy(OtherActor);

	// Lua Component proxy 생성
	FLuaComponentProxy OtherComponentProxy = MakeComponentProxy(OtherComponent);
	FLuaComponentProxy SelfComponentProxy = MakeComponentProxy(SelfComponent);

	// 함수 호출 및 오류 검사
	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	sol::protected_function_result Result =
		Function(
			OtherActorProxy,
			OtherComponentProxy,
			SelfComponentProxy,
			ImpactLocation,
			ImpactNormal);
	return Impl->HandleProtectedResult(this, FunctionName, std::move(Result));
}

bool FLuaScriptInstance::CallLuaInputAction(const FString& FunctionName, const FString& ActionName, const FInputActionValue& Value)
{
	if (!Impl || !Impl->bLoaded)
	{
		return false;
	}

	sol::object FunctionObject = Impl->Env[FunctionName];

	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		return true;
	}

	// TODO: InputMapping에서 발생한 ActionValue를 ScriptComponent까지 전달하는 정식 경로가 필요합니다.
	// 현재는 Lua 프로토타입용 최소 호출 함수만 제공합니다.
	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	sol::protected_function_result Result =
		Function(ActionName, Value.GetVector(), Value.Get());
	return Impl->HandleProtectedResult(this, FunctionName, std::move(Result));
}
#pragma endregion

bool FLuaScriptInstance::StartCoroutine(const FString& FunctionName)
{
	if (!Impl || !Impl->bLoaded)
	{
		SetError("StartCoroutine failed: script is not loaded.");
		return false;
	}

	sol::object FunctionObject = FindLuaObjectByPath(Impl->Env, FunctionName);
	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		SetError("StartCoroutine failed: missing Lua function '" + FunctionName + "'.");
		return false;
	}

	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	return Impl->CoroutineScheduler.StartCoroutine(FunctionName, std::move(Function));
}

bool FLuaScriptInstance::StartCoroutineFunction(const FString& DebugName, sol::protected_function Function)
{
	if (!Impl || !Impl->bLoaded)
	{
		SetError("StartCoroutine failed: script is not loaded.");
		return false;
	}

	return Impl->CoroutineScheduler.StartCoroutine(DebugName, std::move(Function));
}

void FLuaScriptInstance::TickCoroutines(float DeltaTime)
{
	if (!Impl)
	{
		return;
	}

	// Raw DeltaTime 가져오기
	float RawDeltaTime = DeltaTime;

	if (AActor* OwnerActor = GetOwnerActor())
	{
		if (UWorld* World = OwnerActor->GetWorld())
		{
			RawDeltaTime = World->GetRawDeltaTime();
		}
	}

	Impl->LastDeltaTime = DeltaTime;
	Impl->TickLuaProxyTasks(DeltaTime);
	Impl->CoroutineScheduler.Tick(DeltaTime, RawDeltaTime);
}

void FLuaScriptInstance::StopAllCoroutines()
{
	if (!Impl)
	{
		return;
	}

	Impl->CoroutineScheduler.StopAll();
}

bool FLuaScriptInstance::HasError() const
{
	return Impl && Impl->bHasError;
}

const FString& FLuaScriptInstance::GetLastError() const
{
	static const FString EmptyError;
	return Impl ? Impl->LastError : EmptyError;
}

void FLuaScriptInstance::ClearError()
{
	if (!Impl)
	{
		return;
	}

	Impl->bHasError = false;
	Impl->LastError.clear();
}

UScriptComponent* FLuaScriptInstance::GetOwnerComponent() const
{
	return (Impl && Impl->OwnerComponent && IsAliveObject(Impl->OwnerComponent)) ? Impl->OwnerComponent : nullptr;
}

AActor* FLuaScriptInstance::GetOwnerActor() const
{
	UScriptComponent* OwnerComponent = GetOwnerComponent();
	return OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
}

// Bind 관련 함수 (추가할 일 없으면 열지 말 것, bind 짬통)
void FLuaScriptInstance::RegisterEnvFunction(const FString& Name, std::function<void(FLuaActorProxy)> Func) {
	Impl->Env.set_function(Name, Func);
}

void FLuaScriptInstance::BindOwnerObject()
{
	if (!Impl)
	{
		return;
	}

	// Lua 스크립트에서는 obj를 통해 owner actor 기능에 접근한다.
	Impl->OwnerProxy = MakeActorProxy(GetOwnerActor());
	Impl->Env["obj"] = std::ref(Impl->OwnerProxy);
}

void FLuaScriptInstance::BindCoroutineFunctions()
{
	if (!Impl)
	{
		return;
	}

	sol::state& Lua = FLuaScriptRuntime::Get().GetLuaState();
	Impl->CoroutineScheduler.Initialize(this, &Lua, &Impl->Env, &Impl->OwnerProxy);
	Impl->CoroutineScheduler.BindToEnvironment();
}

void FLuaScriptInstance::BindInputFunctions()
{
	if (!Impl)
	{
		return;
	}

	// 입력 바인딩은 문자열 기반 API로 노출해서 Lua 스크립트가 엔진 키코드 상수를 직접 알 필요 없게 만든다.
	// ImGui가 키보드를 캡처 중이면(text input 등) 게임 입력을 받지 않는다 — PIE에서 입력 분리.
	auto GetKey = [](const FString& KeyName)
	{
		int VirtualKey = 0;
		if (!TryParseVirtualKey(KeyName, VirtualKey))
		{
			return false;
		}
		FInputManager& Input = FInputManager::Get();
		if (Input.IsGuiUsingKeyboard()) return false;
		return Input.IsKeyDown(VirtualKey);
	};

	auto GetKeyDown = [](const FString& KeyName)
	{
		int VirtualKey = 0;
		if (!TryParseVirtualKey(KeyName, VirtualKey))
		{
			return false;
		}
		FInputManager& Input = FInputManager::Get();
		if (Input.IsGuiUsingKeyboard()) return false;
		return Input.IsKeyPressed(VirtualKey);
	};

	auto GetKeyUp = [](const FString& KeyName)
	{
		int VirtualKey = 0;
		if (!TryParseVirtualKey(KeyName, VirtualKey)) return false;
		FInputManager& Input = FInputManager::Get();
		if (Input.IsGuiUsingKeyboard()) return false;
		return Input.IsKeyReleased(VirtualKey);
	};

	Impl->Env.set_function("GetKey", GetKey);
	Impl->Env.set_function("GetKeyDown", GetKeyDown);
	Impl->Env.set_function("GetKeyUp", GetKeyUp);

	sol::table InputTable = FLuaScriptRuntime::Get().GetLuaState().create_table();
	InputTable.set_function("GetKey", GetKey);
	InputTable.set_function("GetKeyDown", GetKeyDown);
	InputTable.set_function("GetKeyUp", GetKeyUp);
	Impl->Env["Input"] = InputTable;

	// Mouse Delta & Wheel
	Impl->Env.set_function("GetMouseDeltaX", []() { return FInputManager::Get().GetMouseDeltaX(); });
	Impl->Env.set_function("GetMouseDeltaY", []() { return FInputManager::Get().GetMouseDeltaY(); });
	Impl->Env.set_function("GetMouseWheel", []() { return FInputManager::Get().GetMouseWheelDelta(); });
	Impl->Env.set_function("MouseMoved", []() { return FInputManager::Get().MouseMoved(); });

	// Dragging
	Impl->Env.set_function("IsDragging", [](const FString& ButtonName)
	{
		int VirtualKey = 0;
		if (TryParseVirtualKey(ButtonName, VirtualKey))
			return FInputManager::Get().IsDragging(VirtualKey);
		return false;
	});

	Impl->Env.set_function("GetDragDeltaX", [](const FString& ButtonName)
	{
		int VirtualKey = 0;
		if (TryParseVirtualKey(ButtonName, VirtualKey))
			return (float)FInputManager::Get().GetDragDelta(VirtualKey).x;
		return 0.0f;
	});

	Impl->Env.set_function("GetDragDeltaY", [](const FString& ButtonName)
	{
		int VirtualKey = 0;
		if (TryParseVirtualKey(ButtonName, VirtualKey))
			return (float)FInputManager::Get().GetDragDelta(VirtualKey).y;
		return 0.0f;
	});

	Impl->Env.set_function("GetDragDistance", [](const FString& ButtonName)
	{
		int VirtualKey = 0;
		if (TryParseVirtualKey(ButtonName, VirtualKey))
			return FInputManager::Get().GetDragDistance(VirtualKey);
		return 0.0f;
	});
}

void FLuaScriptInstance::BindDebugTimeFunctions()
{
	if (!Impl)
	{
		return;
	}

	auto LogLuaMessage = [](ELogLevel Level, const char* Prefix, sol::variadic_args Args)
		{
			auto LuaArgToString = [](const sol::object& Arg) -> FString
				{
					switch (Arg.get_type())
					{
					case sol::type::lua_nil:
						return "nil";
					case sol::type::boolean:
						return Arg.as<bool>() ? "true" : "false";
					case sol::type::number:
						return std::to_string(Arg.as<double>());
					case sol::type::string:
						return Arg.as<FString>();
					case sol::type::function:
						return "<function>";
					case sol::type::table:
						return "<table>";
					case sol::type::userdata:
						return "<userdata>";
					case sol::type::thread:
						return "<thread>";
					default:
						return "<unknown>";
					}
				};

			FString Message;
			for (auto Arg : Args)
			{
				if (!Message.empty())
				{
					Message += " ";
				}

				Message += LuaArgToString(Arg);
			}

			FLogManager::Get().LogMessage(Level, "Lua", "%s %s", Prefix, Message.c_str());
		};

	Impl->Env.set_function("log", [LogLuaMessage](sol::variadic_args Args)
	{
		LogLuaMessage(ELogLevel::Info, "[Lua]", Args);
	});

	Impl->Env.set_function("debug_log", [LogLuaMessage](sol::variadic_args Args)
	{
		LogLuaMessage(ELogLevel::Debug, "[Lua][Debug]", Args);
	});

	Impl->Env.set_function("warn", [LogLuaMessage](sol::variadic_args Args)
	{
		LogLuaMessage(ELogLevel::Warning, "[Lua][Warn]", Args);
	});

	Impl->Env.set_function("error_log", [LogLuaMessage](sol::variadic_args Args)
	{
		LogLuaMessage(ELogLevel::Error, "[Lua][Error]", Args);
	});

	Impl->Env.set_function("print", [LogLuaMessage](sol::variadic_args Args)
	{
		// print도 인스턴스 환경에서 엔진 로그로 보내면 WinMain 환경에서도 Lua 로그를 놓치지 않는다.
		LogLuaMessage(ELogLevel::Info, "[Lua]", Args);
	});

	Impl->Env.set_function("time", [this]()
	{
		return Impl ? Impl->ElapsedTime : 0.0f;
	});

	Impl->Env.set_function("delta_time", [this]()
	{
		return Impl ? Impl->LastDeltaTime : 0.0f;
	});
}

void FLuaScriptInstance::BindSoundFunctions()
{
	if (!Impl)
	{
		return;
	}

	Impl->Env.set_function("play_sfx", [](const FString& SoundPath, sol::optional<bool> Looping)
	{
		return FAudioManager::Get().PlaySFX(SoundPath, Looping.value_or(false));
	});

	Impl->Env.set_function("play_bgm", [](const FString& SoundPath, sol::optional<bool> Looping)
	{
		return FAudioManager::Get().PlayBackground(SoundPath, Looping.value_or(true));
	});

	Impl->Env.set_function("stop_audio_by_handle", [](const FString& Handle)
	{
		return FAudioManager::Get().StopSound(Handle);
	});

	Impl->Env.set_function("pause_audio_by_handle", [](const FString& Handle)
	{
		return FAudioManager::Get().PauseSound(Handle);
	});

	Impl->Env.set_function("resume_audio_by_handle", [](const FString& Handle)
	{
		return FAudioManager::Get().ResumeSound(Handle);
	});

	Impl->Env.set_function("is_audio_playing_by_handle", [](const FString& Handle)
	{
		return FAudioManager::Get().IsSoundPlaying(Handle);
	});

	Impl->Env.set_function("stop_bgm", []()
	{
		return FAudioManager::Get().StopBackground();
	});

	Impl->Env.set_function("pause_bgm", []()
	{
		return FAudioManager::Get().PauseBackground();
	});

	Impl->Env.set_function("resume_bgm", []()
	{
		return FAudioManager::Get().ResumeBackground();
	});

	Impl->Env.set_function("is_bgm_playing", []()
	{
		return FAudioManager::Get().IsBackgroundPlaying();
	});

	Impl->Env.set_function("stop_all_audio", []()
	{
		FAudioManager::Get().StopAll();
	});
}

void FLuaScriptInstance::BindWorldFunctions()
{
	if (!Impl)
	{
		return;
	}

	auto GetOwnerWorld = [this]() -> UWorld*
	{
		AActor* OwnerActor = GetOwnerActor();
		return OwnerActor ? OwnerActor->GetWorld() : nullptr;
	};

	Impl->Env.set_function("spawn_actor", [this, GetOwnerWorld](const FString& ClassName, const FVector& Location)
	{
		UWorld* World = GetOwnerWorld();
		if (!World)
		{
			return FLuaActorProxy();
		}

		AActor* SpawnedActor = SpawnActorByClassName(World, ClassName);
		if (!SpawnedActor)
		{
			return FLuaActorProxy();
		}

		SpawnedActor->SetActorLocation(Location);

		// Actor 생성과 소유권은 World/Object system이 담당한다.
		// Lua는 새 Actor를 직접 소유하지 않고 Proxy만 받으므로, Destroy/World 종료 이후에도 Proxy 함수는 생존 체크로 안전하게 실패한다.
		return Impl->TrackProxy(MakeActorProxy(SpawnedActor));
	});

	Impl->Env.set_function("find_actor", [this, GetOwnerWorld](const FString& ActorName)
	{
		UWorld* World = GetOwnerWorld();
		if (!World)
		{
			return FLuaActorProxy();
		}

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !IsAliveObject(Actor))
			{
				continue;
			}

			if (Actor->GetFName().ToString() == ActorName)
			{
				return Impl->TrackProxy(MakeActorProxy(Actor));
			}
		}

		return FLuaActorProxy();
	});

	Impl->Env.set_function("find_actor_by_uuid", [this, GetOwnerWorld](uint32 ActorUUID)
	{
		UWorld* World = GetOwnerWorld();
		if (!World || ActorUUID == 0)
		{
			return FLuaActorProxy();
		}

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !IsAliveObject(Actor))
			{
				continue;
			}

			if (Actor->GetUUID() == ActorUUID)
			{
				return Impl->TrackProxy(MakeActorProxy(Actor));
			}
		}

		return FLuaActorProxy();
	});

	Impl->Env.set_function("find_actor_by_tag", [this, GetOwnerWorld](const FString& Tag)
	{
		UWorld* World = GetOwnerWorld();
		if (!World || Tag.empty())
		{
			return FLuaActorProxy();
		}

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !IsAliveObject(Actor))
			{
				continue;
			}

			if (Actor->HasTag(Tag))
			{
				return Impl->TrackProxy(MakeActorProxy(Actor));
			}
		}

		return FLuaActorProxy();
	});

	Impl->Env.set_function("find_actors_by_tag", [this, GetOwnerWorld](const FString& Tag)
	{
		sol::table Result = FLuaScriptRuntime::Get().GetLuaState().create_table();
		UWorld* World = GetOwnerWorld();
		if (!World || Tag.empty())
		{
			return Result;
		}

		int LuaIndex = 1;
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !IsAliveObject(Actor))
			{
				continue;
			}

			if (Actor->HasTag(Tag))
			{
				Result[LuaIndex++] = Impl->TrackProxy(MakeActorProxy(Actor));
			}
		}

		return Result;
	});

	Impl->Env.set_function("destroy_actor", [](FLuaActorProxy& ActorProxy)
	{
		// destroy_actor는 Lua가 Actor를 소유한다는 뜻이 아니라 Proxy의 Destroy 래퍼일 뿐이다.
		ActorProxy.Destroy();
	});

	Impl->Env.set_function("load_scene", [](const FString& SceneReference)
	{
		return GEngine ? GEngine->RequestSceneLoad(SceneReference) : false;
	});

	Impl->Env.set_function("set_global_time_dilation", [](const float TimeDilation)
	{
		if (!GEngine) return false;

		UWorld* World = GEngine->GetWorld();
		if (!World) return false;
		
		World->SetGlobalTimeDilation(TimeDilation);
		return true;
	});

	Impl->Env.set_function("get_global_time_dilation", []()
	{
		if (!GEngine)
		{
			return 1.0f;
		}

		UWorld* World = GEngine->GetWorld();
		return World ? World->GetGlobalTimeDilation() : 1.0f;
	});

	Impl->Env.set_function("raw_delta_time", []()
	{
		if (!GEngine)
		{
			return 0.0f;
		}

		UWorld* World = GEngine->GetWorld();
		return World ? World->GetRawDeltaTime() : 0.0f;
	});

	// Global에 load Scene Binding
	FLuaScriptRuntime::Get().GetLuaState().set_function("load_scene", [](const FString& SceneReference)
	{
		return GEngine ? GEngine->RequestSceneLoad(SceneReference) : false;
	});	
}

void FLuaScriptInstance::BindDataFunctions()
{
	if (!Impl)
	{
		return;
	}

	Impl->Env.set_function("require_env", [this](const FString& ModuleName) -> sol::object
		{
			sol::state& Lua = FLuaScriptRuntime::Get().GetLuaState();

			if (!Impl || ModuleName.empty())
			{
				return sol::make_object(Lua, sol::lua_nil);
			}

			sol::table ModuleCache = Impl->Env["__env_loaded_modules"];
			if (!ModuleCache.valid())
			{
				ModuleCache = Lua.create_table();
				Impl->Env["__env_loaded_modules"] = ModuleCache;
			}

			sol::object CachedModule = ModuleCache[ModuleName];
			if (CachedModule.valid() && CachedModule != sol::lua_nil)
			{
				return CachedModule;
			}

			const FString ScriptPath = MakeRequireEnvScriptPath(ModuleName);

			FString ScriptSource;
			FString FileReadError;
			if (!FScriptPaths::ReadScriptFile(ScriptPath, ScriptSource, FileReadError))
			{
				SetError("require_env failed: " + FileReadError);
				return sol::make_object(Lua, sol::lua_nil);
			}

			const std::filesystem::path ResolvedPath = FScriptPaths::ResolveScriptPath(ScriptPath);
			const FString ChunkName = FPaths::ToUtf8(ResolvedPath.generic_wstring());

			sol::protected_function_result Result =
				Lua.safe_script(
					ScriptSource,
					Impl->Env,
					sol::script_pass_on_error,
					ChunkName,
					sol::load_mode::text);

			if (!Result.valid() || Result.status() == sol::call_status::yielded)
			{
				Impl->HandleProtectedResult(
					this,
					"require_env(" + ModuleName + ")",
					std::move(Result));

				return sol::make_object(Lua, sol::lua_nil);
			}

			sol::object ModuleResult = sol::make_object(Lua, sol::lua_nil);

			if (Result.return_count() > 0)
			{
				ModuleResult = Result.get<sol::object>(0);
			}

			// Lua require는 return 값이 없으면 true를 캐시한다.
			// 똑같이 맞춰준다.
			if (!ModuleResult.valid() || ModuleResult == sol::lua_nil)
			{
				ModuleResult = sol::make_object(Lua, true);
			}

			ModuleCache[ModuleName] = ModuleResult;
			return ModuleResult;
		});

	auto LoadJsonFile = [](const FString& FilePath)
	{
		sol::state_view Lua = FLuaScriptRuntime::Get().GetLuaState();
		if (FilePath.empty())
		{
			return sol::make_object(Lua, sol::lua_nil);
		}

		const std::filesystem::path AbsolutePath = ResolveDataFilePath(FilePath);
		std::ifstream File(AbsolutePath);
		if (!File.is_open())
		{
			return sol::make_object(Lua, sol::lua_nil);
		}

		FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		if (Content.empty())
		{
			return sol::make_object(Lua, sol::lua_nil);
		}

		const json::JSON Root = json::JSON::Load(Content);
		return MakeLuaObjectFromJson(Lua, Root);
	};

	auto SaveJsonFile = [](const FString& FilePath, sol::object Data)
	{
		if (FilePath.empty())
		{
			return false;
		}

		const std::filesystem::path AbsolutePath = ResolveDataFilePath(FilePath);
		std::error_code ErrorCode;
		if (AbsolutePath.has_parent_path())
		{
			std::filesystem::create_directories(AbsolutePath.parent_path(), ErrorCode);
		}

		std::ofstream File(AbsolutePath, std::ios::trunc);
		if (!File.is_open())
		{
			return false;
		}

		File << MakeJsonFromLuaObject(Data).dump();
		return File.good();
	};

	auto OpenScoreSavePopup = [](int32 Score)
	{
		if (!GEngine)
		{
			return false;
		}

		GEngine->OpenScoreSavePopup(Score);
		return true;
	};

	auto ConsumeScoreSavePopupResult = []()
	{
		sol::state_view Lua = FLuaScriptRuntime::Get().GetLuaState();
		if (!GEngine)
		{
			return sol::make_object(Lua, sol::lua_nil);
		}

		FString Nickname;
		if (!GEngine->ConsumeScoreSavePopupResult(Nickname))
		{
			return sol::make_object(Lua, sol::lua_nil);
		}

		return sol::make_object(Lua, Nickname);
	};

	auto OpenMessagePopup = [](const FString& Message)
	{
		if (!GEngine)
		{
			return false;
		}

		GEngine->OpenMessagePopup(Message);
		return true;
	};

	auto ConsumeMessagePopupConfirmed = []()
	{
		return GEngine ? GEngine->ConsumeMessagePopupConfirmed() : false;
	};

	auto OpenScoreboardPopup = [](const FString& FilePath)
	{
		if (!GEngine)
		{
			return false;
		}

		GEngine->OpenScoreboardPopup(FilePath);
		return true;
	};

	auto OpenTitleOptionsPopup = []()
	{
		if (!GEngine)
		{
			return false;
		}

		GEngine->OpenTitleOptionsPopup();
		return true;
	};

	auto OpenTitleCreditsPopup = []()
	{
		if (!GEngine)
		{
			return false;
		}

		GEngine->OpenTitleCreditsPopup();
		return true;
	};

	auto RequestExitGame = []()
	{
		if (!GEngine || !GEngine->GetWindow())
		{
			return false;
		}

		GEngine->GetWindow()->Close();
		return true;
	};

	Impl->Env.set_function("load_json_file", LoadJsonFile);
	Impl->Env.set_function("save_json_file", SaveJsonFile);
	Impl->Env.set_function("open_score_save_popup", OpenScoreSavePopup);
	Impl->Env.set_function("consume_score_save_popup_result", ConsumeScoreSavePopupResult);
	Impl->Env.set_function("open_message_popup", OpenMessagePopup);
	Impl->Env.set_function("consume_message_popup_ok", ConsumeMessagePopupConfirmed);
	Impl->Env.set_function("open_scoreboard_popup", OpenScoreboardPopup);
	Impl->Env.set_function("open_title_options_popup", OpenTitleOptionsPopup);
	Impl->Env.set_function("open_title_credits_popup", OpenTitleCreditsPopup);
	Impl->Env.set_function("request_exit_game", RequestExitGame);

	sol::state& Lua = FLuaScriptRuntime::Get().GetLuaState();
	Lua.set_function("load_json_file", LoadJsonFile);
	Lua.set_function("save_json_file", SaveJsonFile);
	Lua.set_function("open_score_save_popup", OpenScoreSavePopup);
	Lua.set_function("consume_score_save_popup_result", ConsumeScoreSavePopupResult);
	Lua.set_function("open_message_popup", OpenMessagePopup);
	Lua.set_function("consume_message_popup_ok", ConsumeMessagePopupConfirmed);
	Lua.set_function("open_scoreboard_popup", OpenScoreboardPopup);
	Lua.set_function("open_title_options_popup", OpenTitleOptionsPopup);
	Lua.set_function("open_title_credits_popup", OpenTitleCreditsPopup);
	Lua.set_function("request_exit_game", RequestExitGame);
}

FLuaActorProxy FLuaScriptInstance::MakeActorProxy(AActor* Actor) const
{
	FLuaActorProxy Proxy;
	// Lua 쪽에 죽은 UObject 포인터가 넘어가지 않도록 살아 있는 actor만 노출한다.
	Proxy.Actor = (Actor && IsAliveObject(Actor)) ? Actor : nullptr;
	return Proxy;
}

FLuaComponentProxy FLuaScriptInstance::MakeComponentProxy(UActorComponent* Component)
{
	FLuaComponentProxy Proxy;
	// Lua에 넘기는 순간에도 한 번 거르지만, Proxy가 복사되어 오래 살아남을 수 있으므로 실제 안전성은 각 함수의 재검증이 책임진다.
	Proxy.Component = IsAliveObject(Component) ? Component : nullptr;
	return Proxy;
}

void FLuaScriptInstance::SetError(const FString& ErrorMessage)
{
	if (!Impl)
	{
		Impl = std::make_unique<FInstanceImpl>();
	}

	Impl->bHasError = true;
	Impl->LastError = ErrorMessage;
	UE_LOG_CATEGORY(LuaScript, Error, "%s", ErrorMessage.c_str());
}
