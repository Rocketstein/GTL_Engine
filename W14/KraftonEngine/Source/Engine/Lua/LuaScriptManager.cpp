#include "LuaScriptManager.h"
#include "Lua/LuaDebugManager.h"

#include "Audio/AudioManager.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/Notification.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <windows.h>  // PostQuitMessage
#include "Animation/AnimInstance.h"
#include "Animation/Graph/AnimGraphInstance.h"
#include "Animation/Instance/LuaAnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Animation/Sequence/AnimSequence.h"
#include "CameraShake/CameraShakeAsset.h"
#include "CameraShake/CameraShakeManager.h"
#include "Component/ActorComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/ShapeComponent.h"
#include "Component/Camera/CameraComponent.h"
#include "Component/Camera/CineCameraComponent.h"
#include "Component/Camera/SpringArmComponent.h"
#include "Component/Input/ActionComponent.h"
#include "Component/AI/EncounterComponent.h"
#include "Component/AI/EnemyAIBrainComponent.h"
#include "Component/AI/EnemyAttackComponent.h"
#include "Component/AI/PhaseComponent.h"
#include "Component/Character/CharacterStateMachineComponent.h"
#include "AI/Navigation/PathFollowingComponent.h"
#include "Component/Combat/CombatHitEventComponent.h"
#include "Component/Combat/CombatStateComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Component/Combat/ExecutionComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "Component/Input/InputComponent.h"
#include "Component/Light/AmbientLightComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Light/LightComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/Light/PointLightComponent.h"
#include "Component/Light/SpotLightComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Movement/FloatingPawnMovementComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/Movement/PendulumMovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"
#include "Component/Movement/WheeledVehicleMovementComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Component/Primitive/TextRenderComponent.h"
#include "Component/Script/LuaBlueprintComponent.h"
#include "Component/Script/LuaScriptComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Vehicle/VehicleWheelPoseComponent.h"
#include "Core/Property/ArrayProperty.h"
#include "Core/Property/BoolProperty.h"
#include "Core/Property/ClassProperty.h"
#include "Core/Property/EnumProperty.h"
#include "Core/Property/NameProperty.h"
#include "Core/Property/NumericProperty.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/SoftObjectProperty.h"
#include "Core/Property/StringProperty.h"
#include "Core/Property/StructProperty.h"
#include "Core/Types/CollisionTypes.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/Actor/NavMeshBoundsVolume.h"
#include "GameFramework/Actor/ParticleSystemActor.h"
#include "GameFramework/Camera/CameraModifier.h"
#include "GameFramework/Camera/CameraShakeBase.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/Camera/SequenceCameraShake.h"
#include "GameFramework/Camera/WaveOscillatorCameraShake.h"
#include "GameFramework/GameMode/GameplayStatics.h"
#include "GameFramework/Controller/AIController.h"
#include "GameFramework/Controller/Controller.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/BaseCombatCharacter.h"
#include "GameFramework/Pawn/BossEnemyCharacter.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/Pawn/EnemyCharacter.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/Pawn/WheeledVehiclePawn.h"
#include "Game/Components/LockOnComponent.h"
#include "Input/InputKeyCodes.h"
#include "Input/InputSystem.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Navigation/NavigationSystem.h"
#include "Navigation/NavigationData.h"
#include "Navigation/GridNavMesh.h"
#include "Navigation/NavTypes.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Static/StaticMesh.h"
#include "Object/GarbageCollection.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Reflection/UClass.h"
#include "Object/Reflection/UStruct.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleSystemManager.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Platform/Paths.h"
#include "Platform/WindowsWindow.h"
#include "Profiling/Time/Timer.h"
#include "Runtime/Engine.h"
#include "Texture/Texture2D.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"
#include "Viewport/GameViewportClient.h"

std::unique_ptr<sol::state>                 FLuaScriptManager::Lua;
sol::protected_function                     FLuaScriptManager::OnEscapePressedCallback;
std::mutex                                  FLuaScriptManager::ComponentMutex;
TArray<TWeakObjectPtr<ULuaScriptComponent>> FLuaScriptManager::RegisteredComponents;
TArray<TWeakObjectPtr<ULuaAnimInstance>>    FLuaScriptManager::RegisteredAnimInstances;
FSubscriptionID                             FLuaScriptManager::WatchSub = 0;

namespace
{
    struct FLuaReflectedEventOverride
    {
        TWeakObjectPtr<UObject> Target;
        sol::protected_function Callback;
    };

    TMap<FString, FLuaReflectedEventOverride> GLuaReflectedEventOverrides;

    struct FLuaHealthChangedBinding
    {
        TWeakObjectPtr<UHealthComponent> Target;
        FDelegateHandle Handle;
        sol::protected_function Callback;
    };

    TMap<uint64, FLuaHealthChangedBinding> GLuaHealthChangedBindings;
    uint64 GNextLuaHealthChangedBindingId = 1;

    struct FLuaPostureChangedBinding
    {
        TWeakObjectPtr<UCombatStateComponent> Target;
        FDelegateHandle Handle;
        sol::protected_function Callback;
    };

    TMap<uint64, FLuaPostureChangedBinding> GLuaPostureChangedBindings;
    uint64 GNextLuaPostureChangedBindingId = 1;

    struct FLuaPlayerPawnChangedBinding
    {
        TWeakObjectPtr<UWorld> Target;
        FDelegateHandle Handle;
        sol::protected_function Callback;
    };

    TMap<uint64, FLuaPlayerPawnChangedBinding> GLuaPlayerPawnChangedBindings;
    uint64 GNextLuaPlayerPawnChangedBindingId = 1;

    void RemoveLuaHealthChangedBinding(uint64 BindingId)
    {
        auto It = GLuaHealthChangedBindings.find(BindingId);
        if (It == GLuaHealthChangedBindings.end())
        {
            return;
        }

        if (UHealthComponent* Target = It->second.Target.Get())
        {
            Target->OnHealthChanged.Remove(It->second.Handle);
        }
        GLuaHealthChangedBindings.erase(It);
    }

    void ClearLuaHealthChangedBindings()
    {
        while (!GLuaHealthChangedBindings.empty())
        {
            RemoveLuaHealthChangedBinding(GLuaHealthChangedBindings.begin()->first);
        }
    }

    uint64 BindLuaHealthChanged(UHealthComponent* Target, sol::protected_function Callback)
    {
        if (!IsValid(Target) || !Callback.valid())
        {
            return 0;
        }

        const uint64 BindingId = GNextLuaHealthChangedBindingId++;
        FLuaHealthChangedBinding Binding;
        Binding.Target = Target;
        Binding.Callback = std::move(Callback);
        Binding.Handle = Target->OnHealthChanged.AddLambda(
            [BindingId](UHealthComponent* Component, float PreviousHealth, float CurrentHealth, float MaxHealth)
            {
                auto It = GLuaHealthChangedBindings.find(BindingId);
                if (It == GLuaHealthChangedBindings.end() || !It->second.Callback.valid())
                {
                    return;
                }

                FScopedGarbageCollectionBlocker GCBlocker;
                sol::protected_function_result Result =
                    It->second.Callback(Component, PreviousHealth, CurrentHealth, MaxHealth);
                if (!Result.valid())
                {
                    sol::error Error = Result;
                    UE_LOG("[Lua] HealthComponent OnHealthChanged callback error: %s", Error.what());
                }
            });
        GLuaHealthChangedBindings.emplace(BindingId, std::move(Binding));
        return BindingId;
    }

    bool UnbindLuaHealthChanged(UHealthComponent* Target, uint64 BindingId)
    {
        auto It = GLuaHealthChangedBindings.find(BindingId);
        if (It == GLuaHealthChangedBindings.end() || It->second.Target.Get() != Target)
        {
            return false;
        }

        RemoveLuaHealthChangedBinding(BindingId);
        return true;
    }

    void RemoveLuaPostureChangedBinding(uint64 BindingId)
    {
        auto It = GLuaPostureChangedBindings.find(BindingId);
        if (It == GLuaPostureChangedBindings.end())
        {
            return;
        }

        if (UCombatStateComponent* Target = It->second.Target.Get())
        {
            Target->OnPostureChanged.Remove(It->second.Handle);
        }
        GLuaPostureChangedBindings.erase(It);
    }

    void ClearLuaPostureChangedBindings()
    {
        while (!GLuaPostureChangedBindings.empty())
        {
            RemoveLuaPostureChangedBinding(GLuaPostureChangedBindings.begin()->first);
        }
    }

    uint64 BindLuaPostureChanged(UCombatStateComponent* Target, sol::protected_function Callback)
    {
        if (!IsValid(Target) || !Callback.valid())
        {
            return 0;
        }

        const uint64 BindingId = GNextLuaPostureChangedBindingId++;
        FLuaPostureChangedBinding Binding;
        Binding.Target = Target;
        Binding.Callback = std::move(Callback);
        Binding.Handle = Target->OnPostureChanged.AddLambda(
            [BindingId](UCombatStateComponent* Component, float PreviousPoise, float CurrentPoise, float MaxPoise)
            {
                auto It = GLuaPostureChangedBindings.find(BindingId);
                if (It == GLuaPostureChangedBindings.end() || !It->second.Callback.valid())
                {
                    return;
                }

                FScopedGarbageCollectionBlocker GCBlocker;
                sol::protected_function_result Result =
                    It->second.Callback(Component, PreviousPoise, CurrentPoise, MaxPoise);
                if (!Result.valid())
                {
                    sol::error Error = Result;
                    UE_LOG("[Lua] CombatStateComponent OnPostureChanged callback error: %s", Error.what());
                }
            });
        GLuaPostureChangedBindings.emplace(BindingId, std::move(Binding));
        return BindingId;
    }

    bool UnbindLuaPostureChanged(UCombatStateComponent* Target, uint64 BindingId)
    {
        auto It = GLuaPostureChangedBindings.find(BindingId);
        if (It == GLuaPostureChangedBindings.end() || It->second.Target.Get() != Target)
        {
            return false;
        }

        RemoveLuaPostureChangedBinding(BindingId);
        return true;
    }

    void RemoveLuaPlayerPawnChangedBinding(uint64 BindingId)
    {
        auto It = GLuaPlayerPawnChangedBindings.find(BindingId);
        if (It == GLuaPlayerPawnChangedBindings.end())
        {
            return;
        }

        if (UWorld* Target = It->second.Target.Get())
        {
            Target->OnPlayerPawnChanged.Remove(It->second.Handle);
        }
        GLuaPlayerPawnChangedBindings.erase(It);
    }

    void ClearLuaPlayerPawnChangedBindings()
    {
        while (!GLuaPlayerPawnChangedBindings.empty())
        {
            RemoveLuaPlayerPawnChangedBinding(GLuaPlayerPawnChangedBindings.begin()->first);
        }
    }

    uint64 BindLuaPlayerPawnChanged(UWorld* Target, sol::protected_function Callback)
    {
        if (!IsValid(Target) || !Callback.valid())
        {
            return 0;
        }

        const uint64 BindingId = GNextLuaPlayerPawnChangedBindingId++;
        FLuaPlayerPawnChangedBinding Binding;
        Binding.Target = Target;
        Binding.Callback = std::move(Callback);
        Binding.Handle = Target->OnPlayerPawnChanged.AddLambda(
            [BindingId](APlayerController* PlayerController, APawn* OldPawn, APawn* NewPawn)
            {
                auto It = GLuaPlayerPawnChangedBindings.find(BindingId);
                if (It == GLuaPlayerPawnChangedBindings.end() || !It->second.Callback.valid())
                {
                    return;
                }

                FScopedGarbageCollectionBlocker GCBlocker;
                sol::protected_function_result Result =
                    It->second.Callback(PlayerController, OldPawn, NewPawn);
                if (!Result.valid())
                {
                    sol::error Error = Result;
                    UE_LOG("[Lua] World OnPlayerPawnChanged callback error: %s", Error.what());
                }
            });
        GLuaPlayerPawnChangedBindings.emplace(BindingId, std::move(Binding));
        return BindingId;
    }

    bool UnbindLuaPlayerPawnChanged(UWorld* Target, uint64 BindingId)
    {
        auto It = GLuaPlayerPawnChangedBindings.find(BindingId);
        if (It == GLuaPlayerPawnChangedBindings.end() || It->second.Target.Get() != Target)
        {
            return false;
        }

        RemoveLuaPlayerPawnChangedBinding(BindingId);
        return true;
    }

    FString MakeLuaReflectedEventKey(const UObject* Object, const FFunction& Function)
    {
        if (!Object)
        {
            return {};
        }
        return std::to_string(Object->GetUUID()) + "::" + Function.GetSignature();
    }

    void CompactLuaReflectedEventOverrides()
    {
        for (auto It = GLuaReflectedEventOverrides.begin(); It != GLuaReflectedEventOverrides.end();)
        {
            if (!It->second.Target.IsValid() || !It->second.Callback.valid())
            {
                It = GLuaReflectedEventOverrides.erase(It);
                continue;
            }
            ++It;
        }
    }
}

void FLuaScriptManager::Initialize()
{
    Lua = std::make_unique<sol::state>();
    Lua->open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::coroutine, sol::lib::debug);
    (*Lua)["package"]["path"] = FPaths::ToUtf8(FPaths::Combine(FPaths::ScriptDir(), L"?.lua").c_str());

    // 한글 경로 호환을 위해 require 의 파일 검색을 wide-aware 로 교체.
    // Lua 5.2+ 는 package.searchers, Lua 5.1/LuaJIT 은 package.loaders 를 사용한다.
    sol::table  Package       = (*Lua)["package"];
    sol::object Searchers     = Package["searchers"];
    sol::table  ModuleLoaders = Searchers.valid() && Searchers.get_type() == sol::type::table
            ? Searchers.as<sol::table>()
            : Package["loaders"].get<sol::table>();
    ModuleLoaders[2] = [](sol::this_state ts, const std::string& ModName) -> sol::object
    {
        sol::state_view    L(ts);
        const std::wstring WidePath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ModName + ".lua"));
        std::error_code    EC;
        if (!std::filesystem::exists(WidePath, EC))
        {
            return sol::make_object(L, std::string("\n\tno file '") + FPaths::ToUtf8(WidePath) + "'");
        }

        FString Content;
        if (!ReadScriptFileContent(ModName + ".lua", Content))
        {
            return sol::make_object(L, std::string("\n\tcannot read '") + FPaths::ToUtf8(WidePath) + "'");
        }

        const FString    ChunkName = FPaths::ToUtf8(WidePath);
        sol::load_result LR        = L.load(Content, ChunkName);
        if (!LR.valid())
        {
            sol::error Err = LR;
            return sol::make_object(L, std::string("\n\t") + Err.what());
        }
        return LR.get<sol::object>();
    };

    // 모든 sol::protected_function 호출의 default error handler 를 debug.traceback 으로 설정.
    // RegisterBindings 안에서 helper Lua 파일을 로드하기 전에 먼저 걸어야 helper load error 도 stacktrace 를 갖는다.
    if (sol::object DebugObject = (*Lua)["debug"]; DebugObject.valid() && DebugObject.get_type() == sol::type::table)
    {
        sol::table  DebugTable      = DebugObject.as<sol::table>();
        sol::object TracebackObject = DebugTable["traceback"];
        if (TracebackObject.valid() && TracebackObject.get_type() == sol::type::function)
        {
            sol::protected_function::set_default_handler(TracebackObject.as<sol::function>());
        }
    }

    FLuaDebugManager::Initialize(Lua->lua_state());
    FLuaDebugManager::RegisterLuaBindings(*Lua);
    RegisterBindings(*Lua);

    FWatchID WatchID = FDirectoryWatcher::Get().Watch(FPaths::ScriptDir(), "");
    if (WatchID != 0)
    {
        WatchSub = FDirectoryWatcher::Get().Subscribe(
            WatchID,
            [](const TSet<FString>& Files)
            {
                FLuaScriptManager::OnScriptsChanged(Files);
            }
        );
    }
}

void FLuaScriptManager::Shutdown()
{
    if (WatchSub != 0)
    {
        FDirectoryWatcher::Get().Unsubscribe(WatchSub);
        WatchSub = 0;
    }

    TArray<TWeakObjectPtr<ULuaScriptComponent>> ComponentsToRelease;
    TArray<TWeakObjectPtr<ULuaAnimInstance>>    AnimInstancesToRelease;
    {
        std::lock_guard<std::mutex> Lock(ComponentMutex);
        ComponentsToRelease    = RegisteredComponents;
        AnimInstancesToRelease = RegisteredAnimInstances;
    }

    // lua_State 가 살아있는 동안 런타임 객체들이 들고 있는 sol reference 를 먼저 해제한다.
    // 이 작업을 Lua.reset() 뒤로 미루면 GC/dtor 단계에서 sol::basic_reference::~basic_reference 가
    // 닫힌 lua_State 에 luaL_unref 를 호출하며 lua51.dll 내부에서 크래시난다.
    for (const TWeakObjectPtr<ULuaScriptComponent>& ComponentPtr : ComponentsToRelease)
    {
        if (ULuaScriptComponent* Component = ComponentPtr.GetEvenIfPendingKill())
        {
            if (IsAliveObject(Component))
            {
                Component->ReleaseLuaRuntimeForShutdown();
            }
        }
    }
    for (const TWeakObjectPtr<ULuaAnimInstance>& InstancePtr : AnimInstancesToRelease)
    {
        if (ULuaAnimInstance* Instance = InstancePtr.GetEvenIfPendingKill())
        {
            if (IsAliveObject(Instance))
            {
                Instance->ReleaseLuaRuntimeForShutdown();
            }
        }
    }

    // ULuaBlueprintComponent 는 ULuaScriptComponent/ULuaAnimInstance 와 달리 별도 레지스트리에
    // 등록되지 않으므로, 전역 객체 배열을 훑어 lua_State 가 살아있는 동안 sol 핸들을 해제한다.
    // (누락 시 FEngineLoop::Shutdown 의 최종 GC sweep → BeginDestroy → ClearLuaRuntime 이
    //  이미 닫힌 lua_State 에 luaL_unref 를 호출하며 lua51.dll 에서 크래시)
    for (UObject* Obj : GUObjectArray)
    {
        if (!IsAliveObject(Obj) || !Obj->IsA<ULuaBlueprintComponent>())
        {
            continue;
        }
        static_cast<ULuaBlueprintComponent*>(Obj)->ReleaseLuaRuntimeForShutdown();
    }

    {
        std::lock_guard<std::mutex> Lock(ComponentMutex);
        RegisteredComponents.clear();
        RegisteredAnimInstances.clear();
    }

    // 등록된 Lua 콜백 (sol::protected_function 들) 을 lua_State 가 살아있는 동안 먼저 release.
    // static 멤버라 프로그램 종료 시점까지 살아있는데, 그때 destructor 가 luaL_unref 를
    // 호출하면서 이미 reset 된 lua_State 를 만지면 크래시. 빈 함수로 덮어써 deref 를 지금
    // (Lua 가 valid 한 동안) 일으킨다.
    OnEscapePressedCallback = sol::protected_function();
    ClearLuaHealthChangedBindings();
    ClearLuaPostureChangedBindings();
    ClearLuaPlayerPawnChangedBindings();
    GLuaReflectedEventOverrides.clear();

    FLuaDebugManager::Shutdown();
    Lua.reset();
}

FString FLuaScriptManager::ResolveScriptPath(const FString& ScriptFile)
{
    std::wstring FullPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
    return FPaths::ToUtf8(FullPath);
}

bool FLuaScriptManager::OpenOrCreateScript(const FString& ScriptFile)
{
    std::wstring FullPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
    if (!std::filesystem::exists(FullPath))
    {
        FPaths::CreateDir(FPaths::ScriptDir());

        const std::wstring TemplatePath = FPaths::Combine(FPaths::ScriptDir(), L"template.lua");
        std::error_code    Error;
        if (std::filesystem::exists(TemplatePath))
        {
            std::filesystem::copy_file(TemplatePath, FullPath, std::filesystem::copy_options::none, Error);
            if (Error)
            {
                UE_LOG("Failed to copy Lua script template: %s", Error.message().c_str());
            }
        }

        if (!std::filesystem::exists(FullPath))
        {
            std::ofstream Out(FullPath);
            if (!Out)
            {
                return false;
            }
        }
    }

    HINSTANCE HInst = ShellExecuteW(nullptr, L"open", FullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    if ((INT_PTR)HInst <= 32)
    {
        return false;
    }

    return true;
}

bool FLuaScriptManager::ReadScriptFileContent(const FString& ScriptFile, FString& OutContent)
{
    const std::wstring WidePath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
    std::ifstream      File(WidePath.c_str(), std::ios::binary);
    if (!File.is_open())
    {
        return false;
    }
    std::ostringstream SS;
    SS << File.rdbuf();
    OutContent = SS.str();
    return true;
}

bool FLuaScriptManager::IsInitialized()
{
    return Lua != nullptr;
}

void FLuaScriptManager::SetOnEscapePressed(sol::protected_function Callback)
{
    OnEscapePressedCallback = std::move(Callback);
}

void FLuaScriptManager::FireOnEscapePressed()
{
    if (!OnEscapePressedCallback.valid())
    {
        return;
    }
    FScopedGarbageCollectionBlocker GCBlocker;
    sol::protected_function_result  Result = OnEscapePressedCallback();
    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG("[Lua] OnEscapePressed callback error: %s", Err.what());
    }
}

void FLuaScriptManager::FireWorldReset()
{
    if (!Lua) return;

    ClearLuaHealthChangedBindings();
    ClearLuaPostureChangedBindings();
    ClearLuaPlayerPawnChangedBindings();

    // require 로 한 번 로드된 모듈 테이블은 package.loaded 에 캐시된다. 씬 전환 시에도
    // 살아남기 때문에, 이 두 모듈이 보유한 죽은-월드 참조를 비워준다.
    sol::table Loaded = (*Lua)["package"]["loaded"];
    if (!Loaded.valid()) return;

    // 1) CoroutineManager — 옛 액터의 lua 클로저가 캡처한 환경의 obj 가 dangling.
    //    Wait(30) 도중에 씬 전환되면 새 월드 Tick 에서 만료되면서 freed AActor* deref.
    if (sol::object Coro = Loaded["CoroutineManager"]; Coro.valid() && Coro.get_type() == sol::type::table)
    {
        Coro.as<sol::table>()["coroutines"] = Lua->create_table();
    }

    // 2) ObjRegistry — 액터 핸들 캐시. 새 월드의 BeginPlay 가 다시 등록해줄 때까지 nil 로.
    if (sol::object Reg = Loaded["ObjRegistry"]; Reg.valid() && Reg.get_type() == sol::type::table)
    {
        sol::table T    = Reg.as<sol::table>();
        T["car"]        = sol::nil;
        T["carCamera"]  = sol::nil;
        T["carGas"]     = sol::nil;
        T["manObj"]     = sol::nil;
        T["manCamera"]  = sol::nil;
        T["gasNozzle"]  = sol::nil;
        T["carWasher"]  = sol::nil;
        T["dirtyCar"]   = sol::nil;
        T["policeCars"] = Lua->create_table();
    }
}

void FLuaScriptManager::RegisterComponent(ULuaScriptComponent* Component)
{
    if (!IsAliveObject(Component)) return;

    std::lock_guard<std::mutex> Lock(ComponentMutex);
    for (const TWeakObjectPtr<ULuaScriptComponent>& Existing : RegisteredComponents)
    {
        if (Existing.Get() == Component)
        {
            return;
        }
    }
    RegisteredComponents.push_back(Component);
}

void FLuaScriptManager::UnregisterComponent(ULuaScriptComponent* Component)
{
    if (!Component) return;

    std::lock_guard<std::mutex> Lock(ComponentMutex);
    RegisteredComponents.erase(
        std::remove_if(
            RegisteredComponents.begin(),
            RegisteredComponents.end(),
            [Component](const TWeakObjectPtr<ULuaScriptComponent>& Existing)
            {
                return Existing.Get() == Component || !Existing.IsValid();
            }
        ),
        RegisteredComponents.end()
    );
}

void FLuaScriptManager::RegisterAnimInstance(ULuaAnimInstance* Instance)
{
    if (!IsAliveObject(Instance)) return;
    std::lock_guard<std::mutex> Lock(ComponentMutex);
    for (const TWeakObjectPtr<ULuaAnimInstance>& Existing : RegisteredAnimInstances)
    {
        if (Existing.Get() == Instance)
        {
            return;
        }
    }
    RegisteredAnimInstances.push_back(Instance);
}

void FLuaScriptManager::UnregisterAnimInstance(ULuaAnimInstance* Instance)
{
    if (!Instance) return;
    std::lock_guard<std::mutex> Lock(ComponentMutex);
    RegisteredAnimInstances.erase(
        std::remove_if(
            RegisteredAnimInstances.begin(),
            RegisteredAnimInstances.end(),
            [Instance](const TWeakObjectPtr<ULuaAnimInstance>& Existing)
            {
                return Existing.Get() == Instance || !Existing.IsValid();
            }
        ),
        RegisteredAnimInstances.end()
    );
}

void FLuaScriptManager::OnScriptsChanged(const TSet<FString>& ChangedFiles)
{
    TSet<ULuaScriptComponent*> Targets;

    InvalidateChangedModules(ChangedFiles);

    {
        std::lock_guard<std::mutex> Lock(ComponentMutex);
        RegisteredComponents.erase(
            std::remove_if(
                RegisteredComponents.begin(),
                RegisteredComponents.end(),
                [](const TWeakObjectPtr<ULuaScriptComponent>& Component)
                {
                    return !Component.IsValid();
                }
            ),
            RegisteredComponents.end()
        );
        for (const TWeakObjectPtr<ULuaScriptComponent>& ComponentPtr : RegisteredComponents)
        {
            ULuaScriptComponent* Component = ComponentPtr.Get();
            if (!IsValid(Component)) continue;

            const FString& ScriptFile = Component->GetScriptFile();
            if (ScriptFile.empty()) continue;

            for (const FString& File : ChangedFiles)
            {
                if (File == ScriptFile)
                {
                    Targets.insert(Component);
                    break;
                }
            }
        }
    }

    for (ULuaScriptComponent* Component : Targets)
    {
        if (!Component) continue;

        UE_LOG("[LuaHotReload] Reloading: %s", Component->GetScriptFile().c_str());
        FNotificationManager::Get().AddNotification("Lua Reloaded: " + Component->GetScriptFile(), ENotificationType::Success, 3.0f);
        Component->ReloadScript();
    }

    // AnimInstance 측도 같은 패턴 — 매칭되는 ScriptFile 의 인스턴스 reload.
    TSet<ULuaAnimInstance*> AnimTargets;
    {
        std::lock_guard<std::mutex> Lock(ComponentMutex);
        RegisteredAnimInstances.erase(
            std::remove_if(
                RegisteredAnimInstances.begin(),
                RegisteredAnimInstances.end(),
                [](const TWeakObjectPtr<ULuaAnimInstance>& Instance)
                {
                    return !Instance.IsValid();
                }
            ),
            RegisteredAnimInstances.end()
        );
        for (const TWeakObjectPtr<ULuaAnimInstance>& InstPtr : RegisteredAnimInstances)
        {
            ULuaAnimInstance* Inst = InstPtr.Get();
            if (!IsValid(Inst)) continue;
            const FString& AnimScript = Inst->ScriptFile;
            if (AnimScript.empty()) continue;
            for (const FString& File : ChangedFiles)
            {
                if (File == AnimScript)
                {
                    AnimTargets.insert(Inst);
                    break;
                }
            }
        }
    }
    for (ULuaAnimInstance* Inst : AnimTargets)
    {
        if (!Inst) continue;
        UE_LOG("[LuaHotReload] Reloading Anim: %s", Inst->ScriptFile.c_str());
        FNotificationManager::Get().AddNotification("Anim Reloaded: " + Inst->ScriptFile, ENotificationType::Success, 3.0f);
        Inst->ReloadScript();
    }
}

