#include "Scripting/LuaScriptRuntime.h"

#include "Component/ScriptComponent.h"
#include "Core/EngineTypes.h"
#include "Core/AsciiUtils.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "Object/Object.h"
#include "Platform/DirectoryWatcher.h"
#include "Platform/Paths.h"
#include "Platform/ScriptPaths.h"

// 이거 넣는 이유: sol/sol.hpp 내장 check하고 충돌 방지 목적
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

#include <sol/sol.hpp>

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

struct FLuaScriptRuntime::FRuntimeImpl
{
	// 런타임이 소유하는 전역 Lua VM.
	// 개별 ScriptInstance는 이 VM 위에 environment만 따로 만든다.
	sol::state Lua;
};

namespace
{
	FString MakeRequireScriptPath(const FString& ModuleName)
	{
		FString ScriptPath = ModuleName;

		std::replace(ScriptPath.begin(), ScriptPath.end(), '.', '/');
		std::replace(ScriptPath.begin(), ScriptPath.end(), '\\', '/');

		if (ScriptPath.size() < 4 || ScriptPath.substr(ScriptPath.size() - 4) != ".lua")
		{
			ScriptPath += ".lua";
		}

		return FScriptPaths::NormalizeScriptPath(ScriptPath);
	}

	bool IsTruthyLuaObject(const sol::object& Object)
	{
		if (!Object.valid() || Object == sol::lua_nil)
		{
			return false;
		}

		if (Object.get_type() == sol::type::boolean)
		{
			return Object.as<bool>();
		}

		return true;
	}

	void ConfigurePackagePath(sol::state& Lua)
	{
		sol::table Package = Lua["package"];
		if (!Package.valid())
		{
			return;
		}

		std::filesystem::path ScriptRootPath(FPaths::ScriptsDir());
		FString ScriptRoot = FPaths::ToUtf8(ScriptRootPath.lexically_normal().generic_wstring());
		while (!ScriptRoot.empty() && (ScriptRoot.back() == '/' || ScriptRoot.back() == '\\'))
		{
			ScriptRoot.pop_back();
		}

		// require는 프로젝트 Scripts 폴더 아래만 찾게 제한한다.
		// 기존 package.path를 뒤에 붙이면 실행 환경마다 외부 Lua 파일이 섞일 수 있으므로 사용하지 않는다.
		Package["path"] = ScriptRoot + "/?.lua;" + ScriptRoot + "/?/init.lua";
		Package["cpath"] = "";

		// require로 불러온 모듈은 Lua state 안에서 캐시됩니다.
		// 여러 스크립트가 같은 모듈 테이블을 공유하므로 Config는 상수처럼 사용하는 것이 안전합니다.
	}

	void BindWidePathRequire(sol::state& Lua)
	{
		Lua.set_function("require", [&Lua](const FString& ModuleName) -> sol::object
		{
			if (ModuleName.empty())
			{
				throw sol::error("require failed: module name is empty");
			}

			sol::table Package = Lua["package"];
			if (!Package.valid())
			{
				Package = Lua.create_table();
				Lua["package"] = Package;
			}

			sol::table Loaded = Package["loaded"];
			if (!Loaded.valid())
			{
				Loaded = Lua.create_table();
				Package["loaded"] = Loaded;
			}

			sol::object CachedModule = Loaded[ModuleName];
			if (IsTruthyLuaObject(CachedModule))
			{
				return CachedModule;
			}

			const FString ScriptPath = MakeRequireScriptPath(ModuleName);

			FString ScriptSource;
			FString FileReadError;
			if (!FScriptPaths::ReadScriptFile(ScriptPath, ScriptSource, FileReadError))
			{
				const FString ErrorMessage = "require failed: " + FileReadError;
				throw sol::error(ErrorMessage);
			}

			const std::filesystem::path ResolvedPath = FScriptPaths::ResolveScriptPath(ScriptPath);
			const FString ChunkName = "@" + FPaths::ToUtf8(ResolvedPath.generic_wstring());

			// Mark as loading before executing the chunk, matching Lua require's shared module cache behavior.
			Loaded[ModuleName] = true;

			sol::protected_function_result Result =
				Lua.safe_script(
					ScriptSource,
					sol::script_pass_on_error,
					ChunkName,
					sol::load_mode::text);

			if (!Result.valid() || Result.status() == sol::call_status::yielded)
			{
				Loaded[ModuleName] = sol::lua_nil;
				FString ErrorText = "module yielded while loading";
				if (!Result.valid())
				{
					sol::error Error = Result;
					ErrorText = Error.what();
				}

				const FString ErrorMessage = "require failed: " + ModuleName + ": " + ErrorText;
				throw sol::error(ErrorMessage);
			}

			sol::object ModuleResult = sol::make_object(Lua, sol::lua_nil);
			if (Result.return_count() > 0)
			{
				ModuleResult = Result.get<sol::object>(0);
			}
			if (ModuleResult.valid() && ModuleResult != sol::lua_nil)
			{
				Loaded[ModuleName] = ModuleResult;
				return ModuleResult;
			}

			sol::object LoadedModule = Loaded[ModuleName];
			if (LoadedModule.valid() && LoadedModule != sol::lua_nil)
			{
				return LoadedModule;
			}

			Loaded[ModuleName] = true;
			return sol::make_object(Lua, true);
		});
	}

