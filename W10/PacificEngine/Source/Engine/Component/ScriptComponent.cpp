#include "ScriptComponent.h"

#include "Component/SceneComponent.h"
#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Runtime/Engine.h"
#include "Serialization/Archive.h"
#include "Scripting/LuaScriptAsset.h"
#include "Collision/Collider2DComponent.h"
#include "Engine/Classes/Camera/CameraManager.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

IMPLEMENT_CLASS(UScriptComponent, UActorComponent)

namespace
{
constexpr const char* LuaPropertyPrefix = "Lua.";

FString MakeLuaPropertyIdentifier(const FString& Name)
{
    return FString(LuaPropertyPrefix) + Name;
}

bool IsLuaPropertyIdentifier(const char* Name)
{
    return Name && std::strncmp(Name, LuaPropertyPrefix, std::strlen(LuaPropertyPrefix)) == 0;
}

FLuaScriptPropertyOverride* FindScriptPropertyOverride(TArray<FLuaScriptPropertyOverride>& Properties, const FString& Name)
{
	auto It = std::find_if(Properties.begin(), Properties.end(),
		[&Name](const FLuaScriptPropertyOverride& Property)
		{
			return Property.Name == Name;
		});
    return It != Properties.end() ? &(*It) : nullptr;
}

bool IsScriptInstanceArgument(const sol::object& Arg)
{
    if (!Arg.is<sol::table>())
    {
        return false;
    }

    sol::table Table = Arg.as<sol::table>();
    sol::object AssetPath = Table["__assetPath"];
    sol::object AssetVersion = Table["__assetVersion"];
    return AssetPath.valid() && AssetPath != sol::nil &&
           AssetVersion.valid() && AssetVersion != sol::nil;
}

bool ReadFirstVec3Argument(sol::variadic_args Args, FVector& OutValue)
{
    for (const sol::object& Arg : Args)
    {
        if (IsScriptInstanceArgument(Arg))
        {
            continue;
        }

        if (ReadLuaVec3(Arg, OutValue))
        {
            return true;
        }
    }
    return false;
}

bool ReadFirstBoolArgument(sol::variadic_args Args, bool& OutValue)
{
    for (const sol::object& Arg : Args)
    {
        if (Arg.get_type() == sol::type::boolean)
        {
            OutValue = Arg.as<bool>();
            return true;
        }
    }
    return false;
}

bool ReadFirstFunctionArgument(sol::variadic_args Args, sol::function& OutValue)
{
    for (const sol::object& Arg : Args)
    {
        if (Arg.get_type() == sol::type::function)
        {
            OutValue = Arg.as<sol::function>();
            return true;
        }
    }
    return false;
}

bool ReadFirstUIntArgument(sol::variadic_args Args, uint32& OutValue)
{
    for (const sol::object& Arg : Args)
    {
        if (Arg.get_type() == sol::type::number)
        {
            OutValue = Arg.as<uint32>();
            return true;
        }
    }
    return false;
}

bool ReadFirstFloatArgument(sol::variadic_args Args, float& OutValue)
{
    for (const sol::object& Arg : Args)
    {
        if (Arg.get_type() == sol::type::number)
        {
            OutValue = Arg.as<float>();
            return true;
        }
    }
    return false;
}

void CollectStringArguments(sol::variadic_args Args, TArray<FString>& OutStrings)
{
    OutStrings.clear();
    for (const sol::object& Arg : Args)
    {
        if (Arg.is<FString>())
        {
            OutStrings.push_back(Arg.as<FString>());
        }
    }
}
} // namespace