void FLuaScriptManager::InvalidateChangedModules(const TSet<FString>& ChangedFiles)
{
    if (!Lua) return;

    sol::table Loaded = (*Lua)["package"]["loaded"];
    if (!Loaded.valid()) return;

    for (const FString& File : ChangedFiles)
    {
        FString ModuleName = GetModuleNameFromPath(File);
        if (ModuleName.empty()) continue;

        Loaded[ModuleName] = sol::nil;
        UE_LOG("[LuaHotReload] Invalidated module: %s", ModuleName.c_str());
    }
}

FString FLuaScriptManager::GetModuleNameFromPath(const FString& ScriptPath)
{
    if (ScriptPath.empty())
    {
        return {};
    }

    FString Normalized = ScriptPath;
    for (char& Ch : Normalized)
    {
        if (Ch == '\\')
        {
            Ch = '/';
        }
    }

    constexpr const char* LuaExt = ".lua";
    if (Normalized.size() <= 4 || Normalized.substr(Normalized.size() - 4) != LuaExt)
    {
        return {};
    }

    Normalized.erase(Normalized.size() - 4);
    for (char& Ch : Normalized)
    {
        if (Ch == '/')
        {
            Ch = '.';
        }
    }

    return Normalized;
}

namespace
{
    const char* LuaPropertyTypeName(EPropertyType Type)
    {
        switch (Type)
        {
        case EPropertyType::Bool:
            return "Bool";
        case EPropertyType::ByteBool:
            return "ByteBool";
        case EPropertyType::Int:
            return "Int";
        case EPropertyType::Float:
            return "Float";
        case EPropertyType::Vec3:
            return "Vec3";
        case EPropertyType::Vec4:
            return "Vec4";
        case EPropertyType::Rotator:
            return "Rotator";
        case EPropertyType::String:
            return "String";
        case EPropertyType::Name:
            return "Name";
        case EPropertyType::ObjectRef:
            return "ObjectRef";
        case EPropertyType::Color4:
            return "Color4";
        case EPropertyType::ClassRef:
            return "ClassRef";
        case EPropertyType::Enum:
            return "Enum";
        case EPropertyType::Struct:
            return "Struct";
        case EPropertyType::SoftObjectRef:
            return "SoftObjectRef";
        case EPropertyType::Array:
            return "Array";
        default:
            return "Unknown";
        }
    }

    bool LuaReadNumber(const sol::object& Object, double& OutValue)
    {
        if (!Object.valid() || Object == sol::nil || Object.get_type() != sol::type::number)
        {
            return false;
        }
        OutValue = Object.as<double>();
        return true;
    }

    bool LuaReadFloatField(const sol::table& Table, const char* Name, int Index, float& OutValue)
    {
        double      Number = 0.0;
        sol::object Named  = Table[Name];
        if (LuaReadNumber(Named, Number))
        {
            OutValue = static_cast<float>(Number);
            return true;
        }
        sol::object Indexed = Table[Index];
        if (LuaReadNumber(Indexed, Number))
        {
            OutValue = static_cast<float>(Number);
            return true;
        }
        return false;
    }

    bool LuaObjectToVector(const sol::object& Object, FVector& OutVector)
    {
        if (!Object.valid() || Object == sol::nil)
        {
            return false;
        }
        if (Object.is<FVector>())
        {
            OutVector = Object.as<FVector>();
            return true;
        }
        if (Object.get_type() != sol::type::table)
        {
            return false;
        }
        sol::table Table = Object.as<sol::table>();
        float      X     = 0.0f;
        float      Y     = 0.0f;
        float      Z     = 0.0f;
        LuaReadFloatField(Table, "X", 1, X);
        LuaReadFloatField(Table, "Y", 2, Y);
        LuaReadFloatField(Table, "Z", 3, Z);
        OutVector = FVector(X, Y, Z);
        return true;
    }

    bool LuaObjectToVector4(const sol::object& Object, FVector4& OutVector)
    {
        if (!Object.valid() || Object == sol::nil)
        {
            return false;
        }
        if (Object.is<FVector4>())
        {
            OutVector = Object.as<FVector4>();
            return true;
        }
        if (Object.get_type() != sol::type::table)
        {
            return false;
        }
        sol::table Table = Object.as<sol::table>();
        float      X     = 0.0f;
        float      Y     = 0.0f;
        float      Z     = 0.0f;
        float      W     = 0.0f;
        LuaReadFloatField(Table, "X", 1, X);
        LuaReadFloatField(Table, "Y", 2, Y);
        LuaReadFloatField(Table, "Z", 3, Z);
        if (!LuaReadFloatField(Table, "W", 4, W))
        {
            LuaReadFloatField(Table, "A", 4, W);
        }
        OutVector = FVector4(X, Y, Z, W);
        return true;
    }

    sol::table LuaVector4ToTable(sol::this_state State, const FVector4& Value)
    {
        sol::state_view Lua(State);
        sol::table      Table = Lua.create_table();
        Table["X"]            = Value.X;
        Table["Y"]            = Value.Y;
        Table["Z"]            = Value.Z;
        Table["W"]            = Value.W;
        Table["R"]            = Value.R;
        Table["G"]            = Value.G;
        Table["B"]            = Value.B;
        Table["A"]            = Value.A;
        return Table;
    }

    sol::object LuaValueToObject(sol::this_state State, const FProperty& Property, void* ValuePtr);
    bool        LuaObjectToValue(
        const FProperty&   Property,
        void*              ValuePtr,
        const sol::object& Object,
        FString*           OutError = nullptr
    );

    bool LuaObjectToEnumValue(const FEnum* EnumType, const sol::object& Object, int64& OutValue)
    {
        if (!Object.valid() || Object == sol::nil)
        {
            return false;
        }
        if (Object.get_type() == sol::type::number)
        {
            OutValue = static_cast<int64>(Object.as<double>());
            return true;
        }
        if (Object.get_type() == sol::type::string)
        {
            FString Name = Object.as<FString>();
            if (!EnumType)
            {
                return false;
            }
            for (uint32 Index = 0; Index < EnumType->GetCount(); ++Index)
            {
                const char* EntryName = EnumType->GetNameAt(Index);
                if (EntryName && Name == EntryName)
                {
                    OutValue = EnumType->GetValueAt(Index);
                    return true;
                }
            }
        }
        return false;
    }

    sol::object LuaStructToTable(sol::this_state State, const FStructProperty& Property, void* ValuePtr)
    {
        sol::state_view Lua(State);
        sol::table      Table      = Lua.create_table();
        UStruct*        StructType = Property.GetStructType();
        if (!StructType || !ValuePtr)
        {
            return sol::make_object(Lua, Table);
        }
        TArray<const FProperty*> Children;
        StructType->GetPropertyRefs(Children);
        for (const FProperty* Child : Children)
        {
            if (!Child || !Child->Name)
            {
                continue;
            }
            Table[Child->Name] = LuaValueToObject(State, *Child, Child->GetValuePtrFor(ValuePtr));
        }
        return sol::make_object(Lua, Table);
    }

    bool LuaTableToStruct(const FStructProperty& Property, void* ValuePtr, const sol::object& Object, FString* OutError)
    {
        if (!ValuePtr)
        {
            if (OutError) *OutError = "null struct storage";
            return false;
        }
        if (Object.get_type() != sol::type::table)
        {
            if (OutError) *OutError = "expected table for struct";
            return false;
        }
        UStruct* StructType = Property.GetStructType();
        if (!StructType)
        {
            if (OutError) *OutError = "struct type is not registered";
            return false;
        }
        sol::table               Table = Object.as<sol::table>();
        TArray<const FProperty*> Children;
        StructType->GetPropertyRefs(Children);
        for (const FProperty* Child : Children)
        {
            if (!Child || !Child->Name)
            {
                continue;
            }
            sol::object Field = Table[Child->Name];
            if (!Field.valid() || Field == sol::nil)
            {
                continue;
            }
            if (!LuaObjectToValue(*Child, Child->GetValuePtrFor(ValuePtr), Field, OutError))
            {
                return false;
            }
        }
        return true;
    }

    sol::object LuaArrayToTable(sol::this_state State, const FArrayProperty& Property, void* ValuePtr)
    {
        sol::state_view                  Lua(State);
        sol::table                       Table = Lua.create_table();
        const FArrayProperty::FArrayOps* Ops   = Property.GetArrayOps();
        const FProperty*                 Inner = Property.GetInnerProperty();
        if (!ValuePtr || !Ops || !Ops->GetNum || !Ops->GetElementPtr || !Inner)
        {
            return sol::make_object(Lua, Table);
        }
        const size_t Count = Ops->GetNum(ValuePtr);
        for (size_t Index = 0; Index < Count; ++Index)
        {
            Table[static_cast<int>(Index + 1)] = LuaValueToObject(State, *Inner, Ops->GetElementPtr(ValuePtr, Index));
        }
        return sol::make_object(Lua, Table);
    }

    bool LuaTableToArray(const FArrayProperty& Property, void* ValuePtr, const sol::object& Object, FString* OutError)
    {
        if (Object.get_type() != sol::type::table)
        {
            if (OutError) *OutError = "expected table for array";
            return false;
        }
        const FArrayProperty::FArrayOps* Ops   = Property.GetArrayOps();
        const FProperty*                 Inner = Property.GetInnerProperty();
        if (!ValuePtr || !Ops || !Ops->Resize || !Ops->GetElementPtr || !Inner)
        {
            if (OutError) *OutError = "array reflection ops are missing";
            return false;
        }
        sol::table   Table = Object.as<sol::table>();
        const size_t Count = static_cast<size_t>(Table.size());
        Ops->Resize(ValuePtr, Count);
        for (size_t Index = 0; Index < Count; ++Index)
        {
            sol::object Element = Table[static_cast<int>(Index + 1)];
            if (!LuaObjectToValue(*Inner, Ops->GetElementPtr(ValuePtr, Index), Element, OutError))
            {
                return false;
            }
        }
        return true;
    }

    sol::object LuaValueToObject(sol::this_state State, const FProperty& Property, void* ValuePtr)
    {
        sol::state_view Lua(State);
        if (!ValuePtr)
        {
            return sol::make_object(Lua, sol::nil);
        }

        switch (Property.GetType())
        {
        case EPropertyType::Bool:
            return sol::make_object(Lua, *static_cast<bool*>(ValuePtr));
        case EPropertyType::ByteBool:
            return sol::make_object(Lua, *static_cast<uint8*>(ValuePtr) != 0);
        case EPropertyType::Int:
            return sol::make_object(Lua, *static_cast<int32*>(ValuePtr));
        case EPropertyType::Float:
            return sol::make_object(Lua, *static_cast<float*>(ValuePtr));
        case EPropertyType::String:
            return sol::make_object(Lua, *static_cast<FString*>(ValuePtr));
        case EPropertyType::Name:
            return sol::make_object(Lua, static_cast<FName*>(ValuePtr)->ToString());
        case EPropertyType::Vec3:
            return sol::make_object(Lua, *static_cast<FVector*>(ValuePtr));
        case EPropertyType::Rotator:
            return sol::make_object(Lua, static_cast<FRotator*>(ValuePtr)->ToVector());
        case EPropertyType::Vec4:
        case EPropertyType::Color4:
            return sol::make_object(Lua, LuaVector4ToTable(State, *static_cast<FVector4*>(ValuePtr)));
        case EPropertyType::Enum:
        {
            const FEnum* EnumType  = Property.GetEnumType();
            int64        EnumValue = 0;
            std::memcpy(&EnumValue, ValuePtr, std::min<size_t>(Property.Size, sizeof(EnumValue)));
            if (EnumType)
            {
                const char* Name = EnumType->GetNameByValue(EnumValue);
                if (Name && std::strcmp(Name, "Unknown") != 0)
                {
                    return sol::make_object(Lua, FString(Name));
                }
            }
            return sol::make_object(Lua, EnumValue);
        }
        case EPropertyType::ObjectRef:
        {
            const FObjectProperty* ObjectProperty = Property.AsObjectProperty();
            return sol::make_object(
                Lua,
                ObjectProperty ? ObjectProperty->GetObjectValueFromValuePtr(ValuePtr) : nullptr
            );
        }
        case EPropertyType::ClassRef:
        {
            const FClassProperty* ClassProperty = Property.AsClassProperty();
            UClass*               Class         = ClassProperty ? ClassProperty->GetClassValueFromValuePtr(ValuePtr) : nullptr;
            return Class ? sol::make_object(Lua, FString(Class->GetName())) : sol::make_object(Lua, sol::nil);
        }
        case EPropertyType::SoftObjectRef:
        {
            const FSoftObjectProperty* SoftProperty = Property.AsSoftObjectProperty();
            return SoftProperty ? sol::make_object(Lua, SoftProperty->GetPathFromValuePtr(ValuePtr))
                    : sol::make_object(Lua, sol::nil);
        }
        case EPropertyType::Struct:
        {
            const FStructProperty* StructProperty = Property.AsStructProperty();
            return StructProperty ? LuaStructToTable(State, *StructProperty, ValuePtr)
                    : sol::make_object(Lua, sol::nil);
        }
        case EPropertyType::Array:
        {
            const FArrayProperty* ArrayProperty = Property.AsArrayProperty();
            return ArrayProperty ? LuaArrayToTable(State, *ArrayProperty, ValuePtr) : sol::make_object(Lua, sol::nil);
        }
        default:
            return sol::make_object(Lua, sol::nil);
        }
    }

    bool LuaObjectToValue(const FProperty& Property, void* ValuePtr, const sol::object& Object, FString* OutError)
    {
        if (!ValuePtr)
        {
            if (OutError) *OutError = "null value storage";
            return false;
        }
        if (!Object.valid() || Object == sol::nil)
        {
            if (Property.GetType() == EPropertyType::ObjectRef)
            {
                if (const FObjectProperty* ObjectProperty = Property.AsObjectProperty())
                {
                    ObjectProperty->SetObjectValueFromValuePtr(ValuePtr, nullptr);
                    return true;
                }
            }
            if (Property.GetType() == EPropertyType::ClassRef)
            {
                if (const FClassProperty* ClassProperty = Property.AsClassProperty())
                {
                    ClassProperty->SetClassValueFromValuePtr(ValuePtr, nullptr);
                    return true;
                }
            }
            if (OutError) *OutError = FString("nil is not assignable to ") + LuaPropertyTypeName(Property.GetType());
            return false;
        }

        switch (Property.GetType())
        {
        case EPropertyType::Bool:
            if (!Object.is<bool>())
            {
                if (OutError) *OutError = "expected bool";
                return false;
            }
            *static_cast<bool*>(ValuePtr) = Object.as<bool>();
            return true;
        case EPropertyType::ByteBool:
            if (!Object.is<bool>())
            {
                if (OutError) *OutError = "expected bool";
                return false;
            }
            *static_cast<uint8*>(ValuePtr) = Object.as<bool>() ? 1 : 0;
            return true;
        case EPropertyType::Int:
            if (Object.get_type() != sol::type::number)
            {
                if (OutError) *OutError = "expected number";
                return false;
            }
            *static_cast<int32*>(ValuePtr) = static_cast<int32>(Object.as<double>());
            return true;
        case EPropertyType::Float:
            if (Object.get_type() != sol::type::number)
            {
                if (OutError) *OutError = "expected number";
                return false;
            }
            *static_cast<float*>(ValuePtr) = static_cast<float>(Object.as<double>());
            return true;
        case EPropertyType::String:
            if (Object.get_type() != sol::type::string)
            {
                if (OutError) *OutError = "expected string";
                return false;
            }
            *static_cast<FString*>(ValuePtr) = Object.as<FString>();
            return true;
        case EPropertyType::Name:
            if (Object.get_type() != sol::type::string)
            {
                if (OutError) *OutError = "expected string";
                return false;
            }
            *static_cast<FName*>(ValuePtr) = FName(Object.as<FString>());
            return true;
        case EPropertyType::Vec3:
        {
            FVector Vector;
            if (!LuaObjectToVector(Object, Vector))
            {
                if (OutError) *OutError = "expected Vector or {X,Y,Z}";
                return false;
            }
            *static_cast<FVector*>(ValuePtr) = Vector;
            return true;
        }
        case EPropertyType::Rotator:
        {
            FVector Vector;
            if (!LuaObjectToVector(Object, Vector))
            {
                if (OutError) *OutError = "expected Vector or {X,Y,Z}";
                return false;
            }
            *static_cast<FRotator*>(ValuePtr) = FRotator(Vector);
            return true;
        }
        case EPropertyType::Vec4:
        case EPropertyType::Color4:
        {
            FVector4 Vector;
            if (!LuaObjectToVector4(Object, Vector))
            {
                if (OutError) *OutError = "expected {X,Y,Z,W}";
                return false;
            }
            *static_cast<FVector4*>(ValuePtr) = Vector;
            return true;
        }
        case EPropertyType::Enum:
        {
            int64 EnumValue = 0;
            if (!LuaObjectToEnumValue(Property.GetEnumType(), Object, EnumValue))
            {
                if (OutError) *OutError = "expected enum name or value";
                return false;
            }
            std::memcpy(ValuePtr, &EnumValue, std::min<size_t>(Property.Size, sizeof(EnumValue)));
            return true;
        }
        case EPropertyType::ObjectRef:
        {
            if (!Object.is<UObject*>())
            {
                if (OutError) *OutError = "expected Object";
                return false;
            }
            UObject*               SourceObject   = Object.as<UObject*>();
            const FObjectProperty* ObjectProperty = Property.AsObjectProperty();
            if (!ObjectProperty)
            {
                if (OutError) *OutError = "object property ops missing";
                return false;
            }
            if (SourceObject && !IsValid(SourceObject))
            {
                if (OutError) *OutError = "object is no longer valid";
                return false;
            }
            if (UClass* AllowedClass = ObjectProperty->GetAllowedClassType())
            {
                if (SourceObject && !SourceObject->GetClass()->IsA(AllowedClass))
                {
                    if (OutError) *OutError = "object class is not assignable to parameter";
                    return false;
                }
            }
            ObjectProperty->SetObjectValueFromValuePtr(ValuePtr, SourceObject);
            return true;
        }
        case EPropertyType::ClassRef:
        {
            const FClassProperty* ClassProperty = Property.AsClassProperty();
            if (!ClassProperty)
            {
                if (OutError) *OutError = "class property ops missing";
                return false;
            }
            if (Object.get_type() != sol::type::string)
            {
                if (OutError) *OutError = "expected class name string";
                return false;
            }
            UClass* Class = UClass::FindByName(Object.as<FString>().c_str());
            if (!Class)
            {
                if (OutError) *OutError = "class was not found";
                return false;
            }
            if (UClass* AllowedClass = ClassProperty->GetAllowedClassType())
            {
                if (!Class->IsA(AllowedClass))
                {
                    if (OutError) *OutError = "class is not assignable to parameter";
                    return false;
                }
            }
            ClassProperty->SetClassValueFromValuePtr(ValuePtr, Class);
            return true;
        }
        case EPropertyType::SoftObjectRef:
        {
            const FSoftObjectProperty* SoftProperty = Property.AsSoftObjectProperty();
            if (!SoftProperty)
            {
                if (OutError) *OutError = "soft object property ops missing";
                return false;
            }
            if (Object.get_type() != sol::type::string)
            {
                if (OutError) *OutError = "expected asset path string";
                return false;
            }
            SoftProperty->SetPathFromValuePtr(ValuePtr, Object.as<FString>());
            return true;
        }
        case EPropertyType::Struct:
        {
            const FStructProperty* StructProperty = Property.AsStructProperty();
            return StructProperty ? LuaTableToStruct(*StructProperty, ValuePtr, Object, OutError) : false;
        }
        case EPropertyType::Array:
        {
            const FArrayProperty* ArrayProperty = Property.AsArrayProperty();
            return ArrayProperty ? LuaTableToArray(*ArrayProperty, ValuePtr, Object, OutError) : false;
        }
        default:
            if (OutError) *OutError = "unsupported reflected property type for Lua";
            return false;
        }
    }

    bool LuaCanSkipMissingArgument(const FProperty& Parameter)
    {
        if ((Parameter.Flags & PF_OutParm) != 0 && (Parameter.Flags & PF_ConstParm) == 0)
        {
            return true;
        }
        return Parameter.Metadata.find("defaultvalue") != Parameter.Metadata.end();
    }

    bool LuaTryPrepareFunctionCall(const FFunction& Function, sol::variadic_args Args, void* Storage, FString& OutError)
    {
        const TArray<FProperty*>& Parameters  = Function.GetParameters();
        const size_t              LuaArgCount = Args.size();
        size_t                    LuaArgIndex = 0;

        for (const FProperty* Parameter : Parameters)
        {
            if (!Parameter)
            {
                continue;
            }

            const bool bOutOnly = (Parameter->Flags & PF_OutParm) != 0 && (Parameter->Flags & PF_ConstParm) == 0;
            if (bOutOnly && LuaArgIndex >= LuaArgCount)
            {
                continue;
            }

            if (LuaArgIndex >= LuaArgCount)
            {
                if (LuaCanSkipMissingArgument(*Parameter))
                {
                    continue;
                }
                OutError = FString("missing Lua argument for parameter '") + (Parameter->Name ? Parameter->Name : "") +
                        "'";
                return false;
            }

            sol::object Arg = Args[static_cast<int>(LuaArgIndex)];
            FString     ConvertError;
            if (!LuaObjectToValue(*Parameter, Parameter->GetValuePtrFor(Storage), Arg, &ConvertError))
            {
                OutError = FString("parameter '") + (Parameter->Name ? Parameter->Name : "") + "': " + ConvertError;
                return false;
            }
            ++LuaArgIndex;
        }

        if (LuaArgIndex < LuaArgCount)
        {
            OutError = "too many Lua arguments";
            return false;
        }
        return true;
    }

    sol::object LuaCollectFunctionResult(sol::this_state State, const FFunction& Function, void* Storage)
    {
        sol::state_view          Lua(State);
        const FProperty*         ReturnProperty = Function.GetReturnProperty();
        TArray<const FProperty*> OutParameters;
        for (const FProperty* Parameter : Function.GetParameters())
        {
            if (Parameter && (Parameter->Flags & PF_OutParm) != 0 && (Parameter->Flags & PF_ConstParm) == 0)
            {
                OutParameters.push_back(Parameter);
            }
        }

        if (!ReturnProperty && OutParameters.empty())
        {
            return sol::make_object(Lua, true);
        }
        if (ReturnProperty && OutParameters.empty())
        {
            return LuaValueToObject(State, *ReturnProperty, ReturnProperty->GetValuePtrFor(Storage));
        }

        sol::table Result = Lua.create_table();
        if (ReturnProperty)
        {
            Result["ReturnValue"] = LuaValueToObject(State, *ReturnProperty, ReturnProperty->GetValuePtrFor(Storage));
        }
        for (const FProperty* Parameter : OutParameters)
        {
            Result[(Parameter->Name ? Parameter->Name : "")] = LuaValueToObject(
                State,
                *Parameter,
                Parameter->GetValuePtrFor(Storage)
            );
        }
        return sol::make_object(Lua, Result);
    }

    const FFunction* LuaFindFunctionByNameOrSignature(UStruct* TargetStruct, const FString& FunctionNameOrSignature)
    {
        if (!TargetStruct || FunctionNameOrSignature.empty())
        {
            return nullptr;
        }

        if (FunctionNameOrSignature.find('(') != FString::npos)
        {
            return TargetStruct->FindFunctionBySignature(FunctionNameOrSignature.c_str(), true);
        }

        return TargetStruct->FindFunctionByName(FunctionNameOrSignature.c_str(), true);
    }

    bool LuaCopyFunctionResultFromObject(
        sol::this_state    State,
        const FFunction&   Function,
        void*              Storage,
        const sol::object& ResultObject,
        FString&           OutError
    )
    {
        (void)State;
        const bool bResultIsTable = ResultObject.valid() && ResultObject.get_type() == sol::type::table;

        if (const FProperty* ReturnProperty = Function.GetReturnProperty())
        {
            sol::object ReturnValue = bResultIsTable ? ResultObject.as<sol::table>()["ReturnValue"].get<sol::object>()
                    : ResultObject;
            if (ReturnValue.valid() && ReturnValue != sol::nil)
            {
                FString ConvertError;
                if (!LuaObjectToValue(
                    *ReturnProperty,
                    ReturnProperty->GetValuePtrFor(Storage),
                    ReturnValue,
                    &ConvertError
                ))
                {
                    OutError = FString("return value: ") + ConvertError;
                    return false;
                }
            }
        }

        if (bResultIsTable)
        {
            sol::table ResultTable = ResultObject.as<sol::table>();
            for (const FProperty* Parameter : Function.GetParameters())
            {
                if (!Parameter || (Parameter->Flags & PF_OutParm) == 0 || (Parameter->Flags & PF_ConstParm) != 0)
                {
                    continue;
                }

                sol::object OutValue = ResultTable[Parameter->Name ? Parameter->Name : ""].get<sol::object>();
                if (!OutValue.valid() || OutValue == sol::nil)
                {
                    continue;
                }

                FString ConvertError;
                if (!LuaObjectToValue(*Parameter, Parameter->GetValuePtrFor(Storage), OutValue, &ConvertError))
                {
                    OutError = FString("out parameter '") + (Parameter->Name ? Parameter->Name : "") + "': " +
                            ConvertError;
                    return false;
                }
            }
        }

        return true;
    }

    enum class ELuaEventOverrideResult : uint8
    {
        NotBound,
        Invoked,
        Failed,
    };

    ELuaEventOverrideResult LuaTryInvokeReflectedEventOverride(
        sol::this_state  State,
        UObject*         Instance,
        const FFunction& Function,
        void*            Storage,
        FString&         OutError
    )
    {
        if (!IsValid(Instance) || !Function.HasAnyFunctionFlags(FUNC_Event))
        {
            return ELuaEventOverrideResult::NotBound;
        }

        CompactLuaReflectedEventOverrides();
        auto It = GLuaReflectedEventOverrides.find(MakeLuaReflectedEventKey(Instance, Function));
        if (It == GLuaReflectedEventOverrides.end() || It->second.Target.Get() != Instance || !It->second.Callback.valid())
        {
            return ELuaEventOverrideResult::NotBound;
        }

        TArray<sol::object> LuaArgs;
        for (const FProperty* Parameter : Function.GetParameters())
        {
            if (!Parameter)
            {
                continue;
            }
            const bool bOutOnly = (Parameter->Flags & PF_OutParm) != 0 && (Parameter->Flags & PF_ConstParm) == 0;
            if (bOutOnly)
            {
                continue;
            }
            LuaArgs.push_back(LuaValueToObject(State, *Parameter, Parameter->GetValuePtrFor(Storage)));
        }

        FScopedGarbageCollectionBlocker GCBlocker;
        sol::protected_function_result  Result = It->second.Callback(sol::as_args(LuaArgs));
        if (!Result.valid())
        {
            sol::error Err = Result;
            OutError       = Err.what();
            return ELuaEventOverrideResult::Failed;
        }

        sol::object ResultObject = Result.get<sol::object>();
        if (!LuaCopyFunctionResultFromObject(State, Function, Storage, ResultObject, OutError))
        {
            return ELuaEventOverrideResult::Failed;
        }

        return ELuaEventOverrideResult::Invoked;
    }