	bool IsLuaScriptFile(const FString& Path)
	{
		// DirectoryWatcher는 prefix 포함 상대 경로를 넘겨주므로 확장자만 보고 Lua 대상인지 빠르게 걸러낸다.
		const std::filesystem::path FilePath(FPaths::ToWide(Path));
		FString Extension = FPaths::ToUtf8(FilePath.extension().wstring());
		AsciiUtils::ToLowerInPlace(Extension);
		return Extension == ".lua";
	}
}

FLuaScriptRuntime::FLuaScriptRuntime()
{
	Initialize();
}

FLuaScriptRuntime::~FLuaScriptRuntime()
{
	Shutdown();
}

bool FLuaScriptRuntime::Initialize()
{
	if (bInitialized)
	{
		return true;
	}

	LastError.clear();

	try
	{
		// Runtime은 Lua VM 하나를 전역으로 유지하고,
		// 각 ScriptInstance는 이 VM 위에 독립된 environment를 만들어 쓴다.
		Impl = std::make_unique<FRuntimeImpl>();
		Impl->Lua.open_libraries(
			sol::lib::base,
			sol::lib::math,
			sol::lib::string,
			sol::lib::table,
			sol::lib::coroutine,
			sol::lib::package);
		ConfigurePackagePath(Impl->Lua);
		BindWidePathRequire(Impl->Lua);

		RegisterBindings();

		// 타입 바인딩이 끝난 뒤 watcher를 붙여야 핫 리로드가 즉시 동작할 수 있다.
		InitializeHotReload();

		bInitialized = true;
		return true;
	}
	catch (const std::exception& Exception)
	{
		ShutdownHotReload();
		LastError = Exception.what();
		bInitialized = false;
		Impl.reset();
		UE_LOG_CATEGORY(LuaRuntime, Error, "Initialize failed: %s", LastError.c_str());
		return false;
	}
}

void FLuaScriptRuntime::Shutdown()
{
	// watcher를 먼저 정리해서 shutdown 이후 콜백이 날아오지 않게 막는다.
	ShutdownHotReload();
	Impl.reset();
	bInitialized = false;
	LastError.clear();
}

sol::state& FLuaScriptRuntime::GetLuaState()
{
	if (!Impl)
	{
		// 정상 경로에서는 Initialize가 먼저 호출되지만, 방어적으로 접근 시점에 저장소를 다시 만든다.
		Impl = std::make_unique<FRuntimeImpl>();
	}

	return Impl->Lua;
}

const FString& FLuaScriptRuntime::GetLastError() const
{
	return LastError;
}

void FLuaScriptRuntime::RegisterScriptComponent(UScriptComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// 같은 컴포넌트가 여러 번 등록돼도 TSet이 중복을 막아준다.
	ScriptComponents.insert(Component);
}

void FLuaScriptRuntime::UnregisterScriptComponent(UScriptComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// EndPlay나 경로 변경 등 어떤 이유든 더 이상 대상이 아니면 즉시 목록에서 뺀다.
	ScriptComponents.erase(Component);
}

