#pragma once
#include "ActorComponent.h"
#include "sol.hpp"
#include "Engine/LUA/LuaCoroutine.h"
#include "Engine/Scripting/LuaScriptTypes.h"
#include "Engine/Scripting/LuaEngineBinding.h"

class UScriptComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UScriptComponent, UActorComponent)

    void BeginPlay() override;
    void EndPlay() override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void SetScriptPath(const FString& InScriptPath);
    const FString& GetScriptPath() const { return ScriptPath; }
    void CallFunction(const char* Name);
    bool CallScriptFunction(const FString& FunctionName, const sol::variadic_args& Args);
    bool CallScriptFunction(const FString& FunctionName);

    template <typename TFunc>
    void BindFunction(const char* Name, TFunc&& Function)
    {
        if (!ScriptInstance.valid())
        {
            return;
        }

        ScriptInstance.set_function(Name, std::forward<TFunc>(Function));
    }


protected:
    void OnComponentHit(class UCollider2DComponent* OtherCollider);
    void OnComponentBeginOverlap(class UCollider2DComponent* OtherCollider);
    void OnComponentEndOverlap(class UCollider2DComponent* OtherCollider);

private:
    bool LoadScript();
    void SyncScriptPropertiesWithAsset();
    bool CallLuaFunction(const char* Name);
    bool CallLuaFunction(const char* Name, const sol::variadic_args* Args);
    bool CallLuaFunctionWithCollider(const char* Name, class UCollider2DComponent* OtherCollider);
    void CallLuaTick(float DeltaTime);

    void BindFunctions();

private:
    sol::table ScriptInstance;
    FString ScriptPath;
    TArray<FLuaScriptPropertyOverride> ScriptProperties;
    FString SyncedScriptPath;
    uint64 SyncedScriptVersion = 0;

    FCoroutineExecutorSet CoroutineExecutorSet;
};
