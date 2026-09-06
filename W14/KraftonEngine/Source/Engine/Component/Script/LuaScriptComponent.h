#pragma once

#include "Component/ActorComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Core/Delegate.h"
#include "Math/Vector.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Object/GarbageCollection.h"
#include "Source/Engine/Component/Script/LuaScriptComponent.generated.h"
#include <sol/sol.hpp>

class UPrimitiveComponent;
class UCombatHitEventComponent;
class UHealthComponent;
struct FHitResult;

UCLASS()
class ULuaScriptComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	ULuaScriptComponent();
	~ULuaScriptComponent();

	void InitializeLua();
	void ReleaseLuaRuntimeForShutdown();
	UFUNCTION(Callable, Exec, CallInEditor, Category="Script")
	void ReloadScript();

	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	void BeginDestroy() override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;


	void PreGetEditableProperties() override;
	UFUNCTION(Pure, Category="Script")
	const FString& GetScriptFile() const { return ScriptFile; }
	UFUNCTION(Pure, Category="Script")
	FString GetScriptFileValue() const { return ScriptFile; }
	UFUNCTION(Callable, Category="Script")
	void SetScriptFile(const FString& InScriptFile) { ScriptFile = InScriptFile; }
	void DispatchOverlap(class AActor* OtherActor);

	// Lua script 의 환경(env)에서 인자 없는 전역 함수 하나를 호출. 함수가 없거나
	// nil 이면 조용히 false 반환 — 호출자는 lua 쪽 함수 정의 여부에 신경 쓸 필요 없음.
	UFUNCTION(Callable, Exec, Category="Script")
	bool CallFunction(const FString& FunctionName);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void EnsureDefaultScriptFile();
	void BindOwnerCollisionEvents();
	void BindOwnerCombatEvents();
	void BindOwnerDamageEvents();
	void ClearCollisionBindings();
	void ClearCombatBindings();
	void ClearDamageBindings();
	void ClearLuaRuntime();
	void InvokeLuaEndPlay();
	void HandleDeferredLuaCleanup();
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
	void HandleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& HitResult);
	void HandleEndHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp);
	void HandleAttackHit(
		UCombatHitEventComponent* EventComponent,
		AActor* Attacker,
		AActor* Target,
		UPrimitiveComponent* HitComponent,
		const FCombatDamageSpec& DamageSpec,
		FName HitEventName);
	void HandleAttackParried(
		UCombatHitEventComponent* EventComponent,
		AActor* Attacker,
		AActor* Defender,
		UPrimitiveComponent* HitComponent,
		const FCombatDamageSpec& DamageSpec,
		FName HitEventName);
	void HandleDamageApplied(
		UHealthComponent* HealthComponent,
		const FCombatDamageReport& DamageReport,
		const FCombatDamageSpec& DamageSpec);

	UPROPERTY(Edit, Save, Category="Script", DisplayName="ScriptFile", AssetType="Script")
	FString ScriptFile;
	
	sol::environment Env;
	sol::protected_function LuaBeginPlay;
	sol::protected_function LuaTick;
	sol::protected_function LuaEndPlay;
	sol::protected_function LuaOnOverlap;
	sol::protected_function LuaOnEndOverlap;
	sol::protected_function LuaOnHit;
	sol::protected_function LuaOnEndHit;
	sol::protected_function LuaOnDamaged;
	sol::protected_function LuaOnAttackHit;
	sol::protected_function LuaOnAttackParried;

	bool bEndPlayRouted = false;
	bool bHasCalledLuaEndPlay = false;
	bool bPendingLuaEndPlay = false;
	bool bPendingLuaCleanup = false;
	int32 LuaCallDepth = 0;

	struct FLuaCallScope
	{
		ULuaScriptComponent* Owner = nullptr;

		explicit FLuaCallScope(ULuaScriptComponent* InOwner)
			: Owner(InOwner)
		{
			if (Owner)
			{
				FGarbageCollector::Get().PushCollectionBlock();
				++Owner->LuaCallDepth;
			}
		}

		~FLuaCallScope()
		{
			if (!Owner)
			{
				return;
			}

			--Owner->LuaCallDepth;
			Owner->HandleDeferredLuaCleanup();
			FGarbageCollector::Get().PopCollectionBlock();
		}
	};

	TArray<TWeakObjectPtr<UPrimitiveComponent>> BoundOverlapComponents;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> BoundHitComponents;
	TArray<FDelegateHandle> BeginOverlapHandles;
	TArray<FDelegateHandle> EndOverlapHandles;
	TArray<FDelegateHandle> HitHandles;
	TArray<FDelegateHandle> EndHitHandles;
	TWeakObjectPtr<UCombatHitEventComponent> BoundCombatHitEventComponent;
	FDelegateHandle AttackHitHandle;
	FDelegateHandle AttackParriedHandle;
	TWeakObjectPtr<UHealthComponent> BoundHealthComponent;
	FDelegateHandle DamageAppliedHandle;
};