void UScriptComponent::BeginPlay()
{
    UActorComponent::BeginPlay();

    if (LoadScript())
    {
        BindFunctions();
        if (AActor* OwnerActor = GetOwner())
        {
            OwnerActor->BindScriptFunctions(*this);
        }
        CallLuaFunction("BeginPlay");
    }

    // 2D 물리 시스템과 연동
    if (AActor* OwnerActor = GetOwner())
    {
        if (UCollider2DComponent* Collider = OwnerActor->FindComponentByClass<UCollider2DComponent>())
        {
            Collider->OnComponentHit2D.AddDynamic(this, &UScriptComponent::OnComponentHit);
            Collider->OnComponentBeginOverlap2D.AddDynamic(this, &UScriptComponent::OnComponentBeginOverlap);
            Collider->OnComponentEndOverlap2D.AddDynamic(this, &UScriptComponent::OnComponentEndOverlap);
        }
    }
}

void UScriptComponent::EndPlay()
{
    if (AActor* OwnerActor = GetOwner())
    {
        if (UCollider2DComponent* Collider = OwnerActor->FindComponentByClass<UCollider2DComponent>())
        {
            Collider->OnComponentHit2D.RemoveDynamic(this);
            Collider->OnComponentBeginOverlap2D.RemoveDynamic(this);
            Collider->OnComponentEndOverlap2D.RemoveDynamic(this);
        }
    }

    CallLuaFunction("EndPlay");
    CoroutineExecutorSet.Clear();
    ScriptInstance = sol::nil;

    UActorComponent::EndPlay();
}

void UScriptComponent::OnComponentHit(UCollider2DComponent* OtherCollider)
{
    CallLuaFunctionWithCollider("OnCollision", OtherCollider);
}

void UScriptComponent::OnComponentBeginOverlap(UCollider2DComponent* OtherCollider)
{
    CallLuaFunctionWithCollider("OnOverlapBegin", OtherCollider);
}

void UScriptComponent::OnComponentEndOverlap(UCollider2DComponent* OtherCollider)
{
    CallLuaFunctionWithCollider("OnOverlapEnd", OtherCollider);
}

void UScriptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
    CallLuaTick(DeltaTime);

    CoroutineExecutorSet.Tick({ DeltaTime });
}

void UScriptComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << ScriptPath;
    Ar << ScriptProperties;
    if (Ar.IsLoading())
    {
        SyncedScriptPath.clear();
        SyncedScriptVersion = 0;
    }
}

void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ .Name = "Script",
                         .Type = EPropertyType::String,
                         .ValuePtr = &ScriptPath });

    SyncScriptPropertiesWithAsset();

    if (!GEngine || ScriptPath.empty())
    {
        return;
    }

    FLuaScriptAsset* Asset = GEngine->GetScriptSystem().GetScriptAsset(ScriptPath);
    if (!Asset || !Asset->IsUsable())
    {
        return;
    }

    const TArray<FLuaScriptPropertyDesc>& Descriptors = Asset->GetPropertyDescriptors();
    for (const FLuaScriptPropertyDesc& Desc : Descriptors)
    {
        FLuaScriptPropertyOverride* Override = FindScriptPropertyOverride(ScriptProperties, Desc.Name);
        if (!Override)
        {
            continue;
        }

        void* ValuePtr = GetLuaScriptValuePtr(Override->Value);
        if (!ValuePtr)
        {
            continue;
        }

        OutProps.push_back({ .Name = MakeLuaPropertyIdentifier(Desc.Name),
                             .Type = GetLuaScriptEditorPropertyType(Desc.Type),
                             .ValuePtr = ValuePtr,
                             .Min = Desc.Min,
                             .Max = Desc.Max,
                             .Speed = Desc.Speed });
    }
}

void UScriptComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Script") == 0)
    {
        SetScriptPath(ScriptPath);
    }
    else if (IsLuaPropertyIdentifier(PropertyName))
    {
        // Values are applied when the Lua instance is created at BeginPlay.
    }
}

void UScriptComponent::SetScriptPath(const FString& InScriptPath)
{
    ScriptPath = InScriptPath;
    SyncedScriptPath.clear();
    SyncedScriptVersion = 0;
    SyncScriptPropertiesWithAsset();
}