    sol::object LuaInvokeReflectedFunctionBySignature(
        sol::this_state    State,
        UObject*           Instance,
        UClass*            StaticClass,
        const FString&     Signature,
        sol::variadic_args Args
    )
    {
        FScopedGarbageCollectionBlocker GCBlocker;
        sol::state_view                 Lua(State);
        if (Instance && !IsValid(Instance))
        {
            UE_LOG("[LuaReflection] Reflection.CallSignature failed: target object is no longer valid");
            return sol::make_object(Lua, sol::nil);
        }
        UStruct* TargetStruct = Instance ? Instance->GetClass() : StaticClass;
        if (!TargetStruct)
        {
            UE_LOG("[LuaReflection] Reflection.CallSignature failed: target has no reflected class");
            return sol::make_object(Lua, sol::nil);
        }

        const FFunction* Function = TargetStruct->FindFunctionBySignature(Signature.c_str(), true);
        if (!Function)
        {
            UE_LOG("[LuaReflection] Reflection.CallSignature failed: function not found: %s", Signature.c_str());
            return sol::make_object(Lua, sol::nil);
        }
        if (!Instance && !Function->IsStatic())
        {
            UE_LOG(
                "[LuaReflection] Reflection.CallSignature failed: non-static function requires object instance: %s",
                Signature.c_str()
            );
            return sol::make_object(Lua, sol::nil);
        }

        void* Storage = Function->CreateParameterStorage();
        if (!Storage)
        {
            UE_LOG(
                "[LuaReflection] Reflection.CallSignature failed: failed to allocate parameter storage: %s",
                Signature.c_str()
            );
            return sol::make_object(Lua, sol::nil);
        }

        FString PrepareError;
        if (!LuaTryPrepareFunctionCall(*Function, Args, Storage, PrepareError))
        {
            Function->DestroyParameterStorage(Storage);
            UE_LOG("[LuaReflection] Reflection.CallSignature failed: %s: %s", Signature.c_str(), PrepareError.c_str());
            return sol::make_object(Lua, sol::nil);
        }

        FString                       EventError;
        const ELuaEventOverrideResult EventResult = LuaTryInvokeReflectedEventOverride(
            State,
            Instance,
            *Function,
            Storage,
            EventError
        );
        if (EventResult == ELuaEventOverrideResult::Failed)
        {
            Function->DestroyParameterStorage(Storage);
            UE_LOG(
                "[LuaReflection] Reflection.CallSignature event override failed: %s: %s",
                Signature.c_str(),
                EventError.c_str()
            );
            return sol::make_object(Lua, sol::nil);
        }

        if (EventResult == ELuaEventOverrideResult::NotBound)
        {
            const bool bInvoked = Function->IsStatic()
                    ? Function->Invoke(nullptr, Storage, nullptr)
                    : Instance->ProcessEvent(Function, Storage, nullptr);
            if (!bInvoked)
            {
                Function->DestroyParameterStorage(Storage);
                UE_LOG("[LuaReflection] Reflection.CallSignature failed: native invoke failed: %s", Signature.c_str());
                return sol::make_object(Lua, sol::nil);
            }
        }

        sol::object Result = LuaCollectFunctionResult(State, *Function, Storage);
        Function->DestroyParameterStorage(Storage);
        return Result;
    }

    bool LuaBindReflectedEventOverride(
        UObject*                Object,
        const FString&          FunctionNameOrSignature,
        sol::protected_function Callback
    )
    {
        if (!IsValid(Object) || !Object->GetClass() || !Callback.valid())
        {
            return false;
        }

        const FFunction* Function = LuaFindFunctionByNameOrSignature(Object->GetClass(), FunctionNameOrSignature);
        if (!Function || !Function->HasAnyFunctionFlags(FUNC_Event))
        {
            UE_LOG("[LuaReflection] BindEvent failed: reflected event not found: %s", FunctionNameOrSignature.c_str());
            return false;
        }

        FLuaReflectedEventOverride Override;
        Override.Target                                                          = Object;
        Override.Callback                                                        = std::move(Callback);
        GLuaReflectedEventOverrides[MakeLuaReflectedEventKey(Object, *Function)] = std::move(Override);
        CompactLuaReflectedEventOverrides();
        return true;
    }

    bool LuaUnbindReflectedEventOverride(UObject* Object, const FString& FunctionNameOrSignature)
    {
        if (!IsValid(Object) || !Object->GetClass())
        {
            return false;
        }

        const FFunction* Function = LuaFindFunctionByNameOrSignature(Object->GetClass(), FunctionNameOrSignature);
        if (!Function)
        {
            return false;
        }

        return GLuaReflectedEventOverrides.erase(MakeLuaReflectedEventKey(Object, *Function)) > 0;
    }

    bool LuaHasReflectedEventOverride(UObject* Object, const FString& FunctionNameOrSignature)
    {
        if (!IsValid(Object) || !Object->GetClass())
        {
            return false;
        }

        const FFunction* Function = LuaFindFunctionByNameOrSignature(Object->GetClass(), FunctionNameOrSignature);
        if (!Function)
        {
            return false;
        }

        CompactLuaReflectedEventOverrides();
        auto It = GLuaReflectedEventOverrides.find(MakeLuaReflectedEventKey(Object, *Function));
        return It != GLuaReflectedEventOverrides.end() && It->second.Target.Get() == Object && It->second.Callback.valid();
    }

    sol::object LuaInvokeReflectedFunction(
        sol::this_state    State,
        UObject*           Instance,
        UClass*            StaticClass,
        const FString&     FunctionName,
        sol::variadic_args Args
    )
    {
        FScopedGarbageCollectionBlocker GCBlocker;
        sol::state_view                 Lua(State);
        if (Instance && !IsValid(Instance))
        {
            UE_LOG("[LuaReflection] Reflection.Call failed: target object is no longer valid");
            return sol::make_object(Lua, sol::nil);
        }
        UStruct* TargetStruct = Instance ? Instance->GetClass() : StaticClass;
        if (!TargetStruct)
        {
            UE_LOG("[LuaReflection] Reflection.Call failed: target has no reflected class");
            return sol::make_object(Lua, sol::nil);
        }

        TArray<const FFunction*> Functions;
        TargetStruct->FindFunctionsByName(FunctionName.c_str(), Functions, true);
        if (Functions.empty())
        {
            //UE_LOG("[LuaReflection] Reflection.Call failed: function not found: %s", FunctionName.c_str());
            return sol::make_object(Lua, sol::nil);
        }

        FString LastError;
        for (const FFunction* Function : Functions)
        {
            if (!Function)
            {
                continue;
            }
            if (!Instance && !Function->IsStatic())
            {
                LastError = "non-static function requires object instance";
                continue;
            }

            void* Storage = Function->CreateParameterStorage();
            if (!Storage)
            {
                LastError = "failed to allocate parameter storage";
                continue;
            }

            FString PrepareError;
            if (!LuaTryPrepareFunctionCall(*Function, Args, Storage, PrepareError))
            {
                Function->DestroyParameterStorage(Storage);
                LastError = PrepareError;
                continue;
            }

            FString                       EventError;
            const ELuaEventOverrideResult EventResult = LuaTryInvokeReflectedEventOverride(
                State,
                Instance,
                *Function,
                Storage,
                EventError
            );
            if (EventResult == ELuaEventOverrideResult::Failed)
            {
                Function->DestroyParameterStorage(Storage);
                LastError = FString("event override failed: ") + EventError;
                continue;
            }

            if (EventResult == ELuaEventOverrideResult::NotBound)
            {
                const bool bInvoked = Function->IsStatic()
                        ? Function->Invoke(nullptr, Storage, nullptr)
                        : Instance->ProcessEvent(Function, Storage, nullptr);
                if (!bInvoked)
                {
                    Function->DestroyParameterStorage(Storage);
                    LastError = "native invoke failed";
                    continue;
                }
            }

            sol::object Result = LuaCollectFunctionResult(State, *Function, Storage);
            Function->DestroyParameterStorage(Storage);
            return Result;
        }

        UE_LOG(
            "[LuaReflection] Reflection.Call failed: no overload matched for %s: %s",
            FunctionName.c_str(),
            LastError.c_str()
        );
        return sol::make_object(Lua, sol::nil);
    }

    const FProperty* LuaFindProperty(UObject* Object, const FString& PropertyName)
    {
        if (!IsValid(Object) || !Object->GetClass())
        {
            return nullptr;
        }
        TArray<const FProperty*> Properties;
        Object->GetClass()->GetPropertyRefs(Properties);
        for (const FProperty* Property : Properties)
        {
            if (Property && Property->Name && PropertyName == Property->Name)
            {
                return Property;
            }
        }
        return nullptr;
    }

    sol::object LuaGetReflectedProperty(sol::this_state State, UObject* Object, const FString& PropertyName)
    {
        sol::state_view  Lua(State);
        const FProperty* Property = LuaFindProperty(Object, PropertyName);
        if (!Property)
        {
            return sol::make_object(Lua, sol::nil);
        }
        return LuaValueToObject(State, *Property, Property->GetValuePtrFor(Object));
    }

    bool LuaSetReflectedProperty(UObject* Object, const FString& PropertyName, sol::object Value)
    {
        const FProperty* Property = LuaFindProperty(Object, PropertyName);
        if (!IsValid(Object) || !Property)
        {
            return false;
        }
        FString    Error;
        const bool bOk = LuaObjectToValue(*Property, Property->GetValuePtrFor(Object), Value, &Error);
        if (!bOk)
        {
            UE_LOG(
                "[LuaReflection] SetProperty failed: %s.%s: %s",
                Object->GetClass()->GetName(),
                PropertyName.c_str(),
                Error.c_str()
            );
        }
        return bOk;
    }

    sol::table LuaDescribeProperty(sol::this_state State, const FProperty& Property)
    {
        sol::state_view Lua(State);
        sol::table      Desc = Lua.create_table();
        Desc["Name"]         = Property.Name ? Property.Name : "";
        Desc["DisplayName"]  = Property.DisplayName ? Property.DisplayName : (Property.Name ? Property.Name : "");
        Desc["Category"]     = Property.Category ? Property.Category : "";
        Desc["Type"]         = LuaPropertyTypeName(Property.GetType());
        Desc["Flags"]        = Property.Flags;
        Desc["OwnerClass"]   = Property.OwnerClassName ? Property.OwnerClassName : "";
        if (const FArrayProperty* ArrayProperty = Property.AsArrayProperty())
        {
            Desc["ElementType"] = LuaPropertyTypeName(ArrayProperty->GetElementType());
        }
        if (const FEnum* EnumType = Property.GetEnumType())
        {
            Desc["Enum"] = EnumType->GetName();
        }
        if (UStruct* StructType = Property.GetStructType())
        {
            Desc["Struct"] = StructType->GetName();
        }
        return Desc;
    }

    sol::table LuaDescribeFunction(sol::this_state State, const FFunction& Function)
    {
        sol::state_view Lua(State);
        sol::table      Desc = Lua.create_table();
        Desc["Name"]         = Function.GetName();
        Desc["Signature"]    = Function.GetSignature();
        Desc["DisplayName"]  = Function.GetDisplayName();
        Desc["Category"]     = Function.GetCategory();
        Desc["Flags"]        = Function.GetFlags();
        Desc["Const"]        = Function.IsConst();
        Desc["Static"]       = Function.IsStatic();
        Desc["OwnerClass"]   = Function.OwnerClassName ? Function.OwnerClassName : "";
        sol::table Params    = Lua.create_table();
        int        Index     = 1;
        for (const FProperty* Parameter : Function.GetParameters())
        {
            if (Parameter)
            {
                Params[Index++] = LuaDescribeProperty(State, *Parameter);
            }
        }
        Desc["Parameters"] = Params;
        if (const FProperty* ReturnProperty = Function.GetReturnProperty())
        {
            Desc["Return"] = LuaDescribeProperty(State, *ReturnProperty);
        }
        return Desc;
    }
}

sol::state& FLuaScriptManager::GetState()
{
    return *Lua;
}

void FLuaScriptManager::RegisterBindings(sol::state& Lua)
{
    RegisterLuaHelpers(Lua);
    RegisterCoreBindings(Lua);
    RegisterMathBindings(Lua);
    RegisterReflectionBindings(Lua);
    RegisterActorBindings(Lua);
    RegisterUIBindings(Lua);
}

FInputSystemSnapshot FLuaScriptManager::GetLuaInputSnapshot()
{
    if (GEngine)
    {
        if (UGameViewportClient* GameViewportClient = GEngine->GetGameViewportClient())
        {
            if (GameViewportClient->HasGameInputSnapshot())
            {
                return GameViewportClient->GetGameInputSnapshot();
            }
            return FInputSystemSnapshot {};
        }
    }

    return InputSystem::Get().MakeSnapshot();
}

void FLuaScriptManager::RegisterLuaHelpers(sol::state& Lua)
{
    // 한글 경로 호환 — safe_script_file 은 내부적으로 fopen(UTF-8) 을 쓰므로 ANSI 해석에서
    // 깨진다. wide ifstream 으로 직접 읽어 safe_script(string) 으로 실행.
    FString Content;
    if (!ReadScriptFileContent("CoroutineManager.lua", Content))
    {
        UE_LOG("[Lua] Failed to load CoroutineManager.lua");
        return;
    }
    const FString                  ChunkName = ResolveScriptPath("CoroutineManager.lua");
    sol::protected_function_result Result    = Lua.safe_script(Content, sol::script_pass_on_error, ChunkName);
    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG("[Lua] CoroutineManager.lua error: %s", Err.what());
    }
}

void FLuaScriptManager::RegisterCoreBindings(sol::state& Lua)
{
    Lua.set_function(
        "print",
        [](sol::variadic_args Args)
        {
            FString Message;

            for (auto Arg : Args)
            {
                if (!Message.empty())
                {
                    Message += "\t";
                }

                Message += Arg.as<FString>();
            }

            UE_LOG("[Lua] %s", Message.c_str());
        }
    );

    sol::table Input = Lua.create_named_table("Input");
    Input.set_function(
        "GetKeyDown",
        sol::overload(
            [](const FString& KeyName)
            {
                return GetLuaInputSnapshot().WasPressed(ResolveInputKeyCode(KeyName));
            },
            [](const FString& KeyName, int ControllerIndex)
            {
                return GetLuaInputSnapshot().WasPressed(ResolveInputKeyCode(KeyName), ControllerIndex);
            },
            [](int VK)
            {
                return GetLuaInputSnapshot().WasPressed(VK);
            },
            [](int VK, int ControllerIndex)
            {
                return GetLuaInputSnapshot().WasPressed(VK, ControllerIndex);
            }
        )
    );
    Input.set_function(
        "GetKey",
        sol::overload(
            [](const FString& KeyName)
            {
                return GetLuaInputSnapshot().IsDown(ResolveInputKeyCode(KeyName));
            },
            [](const FString& KeyName, int ControllerIndex)
            {
                return GetLuaInputSnapshot().IsDown(ResolveInputKeyCode(KeyName), ControllerIndex);
            },
            [](int VK)
            {
                return GetLuaInputSnapshot().IsDown(VK);
            },
            [](int VK, int ControllerIndex)
            {
                return GetLuaInputSnapshot().IsDown(VK, ControllerIndex);
            }
        )
    );
    Input.set_function(
        "GetKeyUp",
        sol::overload(
            [](const FString& KeyName)
            {
                return GetLuaInputSnapshot().WasReleased(ResolveInputKeyCode(KeyName));
            },
            [](const FString& KeyName, int ControllerIndex)
            {
                return GetLuaInputSnapshot().WasReleased(ResolveInputKeyCode(KeyName), ControllerIndex);
            },
            [](int VK)
            {
                return GetLuaInputSnapshot().WasReleased(VK);
            },
            [](int VK, int ControllerIndex)
            {
                return GetLuaInputSnapshot().WasReleased(VK, ControllerIndex);
            }
        )
    );
    Input.set_function(
        "GetMouseDeltaX",
        []()
        {
            return GetLuaInputSnapshot().MouseDeltaX;
        }
    );
    Input.set_function(
        "GetMouseDeltaY",
        []()
        {
            return GetLuaInputSnapshot().MouseDeltaY;
        }
    );

    // Engine — 게임 일시정지 / 종료.
    Input.set_function(
        "GetAxis",
        sol::overload(
            [](const FString& AxisName)
            {
                return GetLuaInputSnapshot().GetAxis(ResolveInputAxisCode(AxisName));
            },
            [](const FString& AxisName, int ControllerIndex)
            {
                return GetLuaInputSnapshot().GetAxis(ResolveInputAxisCode(AxisName), ControllerIndex);
            },
            [](int AxisCode)
            {
                return GetLuaInputSnapshot().GetAxis(AxisCode);
            },
            [](int AxisCode, int ControllerIndex)
            {
                return GetLuaInputSnapshot().GetAxis(AxisCode, ControllerIndex);
            }
        )
    );
    Input.set_function(
        "IsGamepadConnected",
        sol::overload(
            []()
            {
                return GetLuaInputSnapshot().IsGamepadConnected();
            },
            [](int ControllerIndex)
            {
                return GetLuaInputSnapshot().IsGamepadConnected(ControllerIndex);
            }
        )
    );
    Input.set_function(
        "PlayGamepadRumble",
        sol::overload(
            [](float LeftMotor, float RightMotor, float DurationSeconds)
            {
                InputSystem::Get().PlayGamepadRumble(LeftMotor, RightMotor, DurationSeconds);
            },
            [](float LeftMotor, float RightMotor, float DurationSeconds, int ControllerIndex)
            {
                InputSystem::Get().PlayGamepadRumble(LeftMotor, RightMotor, DurationSeconds, ControllerIndex);
            }
        )
    );
    Input.set_function(
        "StopGamepadRumble",
        sol::overload(
            []()
            {
                InputSystem::Get().StopGamepadRumble();
            },
            [](int ControllerIndex)
            {
                InputSystem::Get().StopGamepadRumble(ControllerIndex);
            }
        )
    );

    sol::table Engine = Lua.create_named_table("Engine");
    Engine.set_function(
        "PauseGame",
        []()
        {
            if (GEngine)
            {
                if (UWorld* World = GEngine->GetWorld())
                {
                    World->SetPaused(true);
                }
            }
        }
    );
    Engine.set_function(
        "ResumeGame",
        []()
        {
            if (GEngine)
            {
                if (UWorld* World = GEngine->GetWorld())
                {
                    World->SetPaused(false);
                }
            }
        }
    );
    Engine.set_function(
        "IsPaused",
        []()
        {
            if (GEngine)
            {
                if (UWorld* World = GEngine->GetWorld())
                {
                    return World->IsPaused();
                }
            }
            return false;
        }
    );
    Engine.set_function(
        "GetViewportSize",
        []() -> sol::table
        {
            sol::table Result = FLuaScriptManager::GetState().create_table();
            Result["Width"]   = 0.0f;
            Result["Height"]  = 0.0f;

            if (GEngine)
            {
                if (FWindowsWindow* Window = GEngine->GetWindow())
                {
                    Result["Width"]  = Window->GetWidth();
                    Result["Height"] = Window->GetHeight();
                }
            }

            return Result;
        }
    );
    Engine.set_function(
        "Exit",
        []()
        {
            // WM_QUIT — FEngineLoop::Run 이 PumpMessages 에서 잡고 정상 shutdown.
            PostQuitMessage(0);
        }
    );
    Engine.set_function(
        "SetOnEscape",
        [](sol::protected_function Callback)
        {
            FLuaScriptManager::SetOnEscapePressed(std::move(Callback));
        }
    );

    sol::table Key = Lua.create_named_table("Key");
    for (const FString& KeyName : GetKnownInputKeyNames())
    {
        Key[KeyName.c_str()] = ResolveInputKeyCode(KeyName);
    }
    Key.set_function(
        "Resolve",
        [](const FString& KeyName)
        {
            return ResolveInputKeyCode(KeyName);
        }
    );
    Key.set_function(
        "Name",
        [](int32 KeyCode)
        {
            return GetInputKeyName(KeyCode);
        }
    );

    sol::table CameraManager = Lua.create_named_table("CameraManager");
    CameraManager.set_function(
        "ToggleActorCamera",
        [](const FString& ActorName, sol::optional<float> BlendTime)
        {
            if (!GEngine || !GEngine->GetWorld())
            {
                return false;
            }

            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            return Manager ? Manager->ToggleActiveCameraForActor(ActorName, BlendTime.value_or(0.0f)) : false;
        }
    );
    CameraManager.set_function(
        "ToggleOwnerCamera",
        [](AActor* Actor, sol::optional<float> BlendTime)
        {
            if (!GEngine || !GEngine->GetWorld())
            {
                return false;
            }

            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            return Manager ? Manager->ToggleActiveCameraForActor(Actor, BlendTime.value_or(0.0f)) : false;
        }
    );
    CameraManager.set_function(
        "PossessCamera",
        [](UCameraComponent* Camera)
        {
            if (!GEngine || !GEngine->GetWorld() || !IsValid(Camera))
            {
                return false;
            }

            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (!Manager)
            {
                return false;
            }

            Manager->SetActiveCamera(Camera);
            Manager->Possess(Camera);
            return true;
        }
    );
    CameraManager.set_function(
        "GetActiveCameraOwner",
        []() -> AActor*
        {
            if (!GEngine || !GEngine->GetWorld())
            {
                return nullptr;
            }
            APlayerController*    PC           = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager      = PC ? PC->GetPlayerCameraManager() : nullptr;
            UCameraComponent*     ActiveCamera = Manager ? Manager->GetActiveCamera() : nullptr;
            if (!IsValid(ActiveCamera)) return nullptr;
            AActor* Owner = ActiveCamera->GetOwner();
            return IsValid(Owner) ? Owner : nullptr;
        }
    );
    CameraManager.set_function(
        "GetActiveCamera",
        []() -> UCameraComponent*
        {
            if (!GEngine || !GEngine->GetWorld())
            {
                return nullptr;
            }
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            return Manager ? Manager->GetActiveCamera() : nullptr;
        }
    );
    CameraManager.set_function(
        "GetPossessedCamera",
        []() -> UCameraComponent*
        {
            if (!GEngine || !GEngine->GetWorld())
            {
                return nullptr;
            }
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            return Manager ? Manager->GetPossessedCamera() : nullptr;
        }
    );
    CameraManager.set_function(
        "GetPossessedCameraOwner",
        []() -> AActor*
        {
            if (!GEngine || !GEngine->GetWorld())
            {
                return nullptr;
            }
            APlayerController*    PC              = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager         = PC ? PC->GetPlayerCameraManager() : nullptr;
            UCameraComponent*     PossessedCamera = Manager ? Manager->GetPossessedCamera() : nullptr;
            if (!IsValid(PossessedCamera)) return nullptr;
            AActor* Owner = PossessedCamera->GetOwner();
            return IsValid(Owner) ? Owner : nullptr;
        }
    );
    CameraManager.set_function(
        "FadeOut",
        [](float Duration)
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->StartCameraFade(0.0f, 1.0f, Duration, FLinearColor::Black(), false, true);
            }
        }
    );
    CameraManager.set_function(
        "FadeIn",
        [](float Duration)
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->StartCameraFade(1.0f, 0.0f, Duration, FLinearColor::Black(), false, true);
            }
        }
    );
    CameraManager.set_function(
        "SetVignette",
        [](float Intensity, float Radius, float Softness)
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->SetCameraVignette(Intensity, Radius, Softness, FLinearColor::Black());
            }
        }
    );
    CameraManager.set_function(
        "ClearVignette",
        []()
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->ClearCameraVignette();
            }
        }
    );
    CameraManager.set_function(
        "SetViewTargetWithBlend",
        [](AActor* Target, float BlendTime)
        {
            if (!GEngine || !GEngine->GetWorld() || !IsValid(Target)) return;

            APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
            if (PC)
            {
                PC->SetViewTargetWithBlend(Target, BlendTime);
            }
        }
    );
    // ActiveCamera 컴포넌트 단위 blend — 같은 액터 내 1인칭/3인칭 같은 별개 카메라
    // 컴포넌트 사이 부드럽게 전환. BlendTime 미지정 시 0 (즉시 swap).
    CameraManager.set_function(
        "SetActiveCameraWithBlend",
        [](UCameraComponent* NewCamera, sol::optional<float> BlendTime)
        {
            if (!GEngine || !GEngine->GetWorld() || !IsValid(NewCamera)) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->SetActiveCameraWithBlend(NewCamera, BlendTime.value_or(0.0f));
            }
        }
    );
    // Sample wave-oscillator shake — Lua console / 스크립트에서 즉시 흔들기 테스트용.
    // 호출 예: CameraManager.StartWaveShake(1.0)
    CameraManager.set_function(
        "StartWaveShake",
        [](sol::optional<float> Scale)
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->StartCameraShake<UWaveOscillatorCameraShake>(Scale.value_or(1.0f));
            }
        }
    );
    CameraManager.set_function(
        "StartSequenceShake",
        [](sol::optional<float> Scale)
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->StartCameraShake<USequenceCameraShake>(Scale.value_or(1.0f));
            }
        }
    );
    CameraManager.set_function(
        "StartCameraShakeAsset",
        [](const FString& AssetPath, sol::optional<float> Scale)
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->StartCameraShakeAsset(AssetPath, Scale.value_or(1.0f));
            }
        }
    );
    CameraManager.set_function(
        "SetDepthOfField",
        [](float FocusDistance, float FocusRange, float MaxBlurRadius)
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->SetDepthOfField(FocusDistance, FocusRange, MaxBlurRadius);
            }
        }
    );
    CameraManager.set_function(
        "SetBokeh",
        [](float RadiusThreshold, float LumaThreshold, float Intensity)
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->SetBokeh(RadiusThreshold, LumaThreshold, Intensity);
            }
        }
    );
    CameraManager.set_function(
        "ClearDepthOfField",
        []()
        {
            if (!GEngine || !GEngine->GetWorld()) return;
            APlayerController*    PC      = GEngine->GetWorld()->GetFirstPlayerController();
            APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
            if (Manager)
            {
                Manager->ClearDepthOfField();
            }
        }
    );

    sol::table AudioManager = Lua.create_named_table("AudioManager");
    AudioManager.set_function(
        "Load",
        [](const FString& SoundName, const FString& Path, sol::optional<bool> bLoop)
        {
            return FAudioManager::Get().LoadAudio(SoundName, Path, bLoop.value_or(false));
        }
    );
    AudioManager.set_function(
        "Play",
        [](const FString& SoundName, float Volume)
        {
            FAudioManager::Get().PlayAudio(SoundName, Volume);
        }
    );
    AudioManager.set_function(
        "PlayBGM",
        [](const FString& SoundName, float Volume)
        {
            FAudioManager::Get().PlayBGM(SoundName, Volume);
        }
    );
    AudioManager.set_function(
        "StopBGM",
        []()
        {
            FAudioManager::Get().StopBGM();
        }
    );
    AudioManager.set_function(
        "PlayLoop",
        [](const FString& SoundName, const FString& LoopName, sol::optional<float> Volume, sol::optional<float> Pitch)
        {
            FAudioManager::Get().PlayLoop(SoundName, LoopName, Volume.value_or(1.0f), Pitch.value_or(1.0f));
        }
    );
    AudioManager.set_function(
        "StopLoop",
        [](const FString& LoopName)
        {
            FAudioManager::Get().StopLoop(LoopName);
        }
    );
    AudioManager.set_function(
        "StopAllLoops",
        []()
        {
            FAudioManager::Get().StopAllLoops();
        }
    );
    AudioManager.set_function(
        "SetLoopVolume",
        [](const FString& LoopName, float Volume)
        {
            FAudioManager::Get().SetLoopVolume(LoopName, Volume);
        }
    );
    AudioManager.set_function(
        "SetLoopPitch",
        [](const FString& LoopName, float Pitch)
        {
            FAudioManager::Get().SetLoopPitch(LoopName, Pitch);
        }
    );
    AudioManager.set_function(
        "IsLoopPlaying",
        [](const FString& LoopName)
        {
            return FAudioManager::Get().IsLoopPlaying(LoopName);
        }
    );
    AudioManager.set_function(
        "SetMasterVolume",
        [](float Volume)
        {
            FAudioManager::Get().SetMasterVolume(Volume);
        }
    );

    // Short alias for gameplay scripts.
    Lua["Audio"] = AudioManager;

    sol::table Time = Lua.create_named_table("Time");
    Time.set_function(
        "DeltaTime",
        []() -> float
        {
            FTimer* T = GEngine ? GEngine->GetTimer() : nullptr;
            return T ? T->GetDeltaTime() : 0.0f;
        }
    );
    Time.set_function(
        "RawDeltaTime",
        []() -> float
        {
            FTimer* T = GEngine ? GEngine->GetTimer() : nullptr;
            return T ? T->GetRawDeltaTime() : 0.0f;
        }
    );
    Time.set_function(
        "TotalTime",
        []() -> double
        {
            FTimer* T = GEngine ? GEngine->GetTimer() : nullptr;
            return T ? T->GetTotalTime() : 0.0;
        }
    );
    Time.set_function(
        "FPS",
        []() -> float
        {
            FTimer* T = GEngine ? GEngine->GetTimer() : nullptr;
            return T ? T->GetFPS() : 0.0f;
        }
    );
    Time.set_function(
        "DisplayFPS",
        []() -> float
        {
            FTimer* T = GEngine ? GEngine->GetTimer() : nullptr;
            return T ? T->GetDisplayFPS() : 0.0f;
        }
    );
    Time.set_function(
        "FrameTimeMs",
        []() -> float
        {
            FTimer* T = GEngine ? GEngine->GetTimer() : nullptr;
            return T ? T->GetFrameTimeMs() : 0.0f;
        }
    );
    Time.set_function(
        "GetTimeDilation",
        []() -> float
        {
            FTimer* T = GEngine ? GEngine->GetTimer() : nullptr;
            return T ? T->GetTimeDilation() : 1.0f;
        }
    );
    Time.set_function(
        "SetTimeDilation",
        [](float Dilation)
        {
            if (FTimer* T = GEngine ? GEngine->GetTimer() : nullptr) T->SetTimeDilation(Dilation);
        }
    );
    Time.set_function(
        "GetMaxFPS",
        []() -> float
        {
            FTimer* T = GEngine ? GEngine->GetTimer() : nullptr;
            return T ? T->GetMaxFPS() : 0.0f;
        }
    );
    Time.set_function(
        "SetMaxFPS",
        [](float FPS)
        {
            if (FTimer* T = GEngine ? GEngine->GetTimer() : nullptr) T->SetMaxFPS(FPS);
        }
    );


    sol::table Texture = Lua.create_named_table("Texture");
    Texture.set_function(
        "Load",
        [](const FString& Path, sol::optional<bool> bSRGB) -> UTexture2D*
        {
            return UTexture2D::LoadFromCached(Path, bSRGB.value_or(true) ? ETextureColorSpace::SRGB : ETextureColorSpace::Linear);
        }
    );

    sol::table StaticMeshLibrary = Lua.create_named_table("StaticMeshLibrary");
    Lua["StaticMeshes"] = StaticMeshLibrary;
    StaticMeshLibrary.set_function(
        "Load",
        [](const FString& Path) -> UStaticMesh*
        {
            if (!GEngine || Path.empty() || Path == "None") return nullptr;
            ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
            return FMeshManager::LoadStaticMesh(Path, Device);
        }
    );

    sol::table Material = Lua.create_named_table("Material");
    Lua["MaterialLibrary"] = Material;
    Lua["Materials"] = Material;
    Material.set_function(
        "Load",
        [](const FString& Path) -> UMaterial*
        {
            return FMaterialManager::Get().GetOrCreateMaterial(Path);
        }
    );
    Material.set_function(
        "GetOrCreate",
        [](const FString& Path) -> UMaterial*
        {
            return FMaterialManager::Get().GetOrCreateMaterial(Path);
        }
    );
    Material.set_function(
        "Create",
        [](const FString& Path) -> UMaterial*
        {
            return FMaterialManager::Get().CreateMaterialAsset(Path);
        }
    );
    Material.set_function(
        "CreateGraph",
        [](const FString& Path) -> UMaterial*
        {
            return FMaterialManager::Get().CreateGraphMaterialAsset(Path);
        }
    );
    Material.set_function(
        "GetComponentMaterial",
        [](UPrimitiveComponent* Component, int32 ElementIndex) -> UMaterial*
        {
            if (!IsValid(Component)) return nullptr;
            if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
            {
                return StaticMeshComponent->GetMaterial(ElementIndex);
            }
            if (USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(Component))
            {
                return SkinnedMeshComponent->GetMaterial(ElementIndex);
            }
            return nullptr;
        }
    );
    Material.set_function(
        "SetComponentMaterial",
        [](UPrimitiveComponent* Component, int32 ElementIndex, UMaterial* InMaterial) -> bool
        {
            if (!IsValid(Component) || !IsValid(InMaterial)) return false;
            if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
            {
                StaticMeshComponent->SetMaterial(ElementIndex, InMaterial);
                return true;
            }
            if (USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(Component))
            {
                SkinnedMeshComponent->SetMaterial(ElementIndex, InMaterial);
                return true;
            }
            return false;
        }
    );
    Material.set_function(
        "SetComponentMaterialByPath",
        [](UPrimitiveComponent* Component, int32 ElementIndex, const FString& MaterialPath) -> bool
        {
            if (!IsValid(Component)) return false;
            if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
            {
                return StaticMeshComponent->SetMaterialByPath(ElementIndex, MaterialPath);
            }
            if (USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(Component))
            {
                return SkinnedMeshComponent->SetMaterialByPath(ElementIndex, MaterialPath);
            }
            return false;
        }
    );
    Material.set_function(
        "CreateDynamicInstance",
        [](UMaterial* Parent, UObject* Owner, sol::optional<FString> DebugName) -> UMaterialInstanceDynamic*
        {
            return IsValid(Parent) ? UMaterialInstanceDynamic::Create(Parent, Owner, DebugName.value_or(FString())) : nullptr;
        }
    );
    Material.set_function(
        "CreateDynamicInstanceForComponent",
        [](UPrimitiveComponent* Component, int32 ElementIndex, sol::optional<FString> DebugName) -> UMaterialInstanceDynamic*
        {
            if (!IsValid(Component)) return nullptr;

            UMaterial* ParentMaterial = nullptr;
            if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
            {
                ParentMaterial = StaticMeshComponent->GetMaterial(ElementIndex);
                if (UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(ParentMaterial, Component, DebugName.value_or(FString())))
                {
                    StaticMeshComponent->SetMaterial(ElementIndex, Instance);
                    return Instance;
                }
                return nullptr;
            }
            if (USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(Component))
            {
                ParentMaterial = SkinnedMeshComponent->GetMaterial(ElementIndex);
                if (UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(ParentMaterial, Component, DebugName.value_or(FString())))
                {
                    SkinnedMeshComponent->SetMaterial(ElementIndex, Instance);
                    return Instance;
                }
                return nullptr;
            }
            return nullptr;
        }
    );
    Material.set_function(
        "Save",
        [](UMaterial* Mat, const FString& Path)
        {
            return IsValid(Mat) && FMaterialManager::Get().SaveMaterial(Mat, Path);
        }
    );
    Material.set_function(
        "SetShader",
        [](UMaterial* Mat, const FString& ShaderPath)
        {
            return IsValid(Mat) && FMaterialManager::Get().SetMaterialShader(Mat, ShaderPath);
        }
    );
    Material.set_function(
        "SetScalarParameter",
        [](UMaterial* Mat, const FString& ParamName, float Value) -> bool
        {
            return IsValid(Mat) && Mat->SetScalarParameter(ParamName, Value);
        }
    );
    Material.set_function(
        "SetVectorParameter",
        [](UMaterial* Mat, const FString& ParamName, const sol::object& Value) -> bool
        {
            if (!IsValid(Mat)) return false;
            FVector VectorValue;
            if (!LuaObjectToVector(Value, VectorValue)) return false;
            return Mat->SetVector3Parameter(ParamName, VectorValue);
        }
    );
    Material.set_function(
        "SetColorParameter",
        [](UMaterial* Mat, const FString& ParamName, const sol::object& Value) -> bool
        {
            if (!IsValid(Mat)) return false;
            FVector4 ColorValue;
            if (!LuaObjectToVector4(Value, ColorValue)) return false;
            return Mat->SetVector4Parameter(ParamName, ColorValue);
        }
    );
    Material.set_function(
        "SetTextureParameter",
        [](UMaterial* Mat, const FString& ParamName, UTexture2D* Texture) -> bool
        {
            return IsValid(Mat) && IsValid(Texture) && Mat->SetTextureParameter(ParamName, Texture);
        }
    );

    sol::table CameraShake = Lua.create_named_table("CameraShake");
    CameraShake.set_function(
        "Load",
        [](const FString& Path) -> UCameraShakeAsset*
        {
            return FCameraShakeManager::Get().Load(Path);
        }
    );
    CameraShake.set_function(
        "Find",
        [](const FString& Path) -> UCameraShakeAsset*
        {
            return FCameraShakeManager::Get().Find(Path);
        }
    );
    CameraShake.set_function(
        "Save",
        [](UCameraShakeAsset* Asset)
        {
            return IsValid(Asset) && FCameraShakeManager::Get().Save(Asset);
        }
    );

    Lua.set_function(
        "LoadAudio",
        [](const FString& SoundName, const FString& Path, sol::optional<bool> bLoop)
        {
            return FAudioManager::Get().LoadAudio(SoundName, Path, bLoop.value_or(false));
        }
    );
}

