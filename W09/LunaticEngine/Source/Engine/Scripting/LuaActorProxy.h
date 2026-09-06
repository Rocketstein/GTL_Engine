#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Scripting/LuaComponentProxy.h"

#include <memory>

class AActor;
struct FLuaGroundHit;

// =================================================
// Lua에 공개할 기능을 제한하는 FLuaActorProxy
// - Lua Script 작성자가 엔진 내부 규칙을 몰라도 사용 가능하게 한다.
// - Lua에서 Actor 포인터에 바로 접근하지 않고, Proxy를 통해서만 거치게 한다.
// =================================================
struct FLuaActorProxy
{
	// Proxy는 AActor를 소유하지 않는다.
	// Lua에 넘어간 값이 GC되거나 복사되어도 실제 Actor 생명주기는 World/Object system이 관리한다.
	struct FLuaActorTaskState;

	FLuaActorProxy();

	AActor* Actor = nullptr;
	std::shared_ptr<FLuaActorTaskState> TaskState;
	FVector Velocity = FVector(1.0f, 0.0f, 0.0f);

	// Lua가 받은 Proxy가 아직 살아 있는 Actor를 가리키는지 확인한다.
	bool IsValid() const;

	// 모든 Proxy 함수는 실제 작업 전에 이 함수를 거쳐 죽은 Actor 접근을 차단한다.
	AActor* GetActor() const;

	// Actor 이름/ID 조회도 생존 체크 후 안전하게 실패한다.
	FString GetName() const;
	uint32 GetUUID() const;

	FString GetTag() const;
	void SetTag(const FString& InTag);
	bool HasTag(const FString& InTag) const;

	// ActorProxy는 현재 World transform만 명시 지원한다. Local transform은 ComponentProxy에서 사용한다.
	// Transform API는 Lua가 Actor 내부 포인터를 직접 만지지 않고 Proxy를 통해서만 위치를 조작하게 만든다.
	FVector GetWorldLocation() const;
	void SetWorldLocation(const FVector& InLocation);
	void SetWorldLocationXYZ(float X, float Y, float Z);

	FRotator GetWorldRotation() const;
	void SetWorldRotation(const FRotator& InRotation);
	void SetWorldRotationXYZ(float Pitch, float Yaw, float Roll);

	FVector GetWorldScale() const;
	void SetWorldScale(const FVector& InScale);
	void SetWorldScaleXYZ(float X, float Y, float Z);

	FVector GetVelocity() const;
	void SetVelocity(const FVector& InVelocity);

	void AddWorldOffset(const FVector& Delta);
	void AddWorldOffsetXYZ(float X, float Y, float Z);

	FVector GetForwardVector() const;
	FVector GetRightVector() const;
	FVector GetUpVector() const;

	FLuaGroundHit FindGround(float MaxDistance, float SkinWidth) const;

	// 이동 API는 즉시 위치를 바꾸는 함수가 아니라 C++ Tick에서 처리할 목표 상태를 설정한다.
	void MoveTo(const FVector& Target);
	void MoveTo2D(float X, float Y);
	void MoveTo3D(float X, float Y, float Z);

	void MoveBy(const FVector& Delta);
	void MoveBy2D(float X, float Y);
	void MoveBy3D(float X, float Y, float Z);

	void MoveToActor(const FLuaActorProxy& TargetActor);

	void StopMove();
	bool IsMoveDone() const;
	void SetMoveSpeed(float InSpeed);
	float GetMoveSpeed() const;

	// Actor가 가진 Component를 Lua에 직접 포인터로 넘기지 않고, 항상 ComponentProxy로 감싸서 반환한다.
	FLuaComponentProxy GetComponent(const FString& ComponentName);
	FLuaComponentProxy GetComponentByType(const FString& TypeName);
	FLuaComponentProxy FindComponentByClass(const FString& TypeName) { return GetComponentByType(TypeName); }
	FLuaComponentProxy GetScriptComponent();
	FLuaComponentProxy GetStaticMeshComponent();

	// Lua coroutine scheduler가 매 프레임 호출해서 Proxy 내부 비동기 작업을 진행한다.
	void TickLuaTasks(float DeltaTime);
	void TickMovement(float DeltaTime);

	void PrintLocation() const;
	void Destroy();

	// 장애물별 피해량을 Lua에서 읽기 위한 얇은 연결점입니다.
	// 실제 Damage 값은 AObstacleActorBase가 들고 있고, 장애물이 아니면 안전한 기본값을 돌려줍니다.
	int GetDamage() const;
	bool SetDamage(int InDamage);
};

struct FLuaGroundHit
{
	bool bHit = false;
	FVector Location = FVector(0.0f, 0.0f, 0.0f);
	FVector Normal = FVector(0.0f, 0.0f, 1.0f);
	float GroundZ = 0.0f;
	float Distance = 0.0f;
	FLuaActorProxy Actor;
	FLuaComponentProxy Component;
};