bool UScriptComponent::CallScriptFunction(const FString& FunctionName, const sol::variadic_args& Args)
{
    return CallLuaFunction(FunctionName.c_str(), &Args);
}

bool UScriptComponent::CallScriptFunction(const FString& FunctionName)
{
    return CallLuaFunction(FunctionName.c_str());
}

bool UScriptComponent::LoadScript()
{
    ScriptInstance = sol::nil;

    if (ScriptPath.empty() || !GEngine)
    {
        return false;
    }

    SyncScriptPropertiesWithAsset();
    ScriptInstance = GEngine->GetScriptSystem().CreateScriptInstance(ScriptPath, &ScriptProperties);
    if (!ScriptInstance.valid())
    {
        UE_LOG([Lua], Warning, "Failed to create Lua script instance: %s", ScriptPath.c_str());
        return false;
    }

    return true;
}

void UScriptComponent::SyncScriptPropertiesWithAsset()
{
    if (ScriptPath.empty() || !GEngine)
    {
        ScriptProperties.clear();
        SyncedScriptPath.clear();
        SyncedScriptVersion = 0;
        return;
    }

    FLuaScriptAsset* Asset = GEngine->GetScriptSystem().GetScriptAsset(ScriptPath);
    if (!Asset || !Asset->IsUsable())
    {
        if (SyncedScriptPath != ScriptPath)
        {
            ScriptProperties.clear();
            SyncedScriptPath = ScriptPath;
            SyncedScriptVersion = 0;
        }
        return;
    }

    if (SyncedScriptPath == ScriptPath && SyncedScriptVersion == Asset->GetVersion())
    {
        return;
    }

    const TArray<FLuaScriptPropertyDesc>& Descriptors = Asset->GetPropertyDescriptors();
    TArray<FLuaScriptPropertyOverride> NewProperties;
    NewProperties.reserve(Descriptors.size());

    for (const FLuaScriptPropertyDesc& Desc : Descriptors)
    {
        FLuaScriptPropertyOverride NewProperty;
        NewProperty.Name = Desc.Name;

        FLuaScriptPropertyOverride* ExistingProperty = FindScriptPropertyOverride(ScriptProperties, Desc.Name);
        if (ExistingProperty && ExistingProperty->Value.Type == Desc.Type)
        {
            NewProperty.Value = ExistingProperty->Value;
        }
        else
        {
            NewProperty.Value = Desc.DefaultValue;
        }
        NewProperty.Value.Type = Desc.Type;

        NewProperties.push_back(std::move(NewProperty));
    }

    ScriptProperties = std::move(NewProperties);
    SyncedScriptPath = ScriptPath;
    SyncedScriptVersion = Asset->GetVersion();
}

bool UScriptComponent::CallLuaFunction(const char* Name)
{
    return CallLuaFunction(Name, nullptr);
}

bool UScriptComponent::CallLuaFunction(const char* Name, const sol::variadic_args* Args)
{
    if (!Name || Name[0] == '\0' || !ScriptInstance.valid())
    {
        return false;
    }

    sol::object FunctionObject = ScriptInstance[Name];
    if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
    {
        return false;
    }

    sol::protected_function Function = FunctionObject;
    sol::protected_function_result Result = Args ? Function(ScriptInstance, *Args) : Function(ScriptInstance);
    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG([Lua], Error, "Lua script function '%s' failed in '%s': %s",
               Name, ScriptPath.c_str(), Err.what());
        return false;
    }

    return true;
}

bool UScriptComponent::CallLuaFunctionWithCollider(const char* Name, UCollider2DComponent* OtherCollider)
{
    if (!Name || Name[0] == '\0' || !ScriptInstance.valid())
    {
        return false;
    }

    sol::object FunctionObject = ScriptInstance[Name];
    if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
    {
        return false;
    }

    sol::protected_function Function = FunctionObject;
    sol::protected_function_result Result = Function(ScriptInstance, FLuaCollider2DHandle(OtherCollider));
    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG([Lua], Error, "Lua script function '%s' failed in '%s': %s",
               Name, ScriptPath.c_str(), Err.what());
        return false;
    }

    return true;
}