void FLuaScriptManager::RegisterMathBindings(sol::state& Lua)
{
    Lua.new_usertype<FVector>(
        "Vector",
        sol::constructors<FVector(), FVector(float, float, float)>(),
        "X",
        &FVector::X,
        "Y",
        &FVector::Y,
        "Z",
        &FVector::Z,
        "Length",
        &FVector::Length,
        "Normalize",
        &FVector::Normalize,
        "Normalized",
        &FVector::Normalized,
        "Dot",
        &FVector::Dot,
        "Cross",
        sol::overload(
            static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::Cross),
            static_cast<FVector(*)(const FVector&, const FVector&)>(&FVector::Cross)
        ),
        "Distance",
        &FVector::Distance,
        "DistSquared",
        &FVector::DistSquared,
        "Lerp",
        &FVector::Lerp,
        sol::meta_function::addition,
        sol::overload(
            static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::operator+),
            static_cast<FVector(FVector::*)(float) const>(&FVector::operator+)
        ),
        sol::meta_function::subtraction,
        sol::overload(
            static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::operator-),
            static_cast<FVector(FVector::*)(float) const>(&FVector::operator-)
        ),
        sol::meta_function::multiplication,
        static_cast<FVector(FVector::*)(float) const>(&FVector::operator*),
        sol::meta_function::division,
        &FVector::operator/,
        "Zero",
        []()
        {
            return FVector::ZeroVector;
        },
        "One",
        []()
        {
            return FVector::OneVector;
        },
        "Up",
        []()
        {
            return FVector::UpVector;
        },
        "Down",
        []()
        {
            return FVector::DownVector;
        },
        "Forward",
        []()
        {
            return FVector::ForwardVector;
        },
        "Backward",
        []()
        {
            return FVector::BackwardVector;
        },
        "Right",
        []()
        {
            return FVector::RightVector;
        },
        "Left",
        []()
        {
            return FVector::LeftVector;
        },
        "XAxis",
        []()
        {
            return FVector::XAxisVector;
        },
        "YAxis",
        []()
        {
            return FVector::YAxisVector;
        },
        "ZAxis",
        []()
        {
            return FVector::ZAxisVector;
        }
    );

    Lua.set_function(
        "Vec3",
        [](sol::optional<float> X, sol::optional<float> Y, sol::optional<float> Z)
        {
            return FVector(X.value_or(0.0f), Y.value_or(0.0f), Z.value_or(0.0f));
        }
    );
    sol::table Math = Lua.create_named_table("Math");
    Math.set_function(
        "Vector",
        [](sol::optional<float> X, sol::optional<float> Y, sol::optional<float> Z)
        {
            return FVector(X.value_or(0.0f), Y.value_or(0.0f), Z.value_or(0.0f));
        }
    );
    Math.set_function(
        "Clamp",
        [](float Value, float Min, float Max)
        {
            return max(Min, min(Max, Value));
        }
    );
    Math.set_function(
        "Lerp",
        [](float A, float B, float Alpha)
        {
            return A + (B - A) * Alpha;
        }
    );
    Math.set_function(
        "Distance",
        [](const FVector& A, const FVector& B)
        {
            return (A - B).Length();
        }
    );
    Math.set_function(
        "Normalize",
        [](const FVector& V)
        {
            return V.Length() > 0.000001f ? V.Normalized() : FVector::ZeroVector;
        }
    );
    Math.set_function(
        "Dot",
        [](const FVector& A, const FVector& B)
        {
            return A.Dot(B);
        }
    );
    Math.set_function(
        "Cross",
        [](const FVector& A, const FVector& B)
        {
            return A.Cross(B);
        }
    );
}

void FLuaScriptManager::RegisterReflectionBindings(sol::state& Lua)
{
    Lua.new_usertype<UClass>(
        "UClass",
        "GetName",
        [](const UClass& Class)
        {
            return FString(Class.GetName() ? Class.GetName() : "");
        },
        "GetSuperClass",
        &UClass::GetSuperClass,
        "GetFlags",
        &UClass::GetClassFlags,
        "IsA",
        [](const UClass& Class, UClass* Other)
        {
            return Other && Class.IsA(Other);
        },
        "HasAnyClassFlags",
        &UClass::HasAnyClassFlags
    );

    sol::table Class = Lua.create_named_table("Class");
    Class.set_function(
        "Find",
        [](const FString& ClassName) -> UClass*
        {
            return UClass::FindByName(ClassName.c_str());
        }
    );
    Class.set_function(
        "Exists",
        [](const FString& ClassName)
        {
            return UClass::FindByName(ClassName.c_str()) != nullptr;
        }
    );
    Class.set_function(
        "All",
        [](sol::this_state State)
        {
            sol::state_view L(State);
            sol::table      Result = L.create_table();
            int32           Idx    = 1;
            for (UClass* C : UClass::GetAllClasses()) if (C) Result[Idx++] = C;
            return Result;
        }
    );
    Class.set_function(
        "GetFunctions",
        [](const FString& ClassName, sol::this_state State)
        {
            sol::state_view L(State);
            sol::table      Result = L.create_table();
            UClass*         C      = UClass::FindByName(ClassName.c_str());
            if (!C) return Result;
            TArray<const FFunction*> Functions;
            C->GetFunctionRefs(Functions);
            int32 Idx = 1;
            for (const FFunction* F : Functions) if (F) Result[Idx++] = LuaDescribeFunction(State, *F);
            return Result;
        }
    );
    Class.set_function(
        "GetProperties",
        [](const FString& ClassName, sol::this_state State)
        {
            sol::state_view L(State);
            sol::table      Result = L.create_table();
            UClass*         C      = UClass::FindByName(ClassName.c_str());
            if (!C) return Result;
            TArray<const FProperty*> Props;
            C->GetPropertyRefs(Props);
            int32 Idx = 1;
            for (const FProperty* P : Props) if (P) Result[Idx++] = LuaDescribeProperty(State, *P);
            return Result;
        }
    );

    Lua.new_usertype<UObject>(
        "Object",
        "GetName",
        [](UObject& Object)
        {
            return Object.GetName();
        },
        "GetClass",
        [](UObject& Object) -> UClass*
        {
            return IsValid(&Object) ? Object.GetClass() : nullptr;
        },
        "GetClassName",
        [](UObject& Object)
        {
            return IsValid(&Object) && Object.GetClass() ? FString(Object.GetClass()->GetName()) : FString();
        },
        "IsA",
        [](UObject& Object, const FString& ClassName)
        {
            UClass* C = UClass::FindByName(ClassName.c_str());
            return IsValid(&Object) && Object.GetClass() && C && Object.GetClass()->IsA(C);
        },
        "AsActor",
        [](UObject& Object) -> AActor*
        {
            return IsValid(&Object) ? Cast<AActor>(&Object) : nullptr;
        },
        "AsPawn",
        [](UObject& Object) -> APawn*
        {
            return IsValid(&Object) ? Cast<APawn>(&Object) : nullptr;
        },
        "AsPrimitiveComponent",
        [](UObject& Object) -> UPrimitiveComponent*
        {
            return IsValid(&Object) ? Cast<UPrimitiveComponent>(&Object) : nullptr;
        },
        "AsSceneComponent",
        [](UObject& Object) -> USceneComponent*
        {
            return IsValid(&Object) ? Cast<USceneComponent>(&Object) : nullptr;
        },
        "AsComponent",
        [](UObject& Object) -> UActorComponent*
        {
            return IsValid(&Object) ? Cast<UActorComponent>(&Object) : nullptr;
        },
        "GetUUID",
        &UObject::GetUUID,
        "IsValid",
        [](UObject* Object)
        {
            return IsValid(Object);
        },
        "CallFunction",
        [](UObject& Object, const FString& FunctionName, sol::variadic_args Args, sol::this_state State)
        {
            if (!IsValid(&Object))
            {
                return sol::make_object(sol::state_view(State), sol::nil);
            }
            return LuaInvokeReflectedFunction(State, &Object, nullptr, FunctionName, Args);
        },
        "CallFunctionSignature",
        [](UObject& Object, const FString& Signature, sol::variadic_args Args, sol::this_state State)
        {
            if (!IsValid(&Object))
            {
                return sol::make_object(sol::state_view(State), sol::nil);
            }
            return LuaInvokeReflectedFunctionBySignature(State, &Object, nullptr, Signature, Args);
        },
        "BindEvent",
        [](UObject& Object, const FString& FunctionNameOrSignature, sol::protected_function Callback)
        {
            return IsValid(&Object) && LuaBindReflectedEventOverride(&Object, FunctionNameOrSignature, std::move(Callback));
        },
        "UnbindEvent",
        [](UObject& Object, const FString& FunctionNameOrSignature)
        {
            return IsValid(&Object) && LuaUnbindReflectedEventOverride(&Object, FunctionNameOrSignature);
        },
        "HasEventBinding",
        [](UObject& Object, const FString& FunctionNameOrSignature)
        {
            return IsValid(&Object) && LuaHasReflectedEventOverride(&Object, FunctionNameOrSignature);
        },
        "GetProperty",
        [](UObject& Object, const FString& PropertyName, sol::this_state State)
        {
            return IsValid(&Object) ? LuaGetReflectedProperty(State, &Object, PropertyName) : sol::make_object(sol::state_view(State), sol::nil);
        },
        "SetProperty",
        [](UObject& Object, const FString& PropertyName, sol::object Value)
        {
            return IsValid(&Object) && LuaSetReflectedProperty(&Object, PropertyName, Value);
        },
        "GetFunctions",
        [](UObject& Object, sol::this_state State)
        {
            sol::state_view L(State);
            sol::table      Result = L.create_table();
            if (!IsValid(&Object) || !Object.GetClass())
            {
                return Result;
            }
            TArray<const FFunction*> Functions;
            Object.GetClass()->GetFunctionRefs(Functions);
            int Index = 1;
            for (const FFunction* Function : Functions)
            {
                if (Function)
                {
                    Result[Index++] = LuaDescribeFunction(State, *Function);
                }
            }
            return Result;
        },
        "GetProperties",
        [](UObject& Object, sol::this_state State)
        {
            sol::state_view L(State);
            sol::table      Result = L.create_table();
            if (!IsValid(&Object) || !Object.GetClass())
            {
                return Result;
            }
            TArray<const FProperty*> Properties;
            Object.GetClass()->GetPropertyRefs(Properties);
            int Index = 1;
            for (const FProperty* Property : Properties)
            {
                if (Property)
                {
                    Result[Index++] = LuaDescribeProperty(State, *Property);
                }
            }
            return Result;
        }
    );

    Lua.new_usertype<UActorComponent>(
        "ActorComponent",
        sol::base_classes,
        sol::bases<UObject>(),
        "GetOwner",
        &UActorComponent::GetOwner,
        "IsActive",
        &UActorComponent::IsActive,
        "SetActive",
        &UActorComponent::SetActive,
        "Activate",
        &UActorComponent::Activate,
        "Deactivate",
        &UActorComponent::Deactivate
    );


    Lua.new_usertype<ULuaBlueprintComponent>(
        "LuaBlueprintComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "ReloadBlueprint",
        &ULuaBlueprintComponent::ReloadBlueprint,
        "CallFunction",
        &ULuaBlueprintComponent::CallFunction,
        "CallLuaBlueprintFileFunction",
        &ULuaBlueprintComponent::CallLuaBlueprintFileFunction,
        "CallLuaScriptFileFunction",
        &ULuaBlueprintComponent::CallLuaScriptFileFunction,
        "GetBlueprintPath",
        &ULuaBlueprintComponent::GetBlueprintPath,
        "SetBlueprintPath",
        &ULuaBlueprintComponent::SetBlueprintPath
    );

    Lua.new_usertype<ULuaScriptComponent>(
        "LuaScriptComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "ReloadScript",
        &ULuaScriptComponent::ReloadScript,
        "CallFunction",
        &ULuaScriptComponent::CallFunction,
        "GetScriptFile",
        &ULuaScriptComponent::GetScriptFile,
        "SetScriptFile",
        &ULuaScriptComponent::SetScriptFile
    );

    sol::table Reflection = Lua.create_named_table("Reflection");
    Reflection.set_function(
        "Call",
        [](UObject* Object, const FString& FunctionName, sol::variadic_args Args, sol::this_state State)
        {
            return LuaInvokeReflectedFunction(State, Object, nullptr, FunctionName, Args);
        }
    );
    Reflection.set_function(
        "CallSignature",
        [](UObject* Object, const FString& Signature, sol::variadic_args Args, sol::this_state State)
        {
            return LuaInvokeReflectedFunctionBySignature(State, Object, nullptr, Signature, Args);
        }
    );
    Reflection.set_function(
        "CallStatic",
        [](const FString& ClassName, const FString& FunctionName, sol::variadic_args Args, sol::this_state State)
        {
            UClass* Class = UClass::FindByName(ClassName.c_str());
            return LuaInvokeReflectedFunction(State, nullptr, Class, FunctionName, Args);
        }
    );
    Reflection.set_function(
        "CallStaticSignature",
        [](const FString& ClassName, const FString& Signature, sol::variadic_args Args, sol::this_state State)
        {
            UClass* Class = UClass::FindByName(ClassName.c_str());
            return LuaInvokeReflectedFunctionBySignature(State, nullptr, Class, Signature, Args);
        }
    );
    Reflection.set_function(
        "BindEvent",
        [](UObject* Object, const FString& FunctionNameOrSignature, sol::protected_function Callback)
        {
            return LuaBindReflectedEventOverride(Object, FunctionNameOrSignature, std::move(Callback));
        }
    );
    Reflection.set_function(
        "UnbindEvent",
        [](UObject* Object, const FString& FunctionNameOrSignature)
        {
            return LuaUnbindReflectedEventOverride(Object, FunctionNameOrSignature);
        }
    );
    Reflection.set_function(
        "HasEventBinding",
        [](UObject* Object, const FString& FunctionNameOrSignature)
        {
            return LuaHasReflectedEventOverride(Object, FunctionNameOrSignature);
        }
    );
    Reflection.set_function(
        "GetProperty",
        [](UObject* Object, const FString& PropertyName, sol::this_state State)
        {
            return LuaGetReflectedProperty(State, Object, PropertyName);
        }
    );
    Reflection.set_function(
        "SetProperty",
        [](UObject* Object, const FString& PropertyName, sol::object Value)
        {
            return LuaSetReflectedProperty(Object, PropertyName, Value);
        }
    );
    Reflection.set_function(
        "GetFunctions",
        [](UObject* Object, sol::this_state State)
        {
            sol::state_view L(State);
            sol::table      Result = L.create_table();
            if (!IsValid(Object) || !Object->GetClass())
            {
                return Result;
            }
            TArray<const FFunction*> Functions;
            Object->GetClass()->GetFunctionRefs(Functions);
            int Index = 1;
            for (const FFunction* Function : Functions)
            {
                if (Function)
                {
                    Result[Index++] = LuaDescribeFunction(State, *Function);
                }
            }
            return Result;
        }
    );
    // LuaBlueprint Cast 노드용 — IsA 통과 시 같은 객체 반환, 실패 시 nil.
    // 실제 Unreal Blueprint Cast 와 동일한 의미 (성공/실패 분기).
    Reflection.set_function(
        "Cast",
        [](UObject* Object, const FString& ClassName) -> UObject*
        {
            if (!IsValid(Object)) return nullptr;
            UClass* Target = UClass::FindByName(ClassName.c_str());
            if (!Target) return nullptr;
            UClass* Source = Object->GetClass();
            if (!Source) return nullptr;
            return Source->IsA(Target) ? Object : nullptr;
        }
    );
    Reflection.set_function(
        "GetStaticFunctions",
        [](const FString& ClassName, sol::this_state State)
        {
            sol::state_view L(State);
            sol::table      Result = L.create_table();
            UClass*         Class  = UClass::FindByName(ClassName.c_str());
            if (!Class)
            {
                return Result;
            }
            TArray<const FFunction*> Functions;
            Class->GetFunctionRefs(Functions);
            int Index = 1;
            for (const FFunction* Function : Functions)
            {
                if (Function && Function->IsStatic())
                {
                    Result[Index++] = LuaDescribeFunction(State, *Function);
                }
            }
            return Result;
        }
    );
    Reflection.set_function(
        "GetProperties",
        [](UObject* Object, sol::this_state State)
        {
            sol::state_view L(State);
            sol::table      Result = L.create_table();
            if (!IsValid(Object) || !Object->GetClass())
            {
                return Result;
            }
            TArray<const FProperty*> Properties;
            Object->GetClass()->GetPropertyRefs(Properties);
            int Index = 1;
            for (const FProperty* Property : Properties)
            {
                if (Property)
                {
                    Result[Index++] = LuaDescribeProperty(State, *Property);
                }
            }
            return Result;
        }
    );
}

