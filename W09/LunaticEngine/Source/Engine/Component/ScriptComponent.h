#pragma once

#include "Component/ActorComponent.h"
#include "Scripting/LuaScriptInstance.h"
#include "Core/Delegate.h"

class UShapeComponent;
struct FComponentOverlapEvent;
struct FComponentHitEvent;
namespace json
{
	class JSON;
}

// ======================================================
// -- Actor와 Lua Script를 연결해주는 Script 관리 Component --
// - 런타임 감시 시스템과 개별 Lua 실행 인스턴스 사이의 중간 계층 역할을 맡는다.
// - ScriptPath 직렬화, 에디터 편집, BeginPlay/EndPlay 연동, reload 진입점을 모두 여기서 관리한다.
// ======================================================
class UScriptComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UScriptComponent, UActorComponent)

	~UScriptComponent() override;

	// Actor 생명주기에 맞춰 Lua 스크립트를 로드/정리한다.
	void BeginPlay() override;
	void EndPlay() override;

	// 부모 클래스 override
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	// 스크립트 경로를 내부 canonical 표기로 정규화해서 저장한다.
	void SetScriptPath(const FString& InPath);
	const FString& GetScriptPath() const { return ScriptPath; }
	
	// Template.lua를 기반으로 기본 스크립트 파일을 만든다.
	bool CreateScript();
	// 현재 ScriptPath가 가리키는 Lua 파일을 처음부터 다시 로드한다.
	bool LoadScript();
	// 수동 reload와 hot-reload가 모두 이 진입점을 사용한다.
	bool ReloadScript();
	// 현재 ScriptPath 파일을 운영체제 기본 연결 프로그램으로 연다.
	bool OpenScript();
	bool CallScriptFunction(const FString& FunctionName);

	// 마지막 스크립트 에러 메시지를 에디터 UI에서 표시할 때 사용한다.
	const FString& GetLastScriptError() const { return LastScriptError; }

	void RegisterEnvFunction(const FString& Name, std::function<void(FLuaActorProxy)> Func) { ScriptInstance.RegisterEnvFunction(Name, Func); }

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	// Component 수준 에러 상태를 Lua 인스턴스 상태와 동기화한다.
	void ClearScriptError();
	void SetScriptError(const FString& ErrorMessage);
	void RefreshScriptErrorState();

	// 현재 Actor 상태와 ScriptPath를 기준으로 Runtime hot-reload 등록 여부를 갱신한다.
	void UpdateRuntimeRegistration();

private:
	FString ScriptPath;					// 내부 저장용 canonical 스크립트 경로
	bool bLoaded = false;
	bool bHasScriptError = false;		
	FString LastScriptError;

	FLuaScriptInstance ScriptInstance;	// Actor별 독립 Lua 실행 상태 (sol::environment 사용)

	// =================================================================
	// Delegate Section
	// - Lua에서 Delegate 호출을 위해 있는 멤버 변수
	// =================================================================
	struct FShapeCollisionBinding
	{
		UShapeComponent* ShapeComponent = nullptr;
		FDelegateHandle BeginOverlapHandle;
		FDelegateHandle EndOverlapHandle;
		FDelegateHandle HitHandle;
	};

	TArray<FShapeCollisionBinding> ShapeCollisionBindings;

	void BindOwnerShapeCollisionEvents();
	void UnbindOwnerShapeCollisionEvents();

	void OnShapeBeginOverlap(const FComponentOverlapEvent& Event);
	void OnShapeEndOverlap(const FComponentOverlapEvent& Event);
	void OnShapeHit(const FComponentHitEvent& Event);
};