void UScriptComponent::CallFunction(const char* Name)
{
    CallLuaFunction(Name);
}

void UScriptComponent::CallLuaTick(float DeltaTime)
{
    if (!ScriptInstance.valid())
    {
        return;
    }

    sol::object FunctionObject = ScriptInstance["Tick"];
    if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
    {
        return;
    }

    sol::protected_function Function = FunctionObject;
    sol::protected_function_result Result = Function(ScriptInstance, DeltaTime);
    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG([Lua], Error, "Lua script function 'Tick' failed in '%s': %s",
               ScriptPath.c_str(), Err.what());
    }
}

void UScriptComponent::BindFunctions()
{
    BindFunction("GetName",
        [this](sol::variadic_args) -> FString
        {
            return GetFName().ToString();
        });

    BindFunction("GetActor",
        [this](sol::variadic_args) -> FLuaActorHandle
        {
            return FLuaActorHandle(GetOwner());
        });

    BindFunction("GetWorld",
        [this](sol::variadic_args) -> FLuaWorldHandle
        {
            return FLuaWorldHandle(GetOwner() ? GetOwner()->GetWorld() : nullptr);
        });

    BindFunction("GetPostProcess",
        [this](sol::variadic_args) -> FLuaPostProcessHandle
        {
            UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
            APlayerCameraManager* CameraManager = GEngine ? GEngine->GetPlayerCameraManager(World) : nullptr;
            return CameraManager ? FLuaPostProcessHandle(&CameraManager->GetPostProcessController()) : FLuaPostProcessHandle();
        });

    BindFunction("GetComponent",
        [this](const sol::variadic_args& Args) -> FLuaComponentHandle
        {
            return FLuaActorHandle(GetOwner()).GetComponent(Args);
        });

    BindFunction("GetComponents",
        [this](sol::this_state State, const sol::variadic_args& Args) -> sol::table
        {
            return FLuaActorHandle(GetOwner()).GetComponents(State, Args);
        });

	BindFunction("GetRootComponent",
		[this](const sol::variadic_args& Args) -> FLuaComponentHandle
			{
				return FLuaComponentHandle(GetOwner()->GetRootComponent());
			});

    BindFunction("StartCoroutine",
        [this](const sol::variadic_args& Args) -> uint32
        {
            sol::function Func;
            if (!ReadFirstFunctionArgument(Args, Func))
            {
                UE_LOG([Lua], Warning, "StartCoroutine expects a function in '%s'", ScriptPath.c_str());
                return 0;
            }
            return CoroutineExecutorSet.Start(Func);
        });

    BindFunction("StopCoroutine",
        [this](const sol::variadic_args& Args) -> bool
        {
            uint32 FuncKey = 0;
            if (!ReadFirstUIntArgument(Args, FuncKey))
            {
                UE_LOG([Lua], Warning, "StopCoroutine expects a coroutine id in '%s'", ScriptPath.c_str());
                return false;
            }
            return CoroutineExecutorSet.Stop(FuncKey);
        });

    BindFunction("StopAllCoroutines",
        [this]()
        {
            CoroutineExecutorSet.Clear();
        });

    BindFunction("GetComponentByName",
        [this](const sol::variadic_args& Args) -> FLuaComponentHandle
        {
            AActor* OwnerActor = GetOwner();
            if (!OwnerActor)
            {
                return FLuaComponentHandle();
            }

            TArray<FString> Strings;
            CollectStringArguments(Args, Strings);
            const FString ClassName = !Strings.empty() ? Strings[0] : FString();
            const FString ComponentName = Strings.size() > 1 ? Strings[1] : FString();

            for (UActorComponent* Component : OwnerActor->GetComponents())
            {
                if (!Component || ClassName.empty())
                    continue;

                UClass* TargetClass = UClass::FindClassByName(ClassName);
                bool IsValid = TargetClass && Component->GetClass() && Component->GetClass()->IsA(TargetClass);

                if (!IsValid)
                {
                    continue;
                }
                if (!ComponentName.empty() && Component->GetFName().ToString() != ComponentName)
                {
                    continue;
                }
                return FLuaComponentHandle(Component);
            }

            return FLuaComponentHandle();
        });

    BindFunction("QueryActorByTagClosest",
        [this](const sol::variadic_args& Args) -> FLuaActorHandle
        {
            TArray<FString> Strings;
            CollectStringArguments(Args, Strings);
            if (Strings.empty())
            {
                return FLuaActorHandle();
            }

            FVector Position;
            float Radius = 0.0f;
            AActor* OwnerActor = GetOwner();
            UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
            if (!World || !ReadFirstVec3Argument(Args, Position) || !ReadFirstFloatArgument(Args, Radius) || Radius < 0.0f)
            {
                return FLuaActorHandle();
            }

            const FString& Tag = Strings[0];
            const FName TargetTag(Tag);
            const float RadiusSquared = Radius * Radius;
            float BestDistanceSquared = FLT_MAX;
            AActor* BestActor = nullptr;

			//액터 탐색 로직 (태그 기반)
            for (AActor* Actor : World->GetActors())
            {
                if (!Actor || Actor->GetActorTag() != TargetTag || !Actor->IsVisible())
                {
                    continue;
                }

                const float DistanceSquared = FVector::DistSquared(Actor->GetActorLocation(), Position);
                if (DistanceSquared <= RadiusSquared && DistanceSquared < BestDistanceSquared)
                {
                    BestDistanceSquared = DistanceSquared;
                    BestActor = Actor;
                }
            }

            return FLuaActorHandle(BestActor);
        });

	BindFunction("QueryActorsByTagInRadius",
		[this](sol::this_state LuaState, const sol::variadic_args& Args) -> sol::table
			{
				sol::state_view Lua(LuaState);
                sol::table Result = Lua.create_table();

                TArray<FString> Strings;
                CollectStringArguments(Args, Strings);
                if (Strings.empty())
                {
					return Result;
                }

                FVector Position;
                float Radius = 0.0f;

                AActor* OwnerActor = GetOwner();
                UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;

                if (!World ||
					!ReadFirstVec3Argument(Args, Position) ||
					!ReadFirstFloatArgument(Args, Radius) ||
                    Radius < 0.0f)
                    {
						return Result;
                    }

					const FString& Tag = Strings[0];
                    const FName TargetTag(Tag);

                    const float RadiusSquared = Radius * Radius;

                    int32 LuaIndex = 1;

                    for (AActor* Actor : World->GetActors())
                    {
						if (!Actor ||
							Actor->GetActorTag() != TargetTag ||
                            !Actor->IsVisible())
							{
								continue;
							}

                        const float DistanceSquared =
							FVector::DistSquared(Actor->GetActorLocation(), Position);

                        if (DistanceSquared > RadiusSquared)
                        {
							continue;
                        }

                        Result[LuaIndex++] = FLuaActorHandle(Actor);
                    }

                    return Result;
				});

    BindFunction("GetCustomTimeDilation",
        [this](sol::variadic_args) -> float
        {
            return FLuaActorHandle(GetOwner()).GetCustomTimeDilation();
        });

    BindFunction("SetCustomTimeDilation",
        [this](const sol::variadic_args& Args) -> void
        {
            float TimeDilation = 1.0f;
            if (ReadFirstFloatArgument(Args, TimeDilation))
            {
                FLuaActorHandle(GetOwner()).SetCustomTimeDilation(TimeDilation);
            }
        });
}