void FLuaScriptManager::RegisterActorBindings(sol::state& Lua)
{
    Lua.new_usertype<UTexture2D>(
        "Texture2D",
        sol::base_classes,
        sol::bases<UObject>(),
        "GetSourcePath",
        &UTexture2D::GetSourcePath,
        "GetWidth",
        &UTexture2D::GetWidth,
        "GetHeight",
        &UTexture2D::GetHeight,
        "IsLoaded",
        &UTexture2D::IsLoaded
    );

    Lua.new_usertype<UCameraShakeBase>(
        "CameraShakeBase",
        sol::base_classes,
        sol::bases<UObject>(),
        "StopShake",
        [](UCameraShakeBase& S, sol::optional<bool> bImmediately)
        {
            S.StopShake(bImmediately.value_or(true));
        },
        "IsFinished",
        &UCameraShakeBase::IsFinished,
        "GetPlaySpace",
        [](UCameraShakeBase& S)
        {
            return static_cast<int32>(S.GetPlaySpace());
        }
    );

    Lua.new_usertype<UCameraModifier>(
        "CameraModifier",
        sol::base_classes,
        sol::bases<UObject>(),
        "Enable",
        &UCameraModifier::EnableModifier,
        "Disable",
        [](UCameraModifier& M, sol::optional<bool> bImmediate)
        {
            M.DisableModifier(bImmediate.value_or(false));
        },
        "IsDisabled",
        &UCameraModifier::IsDisabled
    );

    Lua.new_usertype<UCameraShakeAsset>(
        "CameraShakeAsset",
        sol::base_classes,
        sol::bases<UObject>(),
        "LoadFromFile",
        &UCameraShakeAsset::LoadFromFile,
        "SaveToFile",
        &UCameraShakeAsset::SaveToFile,
        "SetSourcePath",
        &UCameraShakeAsset::SetSourcePath,
        "GetSourcePath",
        &UCameraShakeAsset::GetSourcePath
    );

    Lua.new_usertype<APlayerCameraManager>(
        "PlayerCameraManager",
        sol::base_classes,
        sol::bases<AActor, UObject>(),
        "RegisterCamera",
        &APlayerCameraManager::RegisterCamera,
        "UnregisterCamera",
        &APlayerCameraManager::UnregisterCamera,
        "AutoPossessDefaultCamera",
        &APlayerCameraManager::AutoPossessDefaultCamera,
        "ToggleActiveCameraForActor",
        sol::overload(
            [](APlayerCameraManager& M, const FString& ActorName, sol::optional<float> BlendTime)
            {
                return M.ToggleActiveCameraForActor(ActorName, BlendTime.value_or(0.0f));
            },
            [](APlayerCameraManager& M, const AActor* Actor, sol::optional<float> BlendTime)
            {
                return M.ToggleActiveCameraForActor(Actor, BlendTime.value_or(0.0f));
            }
        ),
        "GetActiveCamera",
        &APlayerCameraManager::GetActiveCamera,
        "SetActiveCamera",
        &APlayerCameraManager::SetActiveCamera,
        "SetActiveCameraWithBlend",
        [](APlayerCameraManager& M, UCameraComponent* NewCamera, sol::optional<float> BlendTime)
        {
            if (IsValid(NewCamera)) M.SetActiveCameraWithBlend(NewCamera, BlendTime.value_or(0.0f));
        },
        "GetPossessedCamera",
        &APlayerCameraManager::GetPossessedCamera,
        "Possess",
        &APlayerCameraManager::Possess,
        "SetViewTarget",
        [](APlayerCameraManager& M, AActor* Target)
        {
            if (IsValid(Target)) M.SetViewTarget(Target);
        },
        "GetViewTarget",
        &APlayerCameraManager::GetViewTarget,
        "GetPendingViewTarget",
        &APlayerCameraManager::GetPendingViewTarget,
        "StartCameraShakeAssetByPath",
        [](APlayerCameraManager& M, const FString& Path, sol::optional<float> Scale)
        {
            return M.StartCameraShakeAsset(Path, Scale.value_or(1.0f));
        },
        "StartCameraShakeAsset",
        [](APlayerCameraManager& M, UCameraShakeAsset* Asset, sol::optional<float> Scale)
        {
            return IsValid(Asset) ? M.StartCameraShakeAsset(Asset, Scale.value_or(1.0f)) : nullptr;
        },
        "StopAllCameraShakes",
        [](APlayerCameraManager& M, sol::optional<bool> bImmediately)
        {
            M.StopAllCameraShakes(bImmediately.value_or(true));
        },
        "StartCameraFade",
        [](APlayerCameraManager& M, float FromAlpha, float ToAlpha, float Duration, sol::optional<bool> bHold)
        {
            M.StartCameraFade(FromAlpha, ToAlpha, Duration, FLinearColor::Black(), false, bHold.value_or(false));
        },
        "StopCameraFade",
        &APlayerCameraManager::StopCameraFade,
        "SetCameraVignette",
        [](APlayerCameraManager& M, float Intensity, float Radius, float Softness)
        {
            M.SetCameraVignette(Intensity, Radius, Softness, FLinearColor::Black());
        },
        "ClearCameraVignette",
        &APlayerCameraManager::ClearCameraVignette,
        "IsFadeEnabled",
        &APlayerCameraManager::IsFadeEnabled,
        "GetFadeAmount",
        &APlayerCameraManager::GetFadeAmount,
        "IsVignetteEnabled",
        &APlayerCameraManager::IsVignetteEnabled,
        "GetVignetteIntensity",
        &APlayerCameraManager::GetVignetteIntensity,
        "GetVignetteRadius",
        &APlayerCameraManager::GetVignetteRadius,
        "GetVignetteSoftness",
        &APlayerCameraManager::GetVignetteSoftness,
        "SetDepthOfField",
        &APlayerCameraManager::SetDepthOfField,
        "SetBokeh",
        &APlayerCameraManager::SetBokeh,
        "ClearDepthOfField",
        &APlayerCameraManager::ClearDepthOfField,
        "IsDepthOfFieldEnabled",
        &APlayerCameraManager::IsDepthOfFieldEnabled,
        "GetDoFFocusDistance",
        &APlayerCameraManager::GetDoFFocusDistance,
        "GetDoFFocusRange",
        &APlayerCameraManager::GetDoFFocusRange,
        "GetDoFMaxBlurRadius",
        &APlayerCameraManager::GetDoFMaxBlurRadius,
        "GetDoFBokehRadiusThreshold",
        &APlayerCameraManager::GetDoFBokehRadiusThreshold,
        "GetDoFBokehLumaThreshold",
        &APlayerCameraManager::GetDoFBokehLumaThreshold,
        "GetDoFBokehIntensity",
        &APlayerCameraManager::GetDoFBokehIntensity
    );

    // Broad engine/gameplay bindings. The generic Reflection/CallFunction path can call
    // UFUNCTIONs, but concrete usertypes make LuaBlueprint scripting discoverable and
    // usable without hand-writing reflection strings for every common gameplay task.
    Lua.new_usertype<UMovementComponent>(
        "MovementComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "SetUpdatedComponent",
        &UMovementComponent::SetUpdatedComponent,
        "GetUpdatedComponent",
        &UMovementComponent::GetUpdatedComponent,
        "HasValidUpdatedComponent",
        &UMovementComponent::HasValidUpdatedComponent,
        "GetUpdatedComponentDisplayName",
        &UMovementComponent::GetUpdatedComponentDisplayName,
        "ResolveUpdatedComponent",
        &UMovementComponent::ResolveUpdatedComponent
    );

    Lua.new_usertype<UCharacterMovementComponent>(
        "CharacterMovementComponent",
        sol::base_classes,
        sol::bases<UMovementComponent, UActorComponent, UObject>(),
        "AddInputVector",
        sol::overload(
            [](UCharacterMovementComponent& C, const FVector& Direction, float Scale)
            {
                C.AddInputVector(Direction, Scale);
            },
            [](UCharacterMovementComponent& C, const FVector& Direction)
            {
                C.AddInputVector(Direction, 1.0f);
            }
        ),
        "GetVelocity",
        &UCharacterMovementComponent::GetVelocityValue,
        "GetVelocityValue",
        &UCharacterMovementComponent::GetVelocityValue,
        "GetSpeed",
        &UCharacterMovementComponent::GetSpeed,
        "GetMovementMode",
        [](UCharacterMovementComponent& C)
        {
            return static_cast<int32>(C.GetMovementMode());
        },
        "SetMovementMode",
        [](UCharacterMovementComponent& C, int32 Mode)
        {
            C.SetMovementMode(static_cast<EMovementMode>(Mode));
        },
        "IsWalking",
        &UCharacterMovementComponent::IsWalking,
        "IsFalling",
        &UCharacterMovementComponent::IsFalling,
        "Jump",
        &UCharacterMovementComponent::Jump,
        "HasPendingRootMotion",
        &UCharacterMovementComponent::HasPendingRootMotion,
        "HasYawDrivenByRootMotion",
        &UCharacterMovementComponent::HasYawDrivenByRootMotion
    );

    Lua.new_usertype<UProjectileMovementComponent>(
        "ProjectileMovementComponent",
        sol::base_classes,
        sol::bases<UMovementComponent, UActorComponent, UObject>(),
        "SetVelocity",
        &UProjectileMovementComponent::SetVelocity,
        "GetVelocity",
        &UProjectileMovementComponent::GetVelocity,
        "SetInitialSpeed",
        &UProjectileMovementComponent::SetInitialSpeed,
        "GetInitialSpeed",
        &UProjectileMovementComponent::GetInitialSpeed,
        "GetMaxSpeed",
        &UProjectileMovementComponent::GetMaxSpeed,
        "GetPreviewVelocity",
        &UProjectileMovementComponent::GetPreviewVelocity,
        "StopSimulating",
        &UProjectileMovementComponent::StopSimulating
    );

    Lua.new_usertype<URotatingMovementComponent>(
        "RotatingMovementComponent",
        sol::base_classes,
        sol::bases<UMovementComponent, UActorComponent, UObject>(),
        "SetRotationRate",
        [](URotatingMovementComponent& C, const FVector& Rate)
        {
            C.SetRotationRate(FRotator(Rate));
        },
        "GetRotationRate",
        [](URotatingMovementComponent& C)
        {
            return C.GetRotationRate().ToVector();
        },
        "SetRotationInLocalSpace",
        &URotatingMovementComponent::SetRotationInLocalSpace,
        "IsRotationInLocalSpace",
        &URotatingMovementComponent::IsRotationInLocalSpace,
        "SetPivotTranslation",
        &URotatingMovementComponent::SetPivotTranslation,
        "GetPivotTranslation",
        &URotatingMovementComponent::GetPivotTranslation
    );

    Lua.new_usertype<UPendulumMovementComponent>(
        "PendulumMovementComponent",
        sol::base_classes,
        sol::bases<UMovementComponent, UActorComponent, UObject>()
    );

    Lua.new_usertype<UShapeComponent>(
        "ShapeComponent",
        sol::base_classes,
        sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "IsDrawOnlyIfSelected",
        &UShapeComponent::IsDrawOnlyIfSelected,
        "GetShapeColor",
        [](UShapeComponent& C, sol::this_state State)
        {
            return LuaVector4ToTable(State, C.GetShapeColorVec4());
        }
    );

    Lua.new_usertype<UBoxComponent>(
        "BoxComponent",
        sol::base_classes,
        sol::bases<UShapeComponent, UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "SetBoxExtent",
        &UBoxComponent::SetBoxExtent,
        "GetScaledBoxExtent",
        &UBoxComponent::GetScaledBoxExtent,
        "GetUnscaledBoxExtent",
        &UBoxComponent::GetUnscaledBoxExtent
    );

    Lua.new_usertype<USphereComponent>(
        "SphereComponent",
        sol::base_classes,
        sol::bases<UShapeComponent, UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "SetSphereRadius",
        &USphereComponent::SetSphereRadius,
        "GetScaledSphereRadius",
        &USphereComponent::GetScaledSphereRadius,
        "GetUnscaledSphereRadius",
        &USphereComponent::GetUnscaledSphereRadius
    );

    Lua.new_usertype<UCapsuleComponent>(
        "CapsuleComponent",
        sol::base_classes,
        sol::bases<UShapeComponent, UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "SetCapsuleSize",
        &UCapsuleComponent::SetCapsuleSize,
        "GetScaledCapsuleRadius",
        &UCapsuleComponent::GetScaledCapsuleRadius,
        "GetScaledCapsuleHalfHeight",
        &UCapsuleComponent::GetScaledCapsuleHalfHeight,
        "GetUnscaledCapsuleRadius",
        &UCapsuleComponent::GetUnscaledCapsuleRadius,
        "GetUnscaledCapsuleHalfHeight",
        &UCapsuleComponent::GetUnscaledCapsuleHalfHeight
    );

    Lua.new_usertype<ULightComponentBase>(
        "LightComponentBase",
        sol::base_classes,
        sol::bases<USceneComponent, UActorComponent, UObject>(),
        "GetIntensity",
        &ULightComponentBase::GetIntensity,
        "SetIntensity",
        &ULightComponentBase::SetIntensity,
        "GetLightColor",
        [](ULightComponentBase& C, sol::this_state State)
        {
            return LuaVector4ToTable(State, C.GetLightColor());
        },
        "SetLightColor",
        [](ULightComponentBase& C, float R, float G, float B, sol::optional<float> A)
        {
            C.SetLightColor(FVector4(R, G, B, A.value_or(1.0f)));
        },
        "IsVisible",
        &ULightComponentBase::IsVisible,
        "CastShadows",
        &ULightComponentBase::CastShadows,
        "GetLightType",
        [](ULightComponentBase& C)
        {
            return static_cast<int32>(C.GetLightType());
        },
        "PushToScene",
        &ULightComponentBase::PushToScene,
        "DestroyFromScene",
        &ULightComponentBase::DestroyFromScene
    );

    Lua.new_usertype<ULightComponent>(
        "LightComponent",
        sol::base_classes,
        sol::bases<ULightComponentBase, USceneComponent, UActorComponent, UObject>(),
        "GetShadowResolutionScale",
        &ULightComponent::GetShadowResolutionScale,
        "GetShadowBias",
        &ULightComponent::GetShadowBias,
        "SetShadowBias",
        &ULightComponent::SetShadowBias,
        "GetShadowSlopeBias",
        &ULightComponent::GetShadowSlopeBias,
        "SetShadowSlopeBias",
        &ULightComponent::SetShadowSlopeBias,
        "GetShadowNormalBias",
        &ULightComponent::GetShadowNormalBias,
        "SetShadowNormalBias",
        &ULightComponent::SetShadowNormalBias,
        "GetShadowSharpen",
        &ULightComponent::GetShadowSharpen,
        "SetShadowSharpen",
        &ULightComponent::SetShadowSharpen
    );

    Lua.new_usertype<UAmbientLightComponent>("AmbientLightComponent", sol::base_classes, sol::bases<ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject>());
    Lua.new_usertype<UDirectionalLightComponent>("DirectionalLightComponent", sol::base_classes, sol::bases<ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject>());
    Lua.new_usertype<UPointLightComponent>(
        "PointLightComponent",
        sol::base_classes,
        sol::bases<ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject>(),
        "GetAttenuationRadius",
        &UPointLightComponent::GetAttenuationRadius,
        "SetAttenuationRadius",
        &UPointLightComponent::SetAttenuationRadius
    );
    Lua.new_usertype<USpotLightComponent>(
        "SpotLightComponent",
        sol::base_classes,
        sol::bases<UPointLightComponent, ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject>(),
        "GetOuterConeAngle",
        &USpotLightComponent::GetOuterConeAngle
    );

    Lua.new_usertype<UTextRenderComponent>(
        "TextRenderComponent",
        sol::base_classes,
        sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "SetText",
        &UTextRenderComponent::SetText,
        "GetText",
        &UTextRenderComponent::GetText,
        "SetFont",
        [](UTextRenderComponent& C, const FString& FontName)
        {
            C.SetFont(FName(FontName));
        },
        "GetFontName",
        [](UTextRenderComponent& C)
        {
            return C.GetFontName().ToString();
        },
        "SetColor",
        [](UTextRenderComponent& C, float R, float G, float B, sol::optional<float> A)
        {
            C.SetColor(FVector4(R, G, B, A.value_or(1.0f)));
        },
        "GetColor",
        [](UTextRenderComponent& C, sol::this_state State)
        {
            return LuaVector4ToTable(State, C.GetColor());
        },
        "SetFontSize",
        &UTextRenderComponent::SetFontSize,
        "GetFontSize",
        &UTextRenderComponent::GetFontSize,
        "SetRenderSpace",
        [](UTextRenderComponent& C, int32 Space)
        {
            C.SetRenderSpace(static_cast<ETextRenderSpace>(Space));
        },
        "GetRenderSpace",
        [](UTextRenderComponent& C)
        {
            return static_cast<int32>(C.GetRenderSpace());
        },
        "SetScreenPosition",
        &UTextRenderComponent::SetScreenPosition,
        "GetScreenX",
        &UTextRenderComponent::GetScreenX,
        "GetScreenY",
        &UTextRenderComponent::GetScreenY,
        "SetHorizontalAlignment",
        [](UTextRenderComponent& C, int32 Align)
        {
            C.SetHorizontalAlignment(static_cast<ETextHAlign>(Align));
        },
        "GetHorizontalAlignment",
        [](UTextRenderComponent& C)
        {
            return static_cast<int32>(C.GetHorizontalAlignment());
        },
        "SetVerticalAlignment",
        [](UTextRenderComponent& C, int32 Align)
        {
            C.SetVerticalAlignment(static_cast<ETextVAlign>(Align));
        },
        "GetVerticalAlignment",
        [](UTextRenderComponent& C)
        {
            return static_cast<int32>(C.GetVerticalAlignment());
        }
    );

    Lua.new_usertype<UBillboardComponent>(
        "BillboardComponent",
        sol::base_classes,
        sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "SetBillboardEnabled",
        &UBillboardComponent::SetBillboardEnabled
    );

    Lua.new_usertype<USpringArmComponent>(
        "SpringArmComponent",
        sol::base_classes,
        sol::bases<USceneComponent, UActorComponent, UObject>()
    );
    Lua.new_usertype<UCineCameraComponent>(
        "CineCameraComponent",
        sol::base_classes,
        sol::bases<UCameraComponent, USceneComponent, UActorComponent, UObject>(),
        "SetLetterboxEnabled",
        &UCineCameraComponent::SetLetterboxEnabled,
        "SetLetterboxAmount",
        &UCineCameraComponent::SetLetterboxAmount,
        "SetLetterboxThickness",
        &UCineCameraComponent::SetLetterboxThickness,
        "SetLetterboxColor",
        [](UCineCameraComponent& C, float R, float G, float B, sol::optional<float> A)
        {
            C.SetLetterboxColor(FLinearColor(R, G, B, A.value_or(1.0f)));
        }
    );

    Lua.new_usertype<UMaterial>(
        "Material",
        sol::base_classes,
        sol::bases<UObject>(),
        "SetScalarParameter",
        &UMaterial::SetScalarParameter,
        "SetVector3Parameter",
        &UMaterial::SetVector3Parameter,
        "SetVector4Parameter",
        &UMaterial::SetVector4Parameter,
        "SetTextureParameter",
        &UMaterial::SetTextureParameter,
        "GetScalarParameterValue",
        &UMaterial::GetScalarParameterValue,
        "GetVector3ParameterValue",
        &UMaterial::GetVector3ParameterValue,
        "IsMaterialInstance",
        &UMaterial::IsMaterialInstance,
        "IsDynamicMaterialInstance",
        &UMaterial::IsDynamicMaterialInstance,
        "IsGraphMaterial",
        &UMaterial::IsGraphMaterial,
        "EnableGraphMaterial",
        &UMaterial::EnableGraphMaterial,
        "DisableGraphMaterial",
        &UMaterial::DisableGraphMaterial,
        "GetAssetPathFileName",
        &UMaterial::GetAssetPathFileName,
        "SetAssetPathFileName",
        &UMaterial::SetAssetPathFileName
    );

    {
        sol::object MaterialLibraryObject = Lua["MaterialLibrary"];
        sol::object MaterialTypeObject = Lua["Material"];
        if (MaterialLibraryObject.is<sol::table>() && MaterialTypeObject.is<sol::table>())
        {
            sol::table MaterialLibrary = MaterialLibraryObject.as<sol::table>();
            sol::table MaterialType = MaterialTypeObject.as<sol::table>();
            const char* LibraryFunctions[] = {
                    "Load",
                    "GetOrCreate",
                    "Create",
                    "CreateGraph",
                    "GetComponentMaterial",
                    "SetComponentMaterial",
                    "SetComponentMaterialByPath",
                    "CreateDynamicInstance",
                    "CreateDynamicInstanceForComponent",
                    "Save",
                    "SetShader",
                    "SetScalarParameter",
                    "SetVectorParameter",
                    "SetColorParameter",
                    "SetTextureParameter"
            };
            for (const char* FunctionName : LibraryFunctions)
            {
                sol::object Function = MaterialLibrary[FunctionName];
                MaterialType[FunctionName] = Function;
            }
        }
    }

    Lua.new_usertype<UMaterialInstanceDynamic>(
        "MaterialInstanceDynamic",
        sol::base_classes,
        sol::bases<UMaterial, UObject>(),
        "SetScalarParameterValue",
        &UMaterialInstanceDynamic::SetScalarParameterValue,
        "SetVector3ParameterValue",
        &UMaterialInstanceDynamic::SetVector3ParameterValue,
        "SetVectorParameterValue",
        &UMaterialInstanceDynamic::SetVectorParameterValue,
        "SetTextureParameterValue",
        &UMaterialInstanceDynamic::SetTextureParameterValue,
        "GetOwnerObject",
        &UMaterialInstanceDynamic::GetOwnerObject
    );

    Lua.new_usertype<UAnimSequence>(
        "AnimSequence",
        sol::base_classes,
        sol::bases<UObject>(),
        "GetNumberOfFrames",
        &UAnimSequence::GetNumberOfFrames,
        "TimeToFrame",
        &UAnimSequence::TimeToFrame,
        "FrameToTime",
        &UAnimSequence::FrameToTime,
        "GetAssetPathFileName",
        &UAnimSequence::GetAssetPathFileName,
        "SetAssetPathFileName",
        &UAnimSequence::SetAssetPathFileName,
        "GetForceRootLock",
        &UAnimSequence::GetForceRootLock,
        "SetForceRootLock",
        &UAnimSequence::SetForceRootLock,
        "GetEnableRootMotion",
        &UAnimSequence::GetEnableRootMotion,
        "SetEnableRootMotion",
        &UAnimSequence::SetEnableRootMotion,
        "GetRootMotionBoneName",
        &UAnimSequence::GetRootMotionBoneName,
        "SetRootMotionBoneName",
        &UAnimSequence::SetRootMotionBoneName
    );

    Lua.new_usertype<UAnimMontage>(
        "AnimMontage",
        sol::base_classes,
        sol::bases<UObject>(),
        "GetSourceSequence",
        &UAnimMontage::GetSourceSequence,
        "SetSourceSequence",
        &UAnimMontage::SetSourceSequence,
        "GetBlendInTime",
        &UAnimMontage::GetBlendInTime,
        "SetBlendInTime",
        &UAnimMontage::SetBlendInTime,
        "GetBlendOutTime",
        &UAnimMontage::GetBlendOutTime,
        "SetBlendOutTime",
        &UAnimMontage::SetBlendOutTime,
        "GetAssetPathFileName",
        &UAnimMontage::GetAssetPathFileName,
        "SetAssetPathFileName",
        &UAnimMontage::SetAssetPathFileName,
        "GetSourceSequencePath",
        &UAnimMontage::GetSourceSequencePath,
        "EnsureDefaultSection",
        &UAnimMontage::EnsureDefaultSection
    );

    Lua.new_usertype<USkeletalMesh>(
        "SkeletalMesh",
        sol::base_classes,
        sol::bases<UObject>(),
        "GetAssetPathFileName",
        &USkeletalMesh::GetAssetPathFileName,
        "SetAssetPathFileName",
        &USkeletalMesh::SetAssetPathFileName,
        "GetPhysicsAssetPath",
        &USkeletalMesh::GetPhysicsAssetPath
    );

    Lua.new_usertype<UAnimInstance>(
        "AnimInstance",
        sol::base_classes,
        sol::bases<UObject>(),
        "GetOwningComponent",
        &UAnimInstance::GetOwningComponent,
        "GetSkeletalMesh",
        &UAnimInstance::GetSkeletalMesh,
        "TryGetPawnOwner",
        &UAnimInstance::TryGetPawnOwner,
        "GetRootMotionMode",
        [](UAnimInstance& I)
        {
            return static_cast<int32>(I.GetRootMotionMode());
        },
        "SetRootMotionMode",
        [](UAnimInstance& I, int32 Mode)
        {
            I.SetRootMotionMode(static_cast<ERootMotionMode>(Mode));
        },
        "PlayMontage",
        [](UAnimInstance& I, UAnimMontage* M, sol::optional<FString> Section, sol::optional<float> Rate)
        {
            if (IsValid(M)) I.PlayMontage(M, Section ? FName(Section.value()) : FName::None, Rate.value_or(1.0f));
        },
        "StopMontage",
        [](UAnimInstance& I, sol::optional<float> BlendOut, sol::optional<FString> Slot)
        {
            I.StopMontage(BlendOut.value_or(-1.0f), Slot ? FName(Slot.value()) : FName::None);
        },
        "Montage_JumpToSection",
        [](UAnimInstance& I, const FString& Section, sol::optional<FString> Slot)
        {
            I.Montage_JumpToSection(FName(Section), Slot ? FName(Slot.value()) : FName::None);
        },
        "Montage_SetNextSection",
        [](UAnimInstance& I, const FString& From, const FString& To, sol::optional<FString> Slot)
        {
            I.Montage_SetNextSection(FName(From), FName(To), Slot ? FName(Slot.value()) : FName::None);
        },
        "IsMontagePlaying",
        [](UAnimInstance& I, sol::optional<UAnimMontage*> M, sol::optional<FString> Slot)
        {
            return I.IsMontagePlaying(M.value_or(nullptr), Slot ? FName(Slot.value()) : FName::None);
        },
        "IsAnimGraphInstance",
        [](UAnimInstance& I)
        {
            return Cast<UAnimGraphInstance>(&I) != nullptr;
        },
        "SetGraphVariableFloat",
        [](UAnimInstance& I, const FString& VariableName, float Value)
        {
            if (UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(&I))
            {
                return Graph->SetGraphVariableFloat(FName(VariableName), Value);
            }
            return false;
        },
        "SetGraphVariableBool",
        [](UAnimInstance& I, const FString& VariableName, bool bValue)
        {
            if (UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(&I))
            {
                return Graph->SetGraphVariableBool(FName(VariableName), bValue);
            }
            return false;
        },
        "SetGraphVariableInt",
        [](UAnimInstance& I, const FString& VariableName, int32 Value)
        {
            if (UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(&I))
            {
                return Graph->SetGraphVariableInt(FName(VariableName), Value);
            }
            return false;
        },
        "HasGraphVariableFloat",
        [](UAnimInstance& I, const FString& VariableName)
        {
            float Value = 0.0f;
            if (const UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(&I))
            {
                return Graph->GetGraphVariableFloat(FName(VariableName), Value);
            }
            return false;
        },
        "HasGraphVariableBool",
        [](UAnimInstance& I, const FString& VariableName)
        {
            bool bValue = false;
            if (const UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(&I))
            {
                return Graph->GetGraphVariableBool(FName(VariableName), bValue);
            }
            return false;
        },
        "HasGraphVariableInt",
        [](UAnimInstance& I, const FString& VariableName)
        {
            int32 Value = 0;
            if (const UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(&I))
            {
                return Graph->GetGraphVariableInt(FName(VariableName), Value);
            }
            return false;
        },
        "GetGraphVariableFloat",
        [](UAnimInstance& I, const FString& VariableName, sol::optional<float> DefaultValue)
        {
            float Value = DefaultValue.value_or(0.0f);
            if (const UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(&I))
            {
                float RuntimeValue = Value;
                if (Graph->GetGraphVariableFloat(FName(VariableName), RuntimeValue))
                {
                    return RuntimeValue;
                }
            }
            return Value;
        },
        "GetGraphVariableBool",
        [](UAnimInstance& I, const FString& VariableName, sol::optional<bool> DefaultValue)
        {
            bool bValue = DefaultValue.value_or(false);
            if (const UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(&I))
            {
                bool bRuntimeValue = bValue;
                if (Graph->GetGraphVariableBool(FName(VariableName), bRuntimeValue))
                {
                    return bRuntimeValue;
                }
            }
            return bValue;
        },
        "GetGraphVariableInt",
        [](UAnimInstance& I, const FString& VariableName, sol::optional<int32> DefaultValue)
        {
            int32 Value = DefaultValue.value_or(0);
            if (const UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(&I))
            {
                int32 RuntimeValue = Value;
                if (Graph->GetGraphVariableInt(FName(VariableName), RuntimeValue))
                {
                    return RuntimeValue;
                }
            }
            return Value;
        }
    );

    Lua.new_usertype<ACharacter>(
        "Character",
        sol::base_classes,
        sol::bases<APawn, AActor, UObject>(),
        "AddMovementInput",
        sol::overload(
            [](ACharacter& C, const FVector& Direction, float Scale)
            {
                C.AddMovementInput(Direction, Scale);
            },
            [](ACharacter& C, const FVector& Direction)
            {
                C.AddMovementInput(Direction, 1.0f);
            }
        ),
        "Jump",
        &ACharacter::Jump,
        "GetCapsuleComponent",
        &ACharacter::GetCapsuleComponent,
        "GetMesh",
        &ACharacter::GetMesh,
        "GetCharacterMovement",
        &ACharacter::GetCharacterMovement,
        "GetStateMachine",
        &ACharacter::GetStateMachine,
        "EnterRagdoll",
        &ACharacter::EnterRagdoll,
        "ExitRagdoll",
        &ACharacter::ExitRagdoll,
        "BeginRagdollRecovery",
        &ACharacter::BeginRagdollRecovery,
        "IsInRagdoll",
        &ACharacter::IsInRagdoll
    );


    sol::table LocomotionMode = Lua.create_named_table("LocomotionMode");
    LocomotionMode["Locked"] = static_cast<int>(ELocomotionMode::Locked);
    LocomotionMode["InputAllowed"] = static_cast<int>(ELocomotionMode::InputAllowed);
    LocomotionMode["Strafe"] = static_cast<int>(ELocomotionMode::Strafe);
    LocomotionMode["Retreat"] = static_cast<int>(ELocomotionMode::Retreat);

    sol::table CombatTeam = Lua.create_named_table("CombatTeam");
    CombatTeam["Neutral"] = static_cast<int>(ECombatTeam::Neutral);
    CombatTeam["Player"] = static_cast<int>(ECombatTeam::Player);
    CombatTeam["Enemy"] = static_cast<int>(ECombatTeam::Enemy);
    CombatTeam["World"] = static_cast<int>(ECombatTeam::World);

    sol::table EncounterState = Lua.create_named_table("EncounterState");
    EncounterState["Inactive"] = static_cast<int>(EEncounterState::Inactive);
    EncounterState["Active"] = static_cast<int>(EEncounterState::Active);
    EncounterState["Completed"] = static_cast<int>(EEncounterState::Completed);

    sol::table PathFollowingStatus = Lua.create_named_table("PathFollowingStatus");
    PathFollowingStatus["Idle"] = static_cast<int>(EPathFollowingStatus::Idle);
    PathFollowingStatus["Waiting"] = static_cast<int>(EPathFollowingStatus::Waiting);
    PathFollowingStatus["Moving"] = static_cast<int>(EPathFollowingStatus::Moving);
    PathFollowingStatus["Paused"] = static_cast<int>(EPathFollowingStatus::Paused);

    sol::table PathFollowingResult = Lua.create_named_table("PathFollowingResult");
    PathFollowingResult["Success"] = static_cast<int>(EPathFollowingResult::Success);
    PathFollowingResult["Blocked"] = static_cast<int>(EPathFollowingResult::Blocked);
    PathFollowingResult["OffPath"] = static_cast<int>(EPathFollowingResult::OffPath);
    PathFollowingResult["Aborted"] = static_cast<int>(EPathFollowingResult::Aborted);
    PathFollowingResult["Invalid"] = static_cast<int>(EPathFollowingResult::Invalid);

    sol::table PathFollowingRequestResult = Lua.create_named_table("PathFollowingRequestResult");
    PathFollowingRequestResult["Failed"] = static_cast<int>(EPathFollowingRequestResult::Failed);
    PathFollowingRequestResult["AlreadyAtGoal"] = static_cast<int>(EPathFollowingRequestResult::AlreadyAtGoal);
    PathFollowingRequestResult["RequestSuccessful"] = static_cast<int>(EPathFollowingRequestResult::RequestSuccessful);

    Lua.new_usertype<FNavAgentProperties>(
        "NavAgentProperties",
        "Radius", &FNavAgentProperties::Radius,
        "Height", &FNavAgentProperties::Height,
        "StepHeight", &FNavAgentProperties::StepHeight,
        "MaxSlopeDegrees", &FNavAgentProperties::MaxSlopeDegrees
    );

    Lua.new_usertype<FNavigationPath>(
        "NavigationPath",
        "IsValid", &FNavigationPath::IsValid,
        "Num", &FNavigationPath::Num,
        "GetPathPointLocation", &FNavigationPath::GetPathPointLocation,
        "GetEndLocation", &FNavigationPath::GetEndLocation,
        "IsPartial", [](const FNavigationPath& P) { return P.bIsPartial; }
    );

    Lua.new_usertype<ANavigationData>(
        "NavigationData",
        sol::base_classes,
        sol::bases<AActor, UObject>(),
        "RebuildNavigationData", &ANavigationData::RebuildNavigationData,
        "ClearNavigationData", &ANavigationData::ClearNavigationData,
        "IsNavigationDataBuilt", &ANavigationData::IsNavigationDataBuilt,
        "GetNavigationNodeCount", &ANavigationData::GetNavigationNodeCount,
        "GetBlockedNodeCount", &ANavigationData::GetBlockedNodeCount,
        "ProjectPointToNavigation", [](ANavigationData& D, const FVector& Point, const FNavAgentProperties& Props) { FVector Out; return D.ProjectPointToNavigation(Point, Out, Props) ? Out : FVector::ZeroVector; },
        "FindPath", [](ANavigationData& D, const FVector& Start, const FVector& End, const FNavAgentProperties& Props) { FNavigationPath Path; D.FindPath(Start, End, Props, Path); return Path; },
        "NavigationRaycast", &ANavigationData::NavigationRaycast,
        "GetRandomReachablePointInRadius", [](ANavigationData& D, const FVector& Origin, float Radius, const FNavAgentProperties& Props) { FVector Out; return D.GetRandomReachablePointInRadius(Origin, Radius, Out, Props) ? Out : FVector::ZeroVector; },
        "DebugDrawNavigationData", &ANavigationData::DebugDrawNavigationData,
        "GetSupportedAgent", &ANavigationData::GetSupportedAgent,
        "SetSupportedAgent", &ANavigationData::SetSupportedAgent
    );

    Lua.new_usertype<AGridNavMesh>(
        "GridNavMesh",
        sol::base_classes,
        sol::bases<ANavigationData, AActor, UObject>(),
        "RebuildNavigationData", &AGridNavMesh::RebuildNavigationData,
        "ClearNavigationData", &AGridNavMesh::ClearNavigationData,
        "IsNavigationDataBuilt", &AGridNavMesh::IsNavigationDataBuilt,
        "GetNavigationNodeCount", &AGridNavMesh::GetNavigationNodeCount,
        "GetBlockedNodeCount", &AGridNavMesh::GetBlockedNodeCount,
        "GetCellSize", &AGridNavMesh::GetCellSize,
        "GetLastBuildBoundsCount", &AGridNavMesh::GetLastBuildBoundsCount,
        "GetLastBuildCandidateCount", &AGridNavMesh::GetLastBuildCandidateCount,
        "GetLastBuildDurationMs", &AGridNavMesh::GetLastBuildDurationMs,
        "GetLastBuildMessage", &AGridNavMesh::GetLastBuildMessage,
        "SetCellSize", &AGridNavMesh::SetCellSize,
        "IsLocationOnNavData", &AGridNavMesh::IsLocationOnNavData,
        "DebugDrawNavigationData", &AGridNavMesh::DebugDrawNavigationData
    );

    Lua.new_usertype<UNavigationSystem>(
        "NavigationSystem",
        sol::base_classes,
        sol::bases<UObject>(),
        "RebuildNavigation", &UNavigationSystem::RebuildNavigation,
        "InvalidateNavigationData", &UNavigationSystem::InvalidateNavigationData,
        "GetMainNavigationData", &UNavigationSystem::GetMainNavigationData,
        "GetGridNavMesh", &UNavigationSystem::GetGridNavMesh,
        "HasBuiltNavigationData", &UNavigationSystem::HasBuiltNavigationData,
        "DebugDrawNavigationData", &UNavigationSystem::DebugDrawNavigationData,
        "GetLastQueryMessage", &UNavigationSystem::GetLastQueryMessage,
        "GetDebugSummary", &UNavigationSystem::GetDebugSummary,
        "ProjectPointToNavigation", [](UNavigationSystem& N, const FVector& Point, const FNavAgentProperties& Props) { FVector Out; return N.ProjectPointToNavigation(Point, Out, Props) ? Out : FVector::ZeroVector; },
        "FindPathSync", [](UNavigationSystem& N, const FVector& Start, const FVector& End, const FNavAgentProperties& Props) { FNavigationPath Path; N.FindPathSync(Start, End, Props, Path); return Path; },
        "NavigationRaycast", &UNavigationSystem::NavigationRaycast,
        "GetRandomReachablePointInRadius", [](UNavigationSystem& N, const FVector& Origin, float Radius, const FNavAgentProperties& Props) { FVector Out; return N.GetRandomReachablePointInRadius(Origin, Radius, Out, Props) ? Out : FVector::ZeroVector; },
        "HasNavigationBounds", &UNavigationSystem::HasNavigationBounds,
        "IsPointInsideNavigationBounds", &UNavigationSystem::IsPointInsideNavigationBounds
    );

    Lua.new_usertype<UPathFollowingComponent>(
        "PathFollowingComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "AbortMove", &UPathFollowingComponent::AbortMove,
        "PauseMove", &UPathFollowingComponent::PauseMove,
        "ResumeMove", &UPathFollowingComponent::ResumeMove,
        "GetStatus", [](UPathFollowingComponent& C) { return static_cast<int>(C.GetStatus()); },
        "IsMoving", &UPathFollowingComponent::IsMoving,
        "HasValidPath", &UPathFollowingComponent::HasValidPath,
        "GetCurrentPathIndex", &UPathFollowingComponent::GetCurrentPathIndex,
        "GetCurrentPathPoint", &UPathFollowingComponent::GetCurrentPathPoint,
        "GetRemainingDistance", &UPathFollowingComponent::GetRemainingDistance,
        "GetPathPointCount", &UPathFollowingComponent::GetPathPointCount,
        "GetLastMoveResult", [](UPathFollowingComponent& C) { return static_cast<int>(C.GetLastMoveResult()); },
        "GetLastPathPointCount", &UPathFollowingComponent::GetLastPathPointCount,
        "GetLastMoveGoalLocation", &UPathFollowingComponent::GetLastMoveGoalLocation
    );

    Lua.new_usertype<AController>(
        "Controller",
        sol::base_classes,
        sol::bases<AActor, UObject>(),
        "GetPawn", &AController::GetPawn,
        "GetPossessedPawn", &AController::GetPossessedPawn,
        "HasPawn", &AController::HasPawn,
        "UnPossess", &AController::UnPossess,
        "GetControlRotation", [](AController& C) { return C.GetControlRotation().ToVector(); },
        "SetControlRotation", [](AController& C, const FVector& R) { C.SetControlRotation(FRotator(R)); }
    );

    Lua.new_usertype<AAIController>(
        "AIController",
        sol::base_classes,
        sol::bases<AController, AActor, UObject>(),
        "MoveToActor", [](AAIController& C, AActor* Goal, float AcceptanceRadius, bool bUsePathfinding) { return static_cast<int>(C.MoveToActor(Goal, AcceptanceRadius, true, bUsePathfinding)); },
        "MoveToLocation", [](AAIController& C, const FVector& Goal, float AcceptanceRadius, bool bUsePathfinding) { return static_cast<int>(C.MoveToLocation(Goal, AcceptanceRadius, bUsePathfinding)); },
        "StopMovement", &AAIController::StopMovement,
        "GetPathFollowingComponent", &AAIController::GetPathFollowingComponent,
        "GetMoveStatus", [](AAIController& C) { return static_cast<int>(C.GetMoveStatus()); },
        "IsFollowingPath", &AAIController::IsFollowingPath,
        "GetMoveGoalActor", &AAIController::GetMoveGoalActor,
        "GetLastMoveRequestResult", [](AAIController& C) { return static_cast<int>(C.GetLastMoveRequestResult()); },
        "GetLastMoveResult", [](AAIController& C) { return static_cast<int>(C.GetLastMoveResult()); },
        "GetLastMoveFailureReason", &AAIController::GetLastMoveFailureReason,
        "GetCurrentPathPointCount", &AAIController::GetCurrentPathPointCount,
        "SetRepathInterval", &AAIController::SetRepathInterval,
        "GetNavAgentProperties", &AAIController::GetNavAgentProperties,
        "SetNavAgentProperties", &AAIController::SetNavAgentProperties
    );

    Lua.new_usertype<ANavMeshBoundsVolume>(
        "NavMeshBoundsVolume",
        sol::base_classes,
        sol::bases<AActor, UObject>(),
        "GetBoundsComponent", &ANavMeshBoundsVolume::GetBoundsComponent,
        "ContainsPoint", &ANavMeshBoundsVolume::ContainsPoint
    );


    Lua.new_usertype<FEnemyAttackData>(
        "EnemyAttackData",
        "AttackName",
        sol::property(
            [](const FEnemyAttackData& Data) { return Data.AttackName.ToString(); },
            [](FEnemyAttackData& Data, const FString& Name) { Data.AttackName = FName(Name); }
        ),
        "MinPhase", &FEnemyAttackData::MinPhase,
        "MaxPhase", &FEnemyAttackData::MaxPhase,
        "MinRange", &FEnemyAttackData::MinRange,
        "MaxRange", &FEnemyAttackData::MaxRange,
        "MaxAbsAngle", &FEnemyAttackData::MaxAbsAngle,
        "Cooldown", &FEnemyAttackData::Cooldown,
        "RecoveryTime", &FEnemyAttackData::RecoveryTime,
        "Weight", &FEnemyAttackData::Weight,
        "Damage", &FEnemyAttackData::Damage,
        "PoiseDamage", &FEnemyAttackData::PoiseDamage,
        "IsValid", [](const FEnemyAttackData& Data) { return Data.AttackName.IsValid(); }
    );

    Lua.new_usertype<FCombatDamageSpec>(
        "CombatDamageSpec",
        sol::constructors<FCombatDamageSpec()>(),
        "Damage", &FCombatDamageSpec::Damage,
        "PoiseDamage", &FCombatDamageSpec::PoiseDamage,
        "DamageCauser", &FCombatDamageSpec::DamageCauser,
        "InstigatorActor", &FCombatDamageSpec::InstigatorActor,
        "HitLocation", &FCombatDamageSpec::HitLocation,
        "HitDirection", &FCombatDamageSpec::HitDirection
    );

    Lua.new_usertype<FCombatDamageReport>(
        "CombatDamageReport",
        "Result", sol::property([](const FCombatDamageReport& Report) { return static_cast<int>(Report.Result); }),
        "RequestedDamage", &FCombatDamageReport::RequestedDamage,
        "AppliedDamage", &FCombatDamageReport::AppliedDamage,
        "PreviousHealth", &FCombatDamageReport::PreviousHealth,
        "NewHealth", &FCombatDamageReport::NewHealth,
        "Killed", &FCombatDamageReport::bKilled
    );

    Lua.new_usertype<UCombatHitEventComponent>(
        "CombatHitEventComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "ApplyDamageToTarget",
        sol::overload(
            [](UCombatHitEventComponent& C, AActor* Target, const FCombatDamageSpec& DamageSpec) { return C.ApplyDamageSpecToTarget(Target, DamageSpec); },
            [](UCombatHitEventComponent& C, AActor* Target, float Damage, float PoiseDamage) { return C.ApplyDamageToTarget(Target, Damage, PoiseDamage); },
            [](UCombatHitEventComponent& C, AActor* Target, float Damage) { return C.ApplyDamageToTarget(Target, Damage); }
        ),
        "ApplyDamageSpecToTarget", &UCombatHitEventComponent::ApplyDamageSpecToTarget
    );

    Lua.new_usertype<UHealthComponent>(
        "HealthComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "ApplyDamage",
        sol::overload(
            [](UHealthComponent& H, float Damage, AActor* DamageCauser, AActor* InstigatorActor) { return H.ApplyDamage(Damage, DamageCauser, InstigatorActor).AppliedDamage; },
            [](UHealthComponent& H, float Damage) { return H.ApplyDamage(Damage, nullptr, nullptr).AppliedDamage; }
        ),
        "Heal", &UHealthComponent::Heal,
        "SetHealth", &UHealthComponent::SetHealth,
        "Kill", &UHealthComponent::Kill,
        "ResetHealth", &UHealthComponent::ResetHealth,
        "IsDead", &UHealthComponent::IsDead,
        "IsAlive", &UHealthComponent::IsAlive,
        "GetCurrentHealth", &UHealthComponent::GetCurrentHealth,
        "GetMaxHealth", &UHealthComponent::GetMaxHealth,
        "GetHealthRatio", &UHealthComponent::GetHealthRatio,
        "BindOnHealthChanged",
        [](UHealthComponent& H, sol::protected_function Callback)
        {
            return BindLuaHealthChanged(&H, std::move(Callback));
        },
        "UnbindOnHealthChanged",
        [](UHealthComponent& H, uint64 BindingId)
        {
            return UnbindLuaHealthChanged(&H, BindingId);
        },
        "SetInvincible", &UHealthComponent::SetInvincible,
        "IsInvincible", &UHealthComponent::IsInvincible
    );

    Lua.new_usertype<UCombatStateComponent>(
        "CombatStateComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "CanReceiveDamage", &UCombatStateComponent::CanReceiveDamage,
        "SetDamageEnabled", &UCombatStateComponent::SetDamageEnabled,
        "SetInvincible", &UCombatStateComponent::SetInvincible,
        "SetSuperArmor", &UCombatStateComponent::SetSuperArmor,
        "IsInvincible", &UCombatStateComponent::IsInvincible,
        "HasSuperArmor", &UCombatStateComponent::HasSuperArmor,
        "IsStaggered", &UCombatStateComponent::IsStaggered,
        "IsAttacking", &UCombatStateComponent::IsAttacking,
        "ApplyPoiseDamage", &UCombatStateComponent::ApplyPoiseDamage,
        "ResetPoise", &UCombatStateComponent::ResetPoise,
        "StartStagger", &UCombatStateComponent::StartStagger,
        "StopStagger", &UCombatStateComponent::StopStagger,
        "GetPoiseRatio", &UCombatStateComponent::GetPoiseRatio,
        "GetPostureRatio", &UCombatStateComponent::GetPostureRatio,
        "GetCurrentPoise", &UCombatStateComponent::GetCurrentPoise,
        "GetMaxPoise", &UCombatStateComponent::GetMaxPoise,
        "BindOnPostureChanged",
        [](UCombatStateComponent& C, sol::protected_function Callback)
        {
            return BindLuaPostureChanged(&C, std::move(Callback));
        },
        "UnbindOnPostureChanged",
        [](UCombatStateComponent& C, uint64 BindingId)
        {
            return UnbindLuaPostureChanged(&C, BindingId);
        },
        "SetTeam", [](UCombatStateComponent& C, int Team) { C.SetTeam(static_cast<ECombatTeam>(Team)); },
        "GetTeam", [](UCombatStateComponent& C) { return static_cast<int>(C.GetTeam()); },
        "IsHostileTo", &UCombatStateComponent::IsHostileTo
    );

    Lua.new_usertype<ULockOnComponent>(
        "LockOnComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "ToggleLockOn", &ULockOnComponent::ToggleLockOn,
        "ClearLockOn", &ULockOnComponent::ClearLockOn,
        "AddSwitchInput", &ULockOnComponent::AddSwitchInput,
        "SwitchLockTarget", &ULockOnComponent::SwitchLockTarget
    );

    Lua.new_usertype<UEnemyAIBrainComponent>(
        "EnemyAIBrainComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "SetTarget", &UEnemyAIBrainComponent::SetTarget,
        "ClearTarget", &UEnemyAIBrainComponent::ClearTarget,
        "GetTarget", &UEnemyAIBrainComponent::GetTarget,
        "HasValidTarget", &UEnemyAIBrainComponent::HasValidTarget,
        "AcquireTargetByTag", sol::overload(
            [](UEnemyAIBrainComponent& B, const FString& Tag, float SearchRange) { return B.AcquireTargetByTag(FName(Tag), SearchRange); },
            [](UEnemyAIBrainComponent& B, const FString& Tag) { return B.AcquireTargetByTag(FName(Tag), 0.0f); }
        ),
        "AcquireNearestHostileTarget", &UEnemyAIBrainComponent::AcquireNearestHostileTarget,
        "GetDistanceToTarget", &UEnemyAIBrainComponent::GetDistanceToTarget,
        "GetDirectionToTarget", &UEnemyAIBrainComponent::GetDirectionToTarget,
        "GetFlatDirectionToTarget", &UEnemyAIBrainComponent::GetFlatDirectionToTarget,
        "GetAngleToTarget", &UEnemyAIBrainComponent::GetAngleToTarget,
        "IsTargetInRange", &UEnemyAIBrainComponent::IsTargetInRange,
        "IsTargetInFront", &UEnemyAIBrainComponent::IsTargetInFront,
        "IsTargetBehind", &UEnemyAIBrainComponent::IsTargetBehind,
        "RequestMoveToTarget", sol::overload(
            [](UEnemyAIBrainComponent& B, float AcceptanceRadius, bool bUsePathfinding) { return B.RequestMoveToTarget(AcceptanceRadius, bUsePathfinding); },
            [](UEnemyAIBrainComponent& B, float AcceptanceRadius) { return B.RequestMoveToTarget(AcceptanceRadius, true); },
            [](UEnemyAIBrainComponent& B) { return B.RequestMoveToTarget(-1.0f, true); }
        ),
        "StopMove", &UEnemyAIBrainComponent::StopMove,
        "IsMoveActive", &UEnemyAIBrainComponent::IsMoveActive,
        "GetMoveStatus", [](UEnemyAIBrainComponent& B) { return static_cast<int>(B.GetMoveStatus()); },
        "GetLastMoveRequestResult", [](UEnemyAIBrainComponent& B) { return static_cast<int>(B.GetLastMoveRequestResult()); },
        "GetLastMoveResult", [](UEnemyAIBrainComponent& B) { return static_cast<int>(B.GetLastMoveResult()); },
        "GetLastMoveFailureReason", &UEnemyAIBrainComponent::GetLastMoveFailureReason
    );

    Lua.new_usertype<UCharacterStateMachineComponent>(
        "CharacterStateMachineComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "RequestState", [](UCharacterStateMachineComponent& S, const FString& State) { return S.RequestState(FName(State)); },
        "ForceState", [](UCharacterStateMachineComponent& S, const FString& State) { S.ForceState(FName(State)); },
        "DefineState", [](UCharacterStateMachineComponent& S, const FString& State, int Locomotion, int AnimStateId, bool bAllowsPolicyExit, bool bFaceTarget) { S.DefineState(FName(State), Locomotion, AnimStateId, bAllowsPolicyExit, bFaceTarget); },
        "ClearStateDefinitions", &UCharacterStateMachineComponent::ClearStateDefinitions,
        "HasStateDefinition", [](UCharacterStateMachineComponent& S, const FString& State) { return S.HasStateDefinition(FName(State)); },
        "GetState", [](UCharacterStateMachineComponent& S) { return S.GetState().ToString(); },
        "GetPreviousState", [](UCharacterStateMachineComponent& S) { return S.GetPreviousState().ToString(); },
        "GetStateInt", &UCharacterStateMachineComponent::GetStateInt,
        "GetTimeInState", &UCharacterStateMachineComponent::GetTimeInState,
        "GetLocomotionModeInt", &UCharacterStateMachineComponent::GetLocomotionModeInt,
        "IsMovementLocked", &UCharacterStateMachineComponent::IsMovementLocked,
        "CanPolicyExitCurrentState", &UCharacterStateMachineComponent::CanPolicyExitCurrentState,
        "BeginDash", &UCharacterStateMachineComponent::BeginDash,
        "SetMovementTarget", &UCharacterStateMachineComponent::SetMovementTarget,
        "GetMovementTarget", &UCharacterStateMachineComponent::GetMovementTarget
    );

    Lua.new_usertype<UEnemyAttackComponent>(
        "EnemyAttackComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "IsAttackOnCooldown", [](UEnemyAttackComponent& C, const FString& Name) { return C.IsAttackOnCooldown(FName(Name)); },
        "GetAttackCooldownRemaining", [](UEnemyAttackComponent& C, const FString& Name) { return C.GetAttackCooldownRemaining(FName(Name)); },
        "SetAttackCooldown", [](UEnemyAttackComponent& C, const FString& Name, float Cooldown) { C.SetAttackCooldown(FName(Name), Cooldown); },
        "ClearAttackCooldowns", &UEnemyAttackComponent::ClearAttackCooldowns,
        "CanUseAttack", &UEnemyAttackComponent::CanUseAttack,
        "CommitAttack", [](UEnemyAttackComponent& C, const FString& Name) { return C.CommitAttack(FName(Name)); },
        "CommitAttackData", &UEnemyAttackComponent::CommitAttackData,
        "FindAttackByName", [](UEnemyAttackComponent& C, const FString& Name) { return C.FindAttackByName(FName(Name)); },
        "GetLastAttackName", [](UEnemyAttackComponent& C) { return C.GetLastAttackName().ToString(); }
    );

    Lua.new_usertype<UPhaseComponent>(
        "PhaseComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "SetPhase", &UPhaseComponent::SetPhase,
        "GetCurrentPhase", &UPhaseComponent::GetCurrentPhase
    );

    Lua.new_usertype<UEncounterComponent>(
        "EncounterComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "StartEncounter", &UEncounterComponent::StartEncounter,
        "CompleteEncounter", &UEncounterComponent::CompleteEncounter,
        "ResetEncounter", &UEncounterComponent::ResetEncounter,
        "IsEncounterActive", &UEncounterComponent::IsEncounterActive,
        "IsEncounterCompleted", &UEncounterComponent::IsEncounterCompleted,
        "GetEncounterState", [](UEncounterComponent& C) { return static_cast<int>(C.GetEncounterState()); }
    );

    Lua.new_usertype<ABaseCombatCharacter>(
        "BaseCombatCharacter",
        sol::base_classes,
        sol::bases<ACharacter, APawn, AActor, UObject>(),
        "GetHealthComponent", &ABaseCombatCharacter::GetHealthComponent,
        "GetCombatStateComponent", &ABaseCombatCharacter::GetCombatStateComponent,
        "GetActionComponent", &ABaseCombatCharacter::GetActionComponent,
        "ApplyCombatDamage", &ABaseCombatCharacter::ApplyCombatDamage,
        "IsDead", &ABaseCombatCharacter::IsDead,
        "IsAlive", &ABaseCombatCharacter::IsAlive,
        "PlayCombatMontage", &ABaseCombatCharacter::PlayCombatMontage,
        "SetAnimGraphBool", [](ABaseCombatCharacter& C, const FString& Name, bool Value) { C.SetAnimGraphBool(FName(Name), Value); },
        "SetAnimGraphFloat", [](ABaseCombatCharacter& C, const FString& Name, float Value) { C.SetAnimGraphFloat(FName(Name), Value); },
        "SetAnimGraphInt", [](ABaseCombatCharacter& C, const FString& Name, int Value) { C.SetAnimGraphInt(FName(Name), Value); }
    );

    Lua.new_usertype<AEnemyCharacter>(
        "EnemyCharacter",
        sol::base_classes,
        sol::bases<ABaseCombatCharacter, ACharacter, APawn, AActor, UObject>(),
        "GetAIBrainComponent", &AEnemyCharacter::GetAIBrainComponent,
        "GetAttackComponent", &AEnemyCharacter::GetAttackComponent,
        "GetExecution", [](AEnemyCharacter& C) -> UActorComponent* { return C.GetExecution(); },
        "GetLuaScriptComponent", &AEnemyCharacter::GetLuaScriptComponent,
        "MoveToTarget", sol::overload(
            [](AEnemyCharacter& C, float Scale) { C.MoveToTarget(Scale); },
            [](AEnemyCharacter& C) { C.MoveToTarget(1.0f); }
        ),
        "FaceTarget", sol::overload(
            [](AEnemyCharacter& C, float DeltaTime, float YawRate) { C.FaceTarget(DeltaTime, YawRate); },
            [](AEnemyCharacter& C, float DeltaTime) { C.FaceTarget(DeltaTime, -1.0f); }
        ),
        "StopEnemyMovement", &AEnemyCharacter::StopEnemyMovement,
        "SpawnDefaultAIController", &AEnemyCharacter::SpawnDefaultAIController,
        "GetEnemyAIController", &AEnemyCharacter::GetEnemyAIController,
        "RequestMoveToTarget", sol::overload(
            [](AEnemyCharacter& C, float AcceptanceRadius, bool bUsePathfinding) { return C.RequestMoveToTarget(AcceptanceRadius, bUsePathfinding); },
            [](AEnemyCharacter& C, float AcceptanceRadius) { return C.RequestMoveToTarget(AcceptanceRadius, true); },
            [](AEnemyCharacter& C) { return C.RequestMoveToTarget(-1.0f, true); }
        ),
        "RequestMoveToActor", sol::overload(
            [](AEnemyCharacter& C, AActor* Target, float AcceptanceRadius, bool bUsePathfinding) { return C.RequestMoveToActor(Target, AcceptanceRadius, bUsePathfinding); },
            [](AEnemyCharacter& C, AActor* Target, float AcceptanceRadius) { return C.RequestMoveToActor(Target, AcceptanceRadius, true); }
        ),
        "RequestMoveToLocation", sol::overload(
            [](AEnemyCharacter& C, const FVector& Location, float AcceptanceRadius, bool bUsePathfinding) { return C.RequestMoveToLocation(Location, AcceptanceRadius, bUsePathfinding); },
            [](AEnemyCharacter& C, const FVector& Location, float AcceptanceRadius) { return C.RequestMoveToLocation(Location, AcceptanceRadius, true); }
        ),
        "IsPathFollowing", &AEnemyCharacter::IsPathFollowing,
        "PlayAttackMontage", &AEnemyCharacter::PlayAttackMontage,
        "Brain_AcquireTarget", &AEnemyCharacter::Brain_AcquireTarget,
        "Brain_Chase", &AEnemyCharacter::Brain_Chase,
        "Brain_Idle", &AEnemyCharacter::Brain_Idle,
        "Brain_IsBusy", &AEnemyCharacter::Brain_IsBusy,
        "Brain_GetDistance", &AEnemyCharacter::Brain_GetDistance,
        "Brain_GetAttackRange", &AEnemyCharacter::Brain_GetAttackRange,
        "Brain_SetSelectedAttack", [](AEnemyCharacter& C, const FString& AttackName) { return C.Brain_SetSelectedAttack(FName(AttackName)); },
        "Brain_PlaySelectedAttack", &AEnemyCharacter::Brain_PlaySelectedAttack,
        "Brain_PlayAttackByName", [](AEnemyCharacter& C, const FString& AttackName) { return C.Brain_PlayAttackByName(FName(AttackName)); }
    );

    Lua.new_usertype<ABossEnemyCharacter>(
        "BossEnemyCharacter",
        sol::base_classes,
        sol::bases<AEnemyCharacter, ABaseCombatCharacter, ACharacter, APawn, AActor, UObject>(),
        "GetPhaseComponent", &ABossEnemyCharacter::GetPhaseComponent,
        "GetEncounterComponent", &ABossEnemyCharacter::GetEncounterComponent,
        "StartBossEncounter", &ABossEnemyCharacter::StartBossEncounter,
        "CompleteBossEncounter", &ABossEnemyCharacter::CompleteBossEncounter,
        "IsBossEncounterActive", &ABossEnemyCharacter::IsBossEncounterActive,
        "SetBossPhase", &ABossEnemyCharacter::SetBossPhase,
        "GetBossPhase", &ABossEnemyCharacter::GetBossPhase
    );

    Lua.new_usertype<UActionComponent>(
        "ActionComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "HitStop",
        &UActionComponent::HitStop,
        "LocalHitStop",
        &UActionComponent::LocalHitStop,
        "HitSquash",
        &UActionComponent::HitSquash,
        "Knockback",
        &UActionComponent::Knockback,
        "Slomo",
        &UActionComponent::Slomo,
        "StopHitStop",
        &UActionComponent::StopHitStop,
        "StopLocalHitStop",
        &UActionComponent::StopLocalHitStop,
        "StopHitSquash",
        &UActionComponent::StopHitSquash,
        "StopKnockback",
        &UActionComponent::StopKnockback,
        "StopSlomo",
        &UActionComponent::StopSlomo,
        "StopAllActions",
        &UActionComponent::StopAllActions
    );

    Lua.new_usertype<UFloatingPawnMovementComponent>(
        "FloatingPawnMovementComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "SetMoveInput",
        &UFloatingPawnMovementComponent::SetMoveInput,
        "SetLookInput",
        &UFloatingPawnMovementComponent::SetLookInput
    );

    Lua.new_usertype<UWheeledVehicleMovementComponent>(
        "WheeledVehicleMovementComponent",
        sol::base_classes,
        sol::bases<UMovementComponent, UActorComponent, UObject>(),
        "SetThrottleInput",
        &UWheeledVehicleMovementComponent::SetThrottleInput,
        "SetBrakeInput",
        &UWheeledVehicleMovementComponent::SetBrakeInput,
        "SetSteeringInput",
        &UWheeledVehicleMovementComponent::SetSteeringInput,
        "SetHandbrakeInput",
        &UWheeledVehicleMovementComponent::SetHandbrakeInput,
        "ResetVehicle",
        &UWheeledVehicleMovementComponent::ResetVehicle,
        "GetForwardSpeed",
        &UWheeledVehicleMovementComponent::GetForwardSpeed,
        "IsVehicleCreated",
        &UWheeledVehicleMovementComponent::IsVehicleCreated
    );

    Lua.new_usertype<UVehicleWheelPoseComponent>(
        "VehicleWheelPoseComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "SetVehicleMovement",
        &UVehicleWheelPoseComponent::SetVehicleMovement,
        "GetVehicleMovement",
        &UVehicleWheelPoseComponent::GetVehicleMovement
    );

    Lua.new_usertype<UParticleModule>(
        "ParticleModule",
        sol::base_classes,
        sol::bases<UObject>(),
        "IsEnabled",
        &UParticleModule::IsEnabled,
        "SetEnabled",
        &UParticleModule::SetEnabled,
        "GetDisplayName",
        &UParticleModule::GetDisplayName,
        "GetCategory",
        [](UParticleModule& Module)
        {
            return static_cast<int32>(Module.GetCategory());
        },
        "IsUnique",
        &UParticleModule::IsUnique
    );

    Lua.new_usertype<UParticleLODLevel>(
        "ParticleLODLevel",
        sol::base_classes,
        sol::bases<UObject>(),
        "Level",
        sol::property(
            [](UParticleLODLevel& LOD)
            {
                return LOD.Level;
            },
            [](UParticleLODLevel& LOD, int32 Level)
            {
                LOD.Level = Level;
            }
        ),
        "Enabled",
        sol::property(
            [](UParticleLODLevel& LOD)
            {
                return LOD.bEnabled;
            },
            [](UParticleLODLevel& LOD, bool bEnabled)
            {
                LOD.bEnabled = bEnabled;
            }
        ),
        "GetRequiredModule",
        [](UParticleLODLevel& LOD) -> UParticleModuleRequired*
        {
            return LOD.RequiredModule;
        },
        "GetSpawnModule",
        [](UParticleLODLevel& LOD) -> UParticleModuleSpawn*
        {
            return LOD.SpawnModule;
        },
        "GetTypeDataModule",
        [](UParticleLODLevel& LOD) -> UParticleModule*
        {
            return LOD.TypeDataModule;
        },
        "GetModuleCount",
        [](UParticleLODLevel& LOD)
        {
            return static_cast<int32>(LOD.Modules.size());
        },
        "GetModule",
        [](UParticleLODLevel& LOD, int32 Index) -> UParticleModule*
        {
            return (Index >= 0 && Index < static_cast<int32>(LOD.Modules.size())) ? LOD.Modules[Index] : nullptr;
        },
        "GetModules",
        [](UParticleLODLevel& LOD, sol::this_state State)
        {
            sol::state_view L(State);
            sol::table      Result = L.create_table();
            int32           Idx    = 1;
            for (UParticleModule* Module : LOD.Modules) if (IsValid(Module)) Result[Idx++] = Module;
            return Result;
        },
        "AddModule",
        &UParticleLODLevel::AddModule,
        "RemoveModule",
        &UParticleLODLevel::RemoveModule,
        "AddModuleByClass",
        [](UParticleLODLevel& LOD, const FString& ClassName) -> UParticleModule*
        {
            UObject*         Obj    = FObjectFactory::Get().Create(ClassName, &LOD);
            UParticleModule* Module = Cast<UParticleModule>(Obj);
            if (!IsValid(Module))
            {
                if (IsValid(Obj)) UObjectManager::Get().DestroyObject(Obj);
                return nullptr;
            }
            if (!LOD.AddModule(Module))
            {
                UObjectManager::Get().DestroyObject(Module);
                return nullptr;
            }
            return Module;
        },
        "ValidateModules",
        &UParticleLODLevel::ValidateModules,
        "UpdateFromLOD0",
        &UParticleLODLevel::UpdateFromLOD0
    );

    Lua.new_usertype<UParticleEmitter>(
        "ParticleEmitter",
        sol::base_classes,
        sol::bases<UObject>(),
        "Name",
        sol::property(
            [](UParticleEmitter& Emitter)
            {
                return Emitter.EmitterName;
            },
            [](UParticleEmitter& Emitter, const FString& Name)
            {
                Emitter.EmitterName = Name;
            }
        ),
        "Enabled",
        sol::property(&UParticleEmitter::IsEnabled, &UParticleEmitter::SetEnabled),
        "IsEnabled",
        &UParticleEmitter::IsEnabled,
        "SetEnabled",
        &UParticleEmitter::SetEnabled,
        "GetQualityLevelSpawnRateMult",
        &UParticleEmitter::GetQualityLevelSpawnRateMult,
        "SetQualityLevelSpawnRateMult",
        &UParticleEmitter::SetQualityLevelSpawnRateMult,
        "InitializeDefaultLODLevel",
        &UParticleEmitter::InitializeDefaultLODLevel,
        "EnsureLODCoreModules",
        &UParticleEmitter::EnsureLODCoreModules,
        "CreateLODLevel",
        &UParticleEmitter::CreateLODLevel,
        "RemoveLODLevel",
        &UParticleEmitter::RemoveLODLevel,
        "GetLODLevel",
        &UParticleEmitter::GetLODLevel,
        "GetCurrentLODLevel",
        &UParticleEmitter::GetCurrentLODLevel,
        "GetLODCount",
        &UParticleEmitter::GetLODCount,
        "CacheEmitterModuleInfo",
        &UParticleEmitter::CacheEmitterModuleInfo,
        "GetParticleSize",
        &UParticleEmitter::GetParticleSize,
        "GetReqInstanceBytes",
        &UParticleEmitter::GetReqInstanceBytes
    );

    Lua.new_usertype<UParticleSystem>(
        "ParticleSystem",
        sol::base_classes,
        sol::bases<UObject>(),
        "Looping",
        sol::property(
            [](UParticleSystem& System)
            {
                return System.bLooping;
            },
            [](UParticleSystem& System, bool bLooping)
            {
                System.bLooping = bLooping;
            }
        ),
        "UpdateTimeFPS",
        sol::property(
            [](UParticleSystem& System)
            {
                return System.UpdateTimeFPS;
            },
            [](UParticleSystem& System, float FPS)
            {
                System.UpdateTimeFPS = FPS;
            }
        ),
        "AddEmitter",
        &UParticleSystem::AddEmitter,
        "RemoveEmitter",
        &UParticleSystem::RemoveEmitter,
        "MoveEmitter",
        &UParticleSystem::MoveEmitter,
        "GetEmitterCount",
        &UParticleSystem::GetEmitterCount,
        "GetEmitter",
        &UParticleSystem::GetEmitter,
        "GetMaxLODCount",
        &UParticleSystem::GetMaxLODCount,
        "EnsureLODDistances",
        &UParticleSystem::EnsureLODDistances,
        "GetLODIndexForDistance",
        &UParticleSystem::GetLODIndexForDistance,
        "GetLODDistance",
        &UParticleSystem::GetLODDistance,
        "SetLODDistance",
        &UParticleSystem::SetLODDistance,
        "BuildEmitters",
        &UParticleSystem::BuildEmitters,
        "GetSourcePath",
        &UParticleSystem::GetSourcePath,
        "SetSourcePath",
        &UParticleSystem::SetSourcePath
    );

    Lua.new_usertype<UParticleSystemComponent>(
        "ParticleSystemComponent",
        sol::base_classes,
        sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "SetTemplate",
        &UParticleSystemComponent::SetTemplate,
        "SetTemplateByPath",
        [](UParticleSystemComponent& Component, const FString& Path)
        {
            Component.SetTemplate(FParticleSystemManager::Get().Load(Path));
        },
        "GetTemplate",
        &UParticleSystemComponent::GetTemplate,
        "Activate",
        &UParticleSystemComponent::Activate,
        "Deactivate",
        &UParticleSystemComponent::Deactivate,
        "ResetParticles",
        &UParticleSystemComponent::ResetParticles,
        "IsActive",
        &UParticleSystemComponent::IsActive,
        "GetEmitterInstanceCount",
        &UParticleSystemComponent::GetEmitterInstanceCount,
        "GetCurrentLODIndex",
        &UParticleSystemComponent::GetCurrentLODIndex,
        "SetCurrentLODIndex",
        &UParticleSystemComponent::SetCurrentLODIndex,
        "SetRibbonEdgeSourceComponents",
        &UParticleSystemComponent::SetRibbonEdgeSourceComponents,
        "ClearRibbonEdgeSourceComponents",
        &UParticleSystemComponent::ClearRibbonEdgeSourceComponents,
        "HasRibbonEdgeSourceComponents",
        &UParticleSystemComponent::HasRibbonEdgeSourceComponents,
        "RebuildInstances",
        &UParticleSystemComponent::RebuildInstances,
        "GetTemplatePath",
        &UParticleSystemComponent::GetTemplatePath
    );

    Lua.new_usertype<AParticleSystemActor>(
        "ParticleSystemActor",
        sol::base_classes,
        sol::bases<AActor, UObject>(),
        "GetParticleSystemComponent",
        &AParticleSystemActor::GetParticleSystemComponent
    );

    sol::table Particle = Lua.create_named_table("Particle");
    Particle.set_function(
        "LoadSystem",
        [](const FString& Path) -> UParticleSystem*
        {
            return FParticleSystemManager::Get().Load(Path);
        }
    );
    Particle.set_function(
        "FindSystem",
        [](const FString& Path) -> UParticleSystem*
        {
            return FParticleSystemManager::Get().Find(Path);
        }
    );
    Particle.set_function(
        "SaveSystem",
        [](UParticleSystem* System)
        {
            return FParticleSystemManager::Get().Save(System);
        }
    );
    Particle.set_function(
        "NewSystem",
        []() -> UParticleSystem*
        {
            return UObjectManager::Get().CreateObject<UParticleSystem>();
        }
    );
    Particle.set_function(
        "NewEmitter",
        [](UObject* Outer) -> UParticleEmitter*
        {
            return UObjectManager::Get().CreateObject<UParticleEmitter>(Outer);
        }
    );
    Particle.set_function(
        "NewModule",
        [](const FString& ClassName, UObject* Outer) -> UParticleModule*
        {
            UObject* Obj = FObjectFactory::Get().Create(ClassName, Outer);
            return Cast<UParticleModule>(Obj);
        }
    );
    Particle.set_function(
        "AddEmitter",
        [](UParticleSystem* System) -> UParticleEmitter*
        {
            return IsValid(System) ? System->AddEmitter() : nullptr;
        }
    );
    Particle.set_function(
        "AddModule",
        [](UParticleLODLevel* LOD, const FString& ClassName) -> UParticleModule*
        {
            if (!IsValid(LOD)) return nullptr;
            UObject*         Obj    = FObjectFactory::Get().Create(ClassName, LOD);
            UParticleModule* Module = Cast<UParticleModule>(Obj);
            if (!IsValid(Module))
            {
                if (IsValid(Obj)) UObjectManager::Get().DestroyObject(Obj);
                return nullptr;
            }
            if (!LOD->AddModule(Module))
            {
                UObjectManager::Get().DestroyObject(Module);
                return nullptr;
            }
            return Module;
        }
    );
    Particle.set_function(
        "SetComponentTemplate",
        [](UParticleSystemComponent* Component, UParticleSystem* System)
        {
            if (IsValid(Component)) Component->SetTemplate(System);
        }
    );
    Particle.set_function(
        "SetComponentTemplateByPath",
        [](UParticleSystemComponent* Component, const FString& Path)
        {
            if (IsValid(Component)) Component->SetTemplate(FParticleSystemManager::Get().Load(Path));
        }
    );

    Lua.new_usertype<USceneComponent>(
        "SceneComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "Location",
        sol::property(
            [](USceneComponent& Component)
            {
                return Component.GetWorldLocation();
            },
            [](USceneComponent& Component, const FVector& Location)
            {
                Component.SetWorldLocation(Location);
            }
        ),
        "Rotation",
        sol::property(
            [](USceneComponent& Component)
            {
                return Component.GetRelativeRotation().ToVector();
            },
            [](USceneComponent& Component, const FVector& Rotation)
            {
                Component.SetRelativeRotation(Rotation);
            }
        ),
        "Forward",
        sol::property(
            [](USceneComponent& Component)
            {
                return Component.GetForwardVector();
            }
        ),
        "Right",
        sol::property(
            [](USceneComponent& Component)
            {
                return Component.GetRightVector();
            }
        ),
        "Up",
        sol::property(
            [](USceneComponent& Component)
            {
                return Component.GetUpVector();
            }
        ),
        "GetLocation",
        [](USceneComponent& Component)
        {
            return Component.GetWorldLocation();
        },
        "SetLocation",
        [](USceneComponent& Component, const FVector& Location)
        {
            Component.SetWorldLocation(Location);
        },
        "GetRotation",
        [](USceneComponent& Component)
        {
            return Component.GetRelativeRotation().ToVector();
        },
        "SetRotation",
        [](USceneComponent& Component, const FVector& Rotation)
        {
            Component.SetRelativeRotation(Rotation);
        },

        // 부모 기준 상대 위치 — 동일한 메시를 4개 깐 바퀴 같은 케이스에서 앞/뒤 구분 등
        // 위치 기반 필터링에 쓰인다. 월드 위치는 위 "Location" 프로퍼티 참고.
        "RelativeLocation",
        sol::property(
            [](USceneComponent& Component)
            {
                return Component.GetRelativeLocation();
            },
            [](USceneComponent& Component, const FVector& V)
            {
                Component.SetRelativeLocation(V);
            }
        )
    );

    Lua.new_usertype<UPrimitiveComponent>(
        "PrimitiveComponent",
        sol::base_classes,
        sol::bases<USceneComponent, UActorComponent, UObject>(),
        "IsValid",
        [](UPrimitiveComponent* Component)
        {
            return IsValid(Component);
        },
        "SetSimulatePhysics",
        [](UPrimitiveComponent* Component, bool bSimulate)
        {
            if (IsValid(Component)) Component->SetSimulatePhysics(bSimulate);
        },
        "GetSimulatePhysics",
        [](UPrimitiveComponent* Component) -> bool
        {
            return IsValid(Component) ? Component->GetSimulatePhysics() : false;
        },
        "AddForce",
        [](UPrimitiveComponent* Component, const FVector& Force)
        {
            if (IsValid(Component)) Component->AddForce(Force);
        },
        "AddForceAtLocation",
        [](UPrimitiveComponent* Component, const FVector& Force, const FVector& Location)
        {
            if (IsValid(Component)) Component->AddForceAtLocation(Force, Location);
        },
        "AddTorque",
        [](UPrimitiveComponent* Component, const FVector& Torque)
        {
            if (IsValid(Component)) Component->AddTorque(Torque);
        },
        "AddImpulse",
        [](UPrimitiveComponent* Component, const FVector& Impulse)
        {
            if (IsValid(Component)) Component->AddImpulse(Impulse);
        },
        "GetLinearVelocity",
        [](UPrimitiveComponent* Component) -> FVector
        {
            return IsValid(Component) ? Component->GetLinearVelocity() : FVector::ZeroVector;
        },
        "SetLinearVelocity",
        [](UPrimitiveComponent* Component, const FVector& Vel)
        {
            if (IsValid(Component)) Component->SetLinearVelocity(Vel);
        },
        "GetAngularVelocity",
        [](UPrimitiveComponent* Component) -> FVector
        {
            return IsValid(Component) ? Component->GetAngularVelocity() : FVector::ZeroVector;
        },
        "SetAngularVelocity",
        [](UPrimitiveComponent* Component, const FVector& Vel)
        {
            if (IsValid(Component)) Component->SetAngularVelocity(Vel);
        },
        "GetMass",
        [](UPrimitiveComponent* Component) -> float
        {
            return IsValid(Component) ? Component->GetMass() : 0.0f;
        },
        "SetMass",
        [](UPrimitiveComponent* Component, float Mass)
        {
            if (IsValid(Component)) Component->SetMass(Mass);
        },
        "GetGenerateOverlapEvents",
        [](UPrimitiveComponent* Component) -> bool
        {
            return IsValid(Component) ? Component->GetGenerateOverlapEvents() : false;
        }
    );

    Lua.new_usertype<UStaticMesh>(
        "StaticMesh",
        sol::base_classes,
        sol::bases<UObject>(),
        "AssetPath",
        sol::property(
            [](UStaticMesh& Mesh)
            {
                return Mesh.GetAssetPathFileName();
            }
        ),
        "GetAssetPath",
        [](UStaticMesh& Mesh)
        {
            return Mesh.GetAssetPathFileName();
        }
    );

    // 메시 에셋 경로로 컴포넌트 식별 가능하게 노출. 자동 생성된 FName ("UStaticMeshComponent_41")
    // 은 월드 초기화 순서에 따라 카운터가 달라져 빌드별로 매칭이 깨질 수 있다. 메시 경로는
    // 씬 파일에 명시 저장되므로 deterministic.
    Lua.new_usertype<UStaticMeshComponent>(
        "StaticMeshComponent",
        sol::base_classes,
        sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "MeshPath",
        sol::property(
            [](UStaticMeshComponent& C)
            {
                return C.GetStaticMeshPath();
            }
        ),
        "GetMeshPath",
        [](UStaticMeshComponent& C)
        {
            return C.GetStaticMeshPath();
        },
        "SetStaticMesh",
        &UStaticMeshComponent::SetStaticMesh,
        "SetStaticMeshByPath",
        &UStaticMeshComponent::SetStaticMeshByPath,
        "ClearStaticMesh",
        &UStaticMeshComponent::ClearStaticMesh,
        "GetStaticMesh",
        &UStaticMeshComponent::GetStaticMesh,
        "SetMaterialByPath",
        &UStaticMeshComponent::SetMaterialByPath,
        "SetMaterial",
        &UStaticMeshComponent::SetMaterial,
        "GetMaterial",
        &UStaticMeshComponent::GetMaterial,
        "GetMaterialPath",
        &UStaticMeshComponent::GetMaterialPath,
        "GetMaterialSlotCount",
        &UStaticMeshComponent::GetMaterialSlotCount
    );

    Lua.new_usertype<USkinnedMeshComponent>(
        "SkinnedMeshComponent",
        sol::base_classes,
        sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "SetSkeletalMeshByPath",
        &USkinnedMeshComponent::SetSkeletalMeshByPath,
        "ClearSkeletalMesh",
        &USkinnedMeshComponent::ClearSkeletalMesh,
        "GetSkeletalMesh",
        &USkinnedMeshComponent::GetSkeletalMesh,
        "GetSkeletalMeshPathValue",
        &USkinnedMeshComponent::GetSkeletalMeshPathValue,
        "SetMaterialByPath",
        &USkinnedMeshComponent::SetMaterialByPath,
        "SetMaterial",
        &USkinnedMeshComponent::SetMaterial,
        "GetMaterial",
        &USkinnedMeshComponent::GetMaterial,
        "GetMaterialPath",
        &USkinnedMeshComponent::GetMaterialPath,
        "GetMaterialSlotCount",
        &USkinnedMeshComponent::GetMaterialSlotCount
    );

    Lua.new_usertype<USkeletalMeshComponent>(
        "SkeletalMeshComponent",
        sol::base_classes,
        sol::bases<USkinnedMeshComponent, UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "PlayAnimationByPath",
        &USkeletalMeshComponent::PlayAnimationByPath,
        "StopAnimation",
        &USkeletalMeshComponent::StopAnimation,
        "SetAnimationByPath",
        &USkeletalMeshComponent::SetAnimationByPath,
        "SetPlayRate",
        &USkeletalMeshComponent::SetPlayRate,
        "SetLooping",
        &USkeletalMeshComponent::SetLooping,
        "SetPlaying",
        &USkeletalMeshComponent::SetPlaying,
        "GetAnimInstance",
        &USkeletalMeshComponent::GetAnimInstance,
        "GetAnimationMode",
        &USkeletalMeshComponent::GetAnimationMode,
        "GetAnimation",
        &USkeletalMeshComponent::GetAnimation
    );

    Lua.new_usertype<FHitResult>(
        "HitResult",
        "HitComponent",
        sol::property(
            [](const FHitResult& Hit) -> UPrimitiveComponent*
            {
                return IsValid(Hit.HitComponent) ? Hit.HitComponent : nullptr;
            }
        ),
        "HitActor",
        sol::property(
            [](const FHitResult& Hit) -> AActor*
            {
                return IsValid(Hit.HitActor) ? Hit.HitActor : nullptr;
            }
        ),
        "GetHitComponent",
        [](const FHitResult& Hit) -> UPrimitiveComponent*
        {
            return IsValid(Hit.HitComponent) ? Hit.HitComponent : nullptr;
        },
        "GetHitActor",
        [](const FHitResult& Hit) -> AActor*
        {
            return IsValid(Hit.HitActor) ? Hit.HitActor : nullptr;
        },
        "Distance",
        &FHitResult::Distance,
        "PenetrationDepth",
        &FHitResult::PenetrationDepth,
        "WorldHitLocation",
        &FHitResult::WorldHitLocation,
        "WorldNormal",
        &FHitResult::WorldNormal,
        "ImpactNormal",
        &FHitResult::ImpactNormal,
        "FaceIndex",
        &FHitResult::FaceIndex,
        "bHit",
        &FHitResult::bHit
    );

    Lua.new_usertype<UCameraComponent>(
        "CameraComponent",
        sol::base_classes,
        sol::bases<USceneComponent, UActorComponent, UObject>(),
        "LookAt",
        &UCameraComponent::LookAt,
        "SetFOV",
        &UCameraComponent::SetFOV,
        "GetFOV",
        &UCameraComponent::GetFOV,
        "SetAspectRatio",
        &UCameraComponent::SetAspectRatio,
        "GetAspectRatio",
        &UCameraComponent::GetAspectRatio,
        "SetNearPlane",
        &UCameraComponent::SetNearPlane,
        "GetNearPlane",
        &UCameraComponent::GetNearPlane,
        "SetFarPlane",
        &UCameraComponent::SetFarPlane,
        "GetFarPlane",
        &UCameraComponent::GetFarPlane,
        "SetOrthoWidth",
        &UCameraComponent::SetOrthoWidth,
        "GetOrthoWidth",
        &UCameraComponent::GetOrthoWidth,
        "SetOrthographic",
        &UCameraComponent::SetOrthographic,
        "IsOrthographic",
        &UCameraComponent::IsOrthogonal,
        "SetLetterbox",
        [](UCameraComponent& Camera, bool bEnabled, sol::optional<float> Amount, sol::optional<float> Thickness, sol::optional<sol::object> Color)
        {
            Camera.SetLetterboxEnabled(bEnabled);
            if (Amount) Camera.SetLetterboxAmount(Amount.value());
            if (Thickness) Camera.SetLetterboxThickness(Thickness.value());
            if (Color)
            {
                FVector4 ColorValue;
                if (LuaObjectToVector4(Color.value(), ColorValue))
                {
                    Camera.SetLetterboxColor(FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W));
                }
            }
        },
        "ClearLetterbox",
        [](UCameraComponent& Camera)
        {
            Camera.SetLetterboxEnabled(false);
        },
        "OnResize",
        &UCameraComponent::OnResize
    );

    Lua.new_usertype<AActor>(
        "Actor",
        sol::base_classes,
        sol::bases<UObject>(),
        "Location",
        sol::property(
            [](AActor& Actor)
            {
                return Actor.GetActorLocation();
            },
            [](AActor& Actor, const FVector& Location)
            {
                Actor.SetActorLocation(Location);
            }
        ),
        "Rotation",
        sol::property(
            [](AActor& Actor)
            {
                return Actor.GetActorRotation().ToVector();
            },
            [](AActor& Actor, const FVector& Rotation)
            {
                Actor.SetActorRotation(Rotation);
            }
        ),

        "Scale",
        sol::property(
            [](AActor& Actor)
            {
                return Actor.GetActorScale();
            },
            [](AActor& Actor, const FVector& Scale)
            {
                Actor.SetActorScale(Scale);
            }
        ),

        "Forward",
        sol::property(
            [](AActor& Actor)
            {
                return Actor.GetActorForward();
            }
        ),

        "Right",
        sol::property(
            [](AActor& Actor)
            {
                return Actor.GetActorRight();
            }
        ),

        "AddWorldOffset",
        [](AActor& Actor, const FVector& Offset)
        {
            Actor.AddActorWorldOffset(Offset);
        },

        "Destroy",
        [](AActor& Actor)
        {
            // World->DestroyActor가 EndPlay + 정리. Lua는 호출 후 해당 액터를 더 참조하지 말 것.
            if (UWorld* W = Actor.GetWorld()) W->DestroyActor(&Actor);
        },

        "IsValid",
        [](AActor* Actor)
        {
            // Lua가 보유한 actor 핸들이 cpp 측에서 destroy됐는지 확인. nil/destroyed면 false.
            return IsValid(Actor);
        },

        "HasTag",
        [](AActor& Actor, const FString& Tag)
        {
            return Actor.HasTag(FName(Tag));
        },
        "AddTag",
        [](AActor& Actor, const FString& Tag)
        {
            Actor.AddTag(FName(Tag));
        },
        "RemoveTag",
        [](AActor& Actor, const FString& Tag)
        {
            Actor.RemoveTag(FName(Tag));
        },
        "GetTags",
        [](AActor& Actor) -> sol::table
        {
            sol::table Result = FLuaScriptManager::GetState().create_table();
            int        Index  = 1;
            for (const FName& Tag : Actor.GetTags())
            {
                Result[Index++] = Tag.ToString();
            }
            return Result;
        },
        "SetTags",
        [](AActor& Actor, sol::table Tags)
        {
            TArray<FName> Names;
            for (auto& Entry : Tags)
            {
                sol::object Value = Entry.second;
                if (Value.is<std::string>()) Names.push_back(FName(FString(Value.as<std::string>())));
            }
            Actor.SetTags(Names);
        },
        "GetComponents",
        [](AActor& Actor) -> sol::table
        {
            sol::table Result = FLuaScriptManager::GetState().create_table();
            int        Index  = 1;
            for (UActorComponent* Component : Actor.GetComponents())
            {
                if (IsValid(Component)) Result[Index++] = Component;
            }
            return Result;
        },

        "GetCapsuleComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UCapsuleComponent>();
        },

        "GetFloatingPawnMovement",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UFloatingPawnMovementComponent>();
        },

        "GetVehicleMovement",
        [](AActor& Actor) -> UWheeledVehicleMovementComponent*
        {
            if (UWheeledVehicleMovementComponent* Movement = Actor.GetComponentByClass<UWheeledVehicleMovementComponent>())
            {
                return Movement;
            }
            return nullptr;
        },

        "GetStaticMeshComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UStaticMeshComponent>();
        },

        "GetCamera",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UCameraComponent>();
        },

        "GetSkeletalMeshComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<USkeletalMeshComponent>();
        },

        "GetSkinnedMeshComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<USkinnedMeshComponent>();
        },

        "GetLuaBlueprintComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<ULuaBlueprintComponent>();
        },

        "GetLuaScriptComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<ULuaScriptComponent>();
        },

        "GetParticleSystemComponent",
        [](AActor& Actor) -> UParticleSystemComponent*
        {
            if (AParticleSystemActor* ParticleActor = Cast<AParticleSystemActor>(&Actor))
            {
                return ParticleActor->GetParticleSystemComponent();
            }
            return Actor.GetComponentByClass<UParticleSystemComponent>();
        },

        "GetActionComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UActionComponent>();
        },

        "GetHealthComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UHealthComponent>();
        },

        "GetCombatHitEventComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UCombatHitEventComponent>();
        },

        "GetCombatStateComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UCombatStateComponent>();
        },

        "GetExecutionComponent",
        [](AActor& Actor) -> UActorComponent*
        {
            return Actor.GetComponentByClass<UExecutionComponent>();
        },

        "GetLockOnComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<ULockOnComponent>();
        },

        "GetEnemyAIBrainComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UEnemyAIBrainComponent>();
        },

        "GetEnemyAttackComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UEnemyAttackComponent>();
        },

        "GetPhaseComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UPhaseComponent>();
        },

        "GetEncounterComponent",
        [](AActor& Actor)
        {
            return Actor.GetComponentByClass<UEncounterComponent>();
        },

        "GetRootComponent",
        [](AActor& Actor) -> USceneComponent*
        {
            return Actor.GetRootComponent();
        },

        "GetRootPrimitiveComponent",
        [](AActor& Actor) -> UPrimitiveComponent*
        {
            return Cast<UPrimitiveComponent>(Actor.GetRootComponent());
        },

        "AddForceToRoot",
        [](AActor& Actor, const FVector& Force)
        {
            UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor.GetRootComponent());
            if (IsValid(Root)) Root->AddForce(Force);
        },
        "AddTorqueToRoot",
        [](AActor& Actor, const FVector& Torque)
        {
            UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor.GetRootComponent());
            if (IsValid(Root)) Root->AddTorque(Torque);
        },
        "AddImpulseToRoot",
        [](AActor& Actor, const FVector& Impulse)
        {
            UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor.GetRootComponent());
            if (IsValid(Root)) Root->AddImpulse(Impulse);
        },
        "GetRootLinearVelocity",
        [](AActor& Actor) -> FVector
        {
            UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor.GetRootComponent());
            return IsValid(Root) ? Root->GetLinearVelocity() : FVector::ZeroVector;
        },
        "SetRootLinearVelocity",
        [](AActor& Actor, const FVector& Velocity)
        {
            UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor.GetRootComponent());
            if (IsValid(Root)) Root->SetLinearVelocity(Velocity);
        },
        "SetRootSimulatePhysics",
        [](AActor& Actor, bool bSimulate)
        {
            UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor.GetRootComponent());
            if (IsValid(Root)) Root->SetSimulatePhysics(bSimulate);
        },

        "GetPrimitiveComponent",
        [](AActor& Actor) -> UPrimitiveComponent*
        {
            return Actor.GetComponentByClass<UPrimitiveComponent>();
        },

        "GetPrimitiveComponentByName",
        [](AActor& Actor, const FString& ComponentName) -> UPrimitiveComponent*
        {
            for (UActorComponent* Component : Actor.GetComponents())
            {
                UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
                if (PrimitiveComponent && PrimitiveComponent->GetFName().ToString() == ComponentName)
                {
                    return PrimitiveComponent;
                }
            }
            return nullptr;
        },

        "GetComponentByName",
        [](AActor& Actor, const FString& ComponentName) -> USceneComponent*
        {
            for (UActorComponent* Component : Actor.GetComponents())
            {
                USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
                if (SceneComponent && SceneComponent->GetFName().ToString() == ComponentName)
                {
                    return SceneComponent;
                }
            }
            return nullptr;
        },

        "UUID",
        sol::property(
            [](AActor& Actor)
            {
                return Actor.GetUUID();
            }
        ),

        "Name",
        sol::property(
            [](AActor& Actor)
            {
                return Actor.GetFName().ToString();
            }
        )
    );

    Lua.new_usertype<APlayerController>(
        "PlayerController",
        sol::base_classes,
        sol::bases<AController, AActor, UObject>(),
        "Possess",
        &APlayerController::Possess,
        "UnPossess",
        &APlayerController::UnPossess,
        "GetPossessedPawn",
        &APlayerController::GetPossessedPawn,
        "GetPlayerCameraManager",
        &APlayerController::GetPlayerCameraManager,
        "SetViewTargetWithBlend",
        [](APlayerController& Self, AActor* Target, sol::optional<float> BlendTime)
        {
            if (IsValid(Target))
            {
                Self.SetViewTargetWithBlend(Target, BlendTime.value_or(0.0f));
            }
        }
    );

    Lua.new_usertype<APawn>(
        "Pawn",
        sol::base_classes,
        sol::bases<AActor, UObject>(),
        "GetController",
        &APawn::GetController,
        "GetPlayerController",
        &APawn::GetPlayerController,
        "GetAIController",
        &APawn::GetAIController,
        "IsPossessed",
        &APawn::IsPossessed,
        "SetAutoPossessPlayer",
        &APawn::SetAutoPossessPlayer,
        "GetAutoPossessPlayer",
        &APawn::GetAutoPossessPlayer,
        "GetInputComponent",
        &APawn::GetInputComponent
    );

    Lua.new_usertype<AWheeledVehiclePawn>(
        "WheeledVehiclePawn",
        sol::base_classes,
        sol::bases<APawn, AActor, UObject>(),
        "GetMesh",
        &AWheeledVehiclePawn::GetMesh,
        "GetVehicleMovement",
        &AWheeledVehiclePawn::GetVehicleMovement,
        "GetWheelPoseComponent",
        &AWheeledVehiclePawn::GetWheelPoseComponent,
        "GetSpringArm",
        &AWheeledVehiclePawn::GetSpringArm,
        "GetCamera",
        &AWheeledVehiclePawn::GetCamera
    );

    // UInputComponent — Pawn::GetInputComponent 로 얻어 lua 에서 직접 매핑/binding 추가 가능.
    // 예 (BeginPlay 안):
    //   local input = obj:AsPawn():GetInputComponent()
    //   input:AddActionMapping("Jump", "Space")
    //   input:BindAction("Jump", "Pressed", function() print("jump!") end)
    Lua.new_usertype<UInputComponent>(
        "InputComponent",
        sol::base_classes,
        sol::bases<UActorComponent, UObject>(),
        "AddAxisMapping",
        sol::overload(
            [](UInputComponent& Self, const FString& Name, const FString& KeyName, float Scale)
            {
                Self.AddAxisMapping(Name, ResolveInputKeyCode(KeyName), Scale);
            },
            [](UInputComponent& Self, const FString& Name, const FString& KeyName)
            {
                Self.AddAxisMapping(Name, ResolveInputKeyCode(KeyName), 1.0f);
            },
            [](UInputComponent& Self, const FString& Name, int32 KeyCode, float Scale)
            {
                Self.AddAxisMapping(Name, KeyCode, Scale);
            },
            [](UInputComponent& Self, const FString& Name, int32 KeyCode)
            {
                Self.AddAxisMapping(Name, KeyCode, 1.0f);
            }
        ),
        "AddMouseAxisMapping",
        sol::overload(
            [](UInputComponent& Self, const FString& Name, const FString& AxisName, float Scale)
            {
                EInputAxisSourceType Axis = EInputAxisSourceType::MouseX;
                if (AxisName == "MouseY") Axis = EInputAxisSourceType::MouseY;
                else if (AxisName == "MouseWheel") Axis = EInputAxisSourceType::MouseWheel;
                Self.AddMouseAxisMapping(Name, Axis, Scale);
            },
            [](UInputComponent& Self, const FString& Name, const FString& AxisName)
            {
                EInputAxisSourceType Axis = EInputAxisSourceType::MouseX;
                if (AxisName == "MouseY") Axis = EInputAxisSourceType::MouseY;
                else if (AxisName == "MouseWheel") Axis = EInputAxisSourceType::MouseWheel;
                Self.AddMouseAxisMapping(Name, Axis, 1.0f);
            }
        ),
        "AddActionMapping",
        sol::overload(
            [](UInputComponent& Self, const FString& Name, const FString& KeyName)
            {
                Self.AddActionMapping(Name, ResolveInputKeyCode(KeyName));
            },
            [](UInputComponent& Self, const FString& Name, int32 KeyCode)
            {
                Self.AddActionMapping(Name, KeyCode);
            }
        ),
        "BindAxis",
        [](UInputComponent& Self, const FString& Name, sol::protected_function Cb)
        {
            Self.BindAxis(
                Name,
                [Cb](float V)
                {
                    FScopedGarbageCollectionBlocker GCBlocker;
                    auto                            R = Cb(V);
                    if (!R.valid())
                    {
                        sol::error e = R;
                        UE_LOG("[Lua] BindAxis cb error: %s", e.what());
                    }
                }
            );
        },
        "BindAction",
        [](UInputComponent& Self, const FString& Name, const FString& EventStr, sol::protected_function Cb)
        {
            const EInputEvent Ev = (EventStr == "Released") ? EInputEvent::Released : EInputEvent::Pressed;
            Self.BindAction(
                Name,
                Ev,
                [Cb]()
                {
                    FScopedGarbageCollectionBlocker GCBlocker;
                    auto                            R = Cb();
                    if (!R.valid())
                    {
                        sol::error e = R;
                        UE_LOG("[Lua] BindAction cb error: %s", e.what());
                    }
                }
            );
        },
        "ClearBindings",
        &UInputComponent::ClearBindings
    );

    // --- World binding — 런타임 액터 spawn 용 (Engine 일반 기능) ---
    sol::table World = Lua.create_named_table("World");
    World.set_function(
        "GetFirstPlayerController",
        []() -> APlayerController*
        {
            return (GEngine && GEngine->GetWorld()) ? GEngine->GetWorld()->GetFirstPlayerController() : nullptr;
        }
    );
    World.set_function(
        "BindOnPlayerPawnChanged",
        [](sol::protected_function Callback)
        {
            UWorld* Target = GEngine ? GEngine->GetWorld() : nullptr;
            return BindLuaPlayerPawnChanged(Target, std::move(Callback));
        }
    );
    World.set_function(
        "UnbindOnPlayerPawnChanged",
        [](uint64 BindingId)
        {
            UWorld* Target = GEngine ? GEngine->GetWorld() : nullptr;
            return UnbindLuaPlayerPawnChanged(Target, BindingId);
        }
    );
    World.set_function(
        "GetNavigationSystem",
        []() -> UNavigationSystem*
        {
            return (GEngine && GEngine->GetWorld()) ? GEngine->GetWorld()->GetNavigationSystem() : nullptr;
        }
    );
    World.set_function(
        "SpawnActor",
        [](const FString& ClassName, sol::optional<FVector> Location, sol::optional<FVector> Rotation, sol::optional<FVector> Scale) -> AActor*
        {
            if (!GEngine) return nullptr;
            UWorld* W = GEngine->GetWorld();
            if (!W) return nullptr;
            UClass* Cls = UClass::FindByName(ClassName.c_str());
            if (!Cls) return nullptr;
            AActor* Actor = W->SpawnActorByClass(Cls);
            if (IsValid(Actor))
            {
                Actor->SetActorLocation(Location.value_or(FVector(0, 0, 0)));
                Actor->SetActorRotation(Rotation.value_or(FVector(0, 0, 0)));
                Actor->SetActorScale(Scale.value_or(FVector(1, 1, 1)));
            }
            return Actor;
        }
    );
    World.set_function(
        "SpawnPawn",
        [](const FString& ClassName, sol::optional<FVector> Location, sol::optional<FVector> Rotation, sol::optional<FVector> Scale, sol::optional<bool> bPossess) -> APawn*
        {
            if (!GEngine) return nullptr;
            UWorld* W = GEngine->GetWorld();
            if (!W) return nullptr;
            UClass* Cls = UClass::FindByName(ClassName.c_str());
            if (!Cls) return nullptr;
            AActor* Actor = W->SpawnActorByClass(Cls);
            APawn*  Pawn  = Cast<APawn>(Actor);
            if (!IsValid(Pawn))
            {
                if (IsValid(Actor)) W->DestroyActor(Actor);
                return nullptr;
            }
            Pawn->SetActorLocation(Location.value_or(FVector(0, 0, 0)));
            Pawn->SetActorRotation(Rotation.value_or(FVector(0, 0, 0)));
            Pawn->SetActorScale(Scale.value_or(FVector(1, 1, 1)));
            if (bPossess.value_or(false))
            {
                if (APlayerController* PC = W->GetFirstPlayerController()) PC->Possess(Pawn);
            }
            return Pawn;
        }
    );
    World.set_function(
        "FindActorByName",
        [](const FString& ActorName) -> AActor*
        {
            if (!GEngine || !GEngine->GetWorld()) return nullptr;
            UWorld* W = GEngine->GetWorld();
            for (AActor* Actor : W->GetActors())
            {
                if (IsValid(Actor) && Actor->GetFName().ToString() == ActorName)
                {
                    return Actor;
                }
            }
            return nullptr;
        }
    );
    World.set_function(
        "FindFirstActorByClass",
        [](const FString& ClassName) -> AActor*
        {
            if (!GEngine || !GEngine->GetWorld()) return nullptr;
            UWorld* W   = GEngine->GetWorld();
            UClass* Cls = UClass::FindByName(ClassName.c_str());
            if (!Cls) return nullptr;
            for (AActor* Actor : W->GetActors())
            {
                if (IsValid(Actor) && Actor->GetClass()->IsA(Cls))
                {
                    return Actor;
                }
            }
            return nullptr;
        }
    );
    World.set_function(
        "FindFirstActorByTag",
        [](const FString& Tag) -> AActor*
        {
            return FGameplayStatics::FindFirstActorByTag(
                GEngine ? GEngine->GetWorld() : nullptr,
                FName(Tag)
            );
        }
    );
    World.set_function(
        "FindActorsByTag",
        [](const FString& Tag) -> sol::table
        {
            sol::table            Result = FLuaScriptManager::GetState().create_table();
            const TArray<AActor*> Found  = FGameplayStatics::FindActorsByTag(
                GEngine ? GEngine->GetWorld() : nullptr,
                FName(Tag)
            );
            int Idx = 1; // Lua arrays are 1-indexed
            for (AActor* Actor : Found)
            {
                Result[Idx++] = Actor;
            }
            return Result;
        }
    );
    // LuaBlueprint ForEachActorByClass 노드용 — 동일 패턴(table 반환)으로 노출.
    World.set_function(
        "FindActorsByClass",
        [](const FString& ClassName) -> sol::table
        {
            sol::table Result = FLuaScriptManager::GetState().create_table();
            if (!GEngine || !GEngine->GetWorld()) return Result;
            UClass* Cls = UClass::FindByName(ClassName.c_str());
            if (!Cls)
            {
                static TSet<FString> WarnedUnknownClasses;
                if (WarnedUnknownClasses.find(ClassName) == WarnedUnknownClasses.end())
                {
                    WarnedUnknownClasses.insert(ClassName);
                    UE_LOG(
                        "World.FindActorsByClass: 등록되지 않은 액터 클래스 '%s' — 빈 리스트 반환 "
                        "(클래스 이름 오타/미설정 확인)",
                        ClassName.c_str()
                    );
                }
                return Result;
            }
            int Idx = 1;
            for (AActor* Actor : GEngine->GetWorld()->GetActors())
            {
                if (IsValid(Actor) && Actor->GetClass() && Actor->GetClass()->IsA(Cls))
                {
                    Result[Idx++] = Actor;
                }
            }
            return Result;
        }
    );
    World.set_function(
        "GetGameTime",
        []() -> float
        {
            UWorld* CurrentWorld = GEngine ? GEngine->GetWorld() : nullptr;
            return CurrentWorld ? CurrentWorld->GetGameTimeSeconds() : 0.0f;
        }
    );
    World.set_function(
        "LineTrace",
        [](const FVector& Start, const FVector& End, sol::optional<AActor*> IgnoreActor) -> sol::table
        {
            sol::table Result   = FLuaScriptManager::GetState().create_table();
            Result["Hit"]       = false;
            Result["Actor"]     = static_cast<AActor*>(nullptr);
            Result["Component"] = static_cast<UPrimitiveComponent*>(nullptr);
            Result["Location"]  = FVector(0, 0, 0);
            Result["Normal"]    = FVector(0, 0, 0);
            Result["Distance"]  = 0.0f;

            UWorld* CurrentWorld = GEngine ? GEngine->GetWorld() : nullptr;
            if (!CurrentWorld)
            {
                return Result;
            }

            FVector     Delta       = End - Start;
            const float MaxDistance = Delta.Length();
            if (MaxDistance <= 0.0001f)
            {
                return Result;
            }
            const FVector Direction = Delta / MaxDistance;
            FHitResult    Hit;
            if (CurrentWorld->PhysicsRaycast(Start, Direction, MaxDistance, Hit, ECollisionChannel::WorldStatic, IgnoreActor.value_or(nullptr)))
            {
                Result["Hit"]       = true;
                Result["Actor"]     = Hit.HitActor;
                Result["Component"] = Hit.HitComponent;
                Result["Location"]  = Hit.WorldHitLocation;
                Result["Normal"]    = Hit.WorldNormal;
                Result["Distance"]  = Hit.Distance;
            }
            return Result;
        }
    );

    // 게임 특화 usertype/enum/global(GetGameState 등) 은 Game 모듈의
    // RegisterGameLuaBindings 가 등록한다. 호출 순서는 GameEngine/EditorEngine::Init
    // 에서 UEngine::Init() 직후.
}