void FLuaScriptRuntime::RegisterBindings()
{
	// 런타임 레벨에서 공통으로 노출할 타입은 모두 여기서 한 번만 등록한다.
	BindVectorType();
	BindRotatorType();
	BindActorProxyType();
	BindComponentProxyType();
	// Color 바인딩은 유지 보수용 함수만 남기고 공개하지 않는다.
}

// ... (other functions)

void FLuaScriptRuntime::BindRotatorType()
{
	sol::state& Lua = GetLuaState();

	Lua.new_usertype<FRotator>(
		"Rotator",
		sol::constructors<FRotator(), FRotator(float, float, float)>(),
		"pitch", sol::property([](const FRotator& Value) { return Value.Pitch; }, [](FRotator& Value, float InPitch) { Value.Pitch = InPitch; }),
		"yaw", sol::property([](const FRotator& Value) { return Value.Yaw; }, [](FRotator& Value, float InYaw) { Value.Yaw = InYaw; }),
		"roll", sol::property([](const FRotator& Value) { return Value.Roll; }, [](FRotator& Value, float InRoll) { Value.Roll = InRoll; }),
		sol::meta_function::addition, [](const FRotator& A, const FRotator& B) { return A + B; },
		sol::meta_function::subtraction, [](const FRotator& A, const FRotator& B) { return A - B; },
		sol::meta_function::multiplication, [](const FRotator& A, float Scalar) { return A * Scalar; }
	);

	Lua.set_function("rotator", [](float Pitch, float Yaw, float Roll)
	{
		return FRotator(Pitch, Yaw, Roll);
	});
}

void FLuaScriptRuntime::InitializeHotReload()
{
	if (WatchSub != 0)
	{
		return;
	}

	// watcher가 넘겨주는 경로도 내부 저장 경로와 같은 "Scripts/" prefix를 쓰게 한다.
	const FWatchID WatchID = FDirectoryWatcher::Get().Watch(FPaths::ScriptsDir(), "Scripts/");
	if (WatchID == 0)
	{
		UE_LOG_CATEGORY(LuaRuntime, Warning, "Failed to watch Scripts directory.");
		return;
	}

	WatchSub = FDirectoryWatcher::Get().Subscribe(
	WatchID,
	[this](const TSet<FString>& Files)
	{
		// ProcessChanges()에서 메인 스레드로 디스패치되므로
		// 여기서 바로 UObject 접근과 ReloadScript() 호출이 가능하다.
		OnScriptsChanged(Files);
	});
}

void FLuaScriptRuntime::ShutdownHotReload()
{
	if (WatchSub != 0)
	{
		FDirectoryWatcher::Get().Unsubscribe(WatchSub);
		WatchSub = 0;
	}

	// 플레이 세션이 끝나면 이전 component 포인터를 다음 세션까지 끌고 가지 않는다.
	ScriptComponents.clear();
}

void FLuaScriptRuntime::OnScriptsChanged(const TSet<FString>& ChangedFiles)
{
	TSet<FString> ChangedLuaScripts;
	for (const FString& ChangedFile : ChangedFiles)
	{
		if (!IsLuaScriptFile(ChangedFile))
		{
			continue;
		}

		const FString NormalizedPath = FScriptPaths::NormalizeScriptPath(ChangedFile);
		if (!NormalizedPath.empty())
		{
			// Watch prefix도 Scripts/ 이고 내부 저장 경로도 Scripts/ 로 통일돼 있으므로
			// 여기서 한 번만 정규화하면 곧바로 component ScriptPath와 비교할 수 있다.
			ChangedLuaScripts.insert(NormalizedPath);
		}
	}

	if (ChangedLuaScripts.empty() || ScriptComponents.empty())
	{
		return;
	}

	// 순회 중 죽은 UObject를 바로 erase하면 iterator 관리가 번거로워지므로
	// stale 목록에 모아 뒀다가 마지막에 한 번에 정리한다.
	TArray<UScriptComponent*> StaleComponents;
	for (UScriptComponent* Component : ScriptComponents)
	{
		if (!Component || !IsAliveObject(Component))
		{
			StaleComponents.push_back(Component);
			continue;
		}

		const FString ComponentScriptPath = FScriptPaths::NormalizeScriptPath(Component->GetScriptPath());
		if (ComponentScriptPath.empty() || ChangedLuaScripts.find(ComponentScriptPath) == ChangedLuaScripts.end())
		{
			continue;
		}

		// 실제 재초기화 순서는 Component가 알고 있으므로 Runtime은 위임만 한다.
		if (Component->ReloadScript())
		{
			UE_LOG_CATEGORY(ScriptHotReload, Info, "Reloaded: %s", ComponentScriptPath.c_str());
			FNotificationManager::Get().AddNotification("Script Reloaded: " + ComponentScriptPath, ENotificationType::Success, 3.0f);
		}
		else
		{
			const FString ReloadError = Component->GetLastScriptError();
			if (ReloadError.empty())
			{
				UE_LOG_CATEGORY(ScriptHotReload, Error, "Failed: %s", ComponentScriptPath.c_str());
			}
			else
			{
				UE_LOG_CATEGORY(ScriptHotReload, Error, "Failed: %s (%s)", ComponentScriptPath.c_str(), ReloadError.c_str());
			}
			FNotificationManager::Get().AddNotification("Script Failed: " + ComponentScriptPath, ENotificationType::Error, 5.0f);
		}
	}

	for (UScriptComponent* StaleComponent : StaleComponents)
	{
		// UObject가 이미 파괴된 포인터는 다음 이벤트부터 순회하지 않도록 정리한다.
		ScriptComponents.erase(StaleComponent);
	}
}

