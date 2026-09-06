#pragma once
#include "GameFramework/StaticMeshActor.h"  
#include "Core/CollisionEventTypes.h"  
#include <functional>  

class UShapeComponent;
class UBoxComponent;
class USphereComponent;
class UCapsuleComponent;

enum class EObstacleType : uint8
{
	Barrier,	// must switch lanes
	LowBar,		// must jump
	HighBar,	// must slide
	Pendulum,
	Misc,
};

class AObstacleActorBase : public AStaticMeshActor {
public:
	DECLARE_CLASS(AObstacleActorBase, AStaticMeshActor)
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime) {};
	virtual void EndPlay() override;

	virtual void InitDefaultComponents(const FString& UStaticMeshFileName) override;

	int GetDamage() const { return Damage; }
	void SetDamage(int InDamage) { Damage = InDamage; }

protected:
	virtual ~AObstacleActorBase() = default;

	//---------------------------------------------------------------------------  
	// The "Hitter" identifies the "Victim", but the Victim decides how to bleed.  
	//---------------------------------------------------------------------------  

	// Notify the Player that it is hit by an obstacle.   
	virtual void OnHit(const FComponentHitEvent& Other);

	// Notify the Player that it is overlapping with an obstacle  
	virtual void OnOverlap(const FComponentOverlapEvent& Other);

	// Should be called when a player collides into one of its shape components.  
	// TODO: Add a concrete definition once player class is ready  
	virtual void OnPlayerCollision() = 0;

	virtual void SetCollisionBoxExtent(FVector InExtent) { CollisionBoxExtent = InExtent; }
	virtual void SetCollisionBoxOffset(FVector InOffset) { CollisionBoxOffset = InOffset; }

protected:
	float OnHitDamage = 0.f;
	// Lua PlayerController가 충돌 시 읽는 장애물 피해량입니다.
	// TODO: 장애물 Damage 테이블을 Lua Config로 완전히 옮길 수 있으면 타입별 밸런스를 Lua에서 관리하면 됨.
	int Damage = 1;

	FVector CollisionBoxExtent = FVector(1.f, 1.f, 1.f);
	FVector CollisionBoxOffset = FVector();
};