void FLuaScriptManager::RegisterUIBindings(sol::state& Lua)
{
    Lua.new_usertype<UUserWidget>(
        "UserWidget",
        sol::base_classes,
        sol::bases<UObject>(),
        "AddToViewport",
        [](UUserWidget& Widget)
        {
            Widget.AddToViewport();
        },
        "AddToViewportZ",
        [](UUserWidget& Widget, int32 ZOrder)
        {
            Widget.AddToViewport(ZOrder);
        },
        "RemoveFromParent",
        &UUserWidget::RemoveFromParent,
        "Reload",
        &UUserWidget::ReloadDocument,
        "reload",
        &UUserWidget::ReloadDocument,
        "ReloadDocument",
        &UUserWidget::ReloadDocument,
        "ReloadStyleSheet",
        &UUserWidget::ReloadStyleSheet,
        "reload_style",
        &UUserWidget::ReloadStyleSheet,
        "Show",
        [](UUserWidget& Widget)
        {
            Widget.AddToViewport();
        },
        "Hide",
        &UUserWidget::RemoveFromParent,
        "show",
        [](UUserWidget& Widget)
        {
            Widget.AddToViewport();
        },
        "hide",
        &UUserWidget::RemoveFromParent,
        "IsInViewport",
        &UUserWidget::IsInViewport,
        "bind_click",
        [](UUserWidget& Widget, const FString& ElementId, sol::protected_function Callback)
        {
            Widget.BindClick(ElementId, Callback);
        },
        "SetText",
        &UUserWidget::SetText,
        "set_text",
        &UUserWidget::SetText,
        "SetProperty",
        &UUserWidget::SetProperty,
        "set_property",
        &UUserWidget::SetProperty,
        "SetWantsMouse",
        &UUserWidget::SetWantsMouse,
        "WantsMouse",
        &UUserWidget::WantsMouse
    );

    sol::table UI = Lua.create_named_table("UI");
    UI.set_function(
        "CreateWidget",
        [](const FString& DocumentPath)
        {
            return UUIManager::Get().CreateWidget(nullptr, DocumentPath);
        }
    );
    UI.set_function(
        "ReloadAllStyleSheets",
        []()
        {
            UUIManager::Get().ReloadAllStyleSheets();
        }
    );
}