// TODO: 어쩔 수 없이 하드코딩으로 바인딩 -> 추후 구조 개선 고려
void FLuaScriptRuntime::BindVectorType()
{
	sol::state& Lua = GetLuaState();

	// 스크립트에서 수학 타입을 자연스럽게 쓰도록 연산자까지 함께 바인딩한다.
	Lua.new_usertype<FVector>(
		"Vector",
		sol::constructors<FVector(), FVector(float, float, float)>(),
		"x", sol::property([](const FVector& Value) { return Value.X; }, [](FVector& Value, float InX) { Value.X = InX; }),
		"y", sol::property([](const FVector& Value) { return Value.Y; }, [](FVector& Value, float InY) { Value.Y = InY; }),
		"z", sol::property([](const FVector& Value) { return Value.Z; }, [](FVector& Value, float InZ) { Value.Z = InZ; }),
		sol::meta_function::addition, [](const FVector& A, const FVector& B) { return A + B; },
		sol::meta_function::subtraction, [](const FVector& A, const FVector& B) { return A - B; },
		sol::meta_function::multiplication, sol::overload(
			[](const FVector& A, float Scalar) { return A * Scalar; },
			[](float Scalar, const FVector& A) { return A * Scalar; }),
		sol::meta_function::division, [](const FVector& A, float Scalar)
		{
			return Scalar == 0.0f ? FVector(0.0f, 0.0f, 0.0f) : (A / Scalar);
		});

	Lua.set_function("vec3", [](float X, float Y, float Z)
	{
		return FVector(X, Y, Z);
	});

	Lua.set_function("Vector3", [](float X, float Y, float Z)
	{
		return FVector(X, Y, Z);
	});

	Lua.set_function("print", [](sol::variadic_args Args)
	{
		FString Message;

		for (auto Arg : Args)
		{
			if (!Message.empty())
			{
				Message += " ";
			}

			Message += Arg.as<FString>();
		}

		UE_LOG("[Lua] %s", Message.c_str());
	});
}

void FLuaScriptRuntime::BindColorType()
{
	sol::state& Lua = GetLuaState();

	Lua.new_usertype<FColor>(
		"Color",
		sol::constructors<FColor(), FColor(int32, int32, int32, int32)>(),
		"r", sol::property([](const FColor& Value) { return Value.R; }, [](FColor& Value, int32 InR) { Value.R = InR; }),
		"g", sol::property([](const FColor& Value) { return Value.G; }, [](FColor& Value, int32 InG) { Value.G = InG; }),
		"b", sol::property([](const FColor& Value) { return Value.B; }, [](FColor& Value, int32 InB) { Value.B = InB; }),
		"a", sol::property([](const FColor& Value) { return Value.A; }, [](FColor& Value, int32 InA) { Value.A = InA; })
	);

	Lua.set_function("MakeColor", [](int32 R, int32 G, int32 B, int32 A)
	{
		return FColor(R, G, B, A);
	});
}
