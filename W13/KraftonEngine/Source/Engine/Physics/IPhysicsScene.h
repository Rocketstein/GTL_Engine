#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Transform.h"
#include "Core/Types/RayTypes.h"
#include "Core/Types/CollisionTypes.h"
#include "Physics/PhysicsHandles.h"
#include "Physics/PhysicsInterfaceTypes.h"

class UWorld;
class AActor;
class UPrimitiveComponent;
class IPhysicsBodySync;
struct FHitResult;
struct FBodyInstance;

// ============================================================
// IPhysicsScene — 물리 시스템 어댑터 인터페이스
//
// World가 소유하며, PrimitiveComponent가 등록/해제.
// 엔진 코드는 이 경계를 통해 PhysX 구현 세부사항(PxScene/PxActor/PxShape)에 직접 의존하지 않는다.
// ============================================================
class IPhysicsScene
{
public:
	virtual ~IPhysicsScene() = default;

	// --- Lifecycle ---
	virtual void Initialize(UWorld* InWorld) = 0;
	virtual void Shutdown() = 0;
	virtual bool IsInitialized() const = 0;

	// --- Per-component body 레지스트리 ---
	// UPrimitiveComponent 가 소유한 FBodyInstance 를 씬의 동기화 대상으로 등록/해제한다.
	// Start/FinishSimulation 이 등록된 바디를 순회하며 OwnerComponent 와 트랜스폼을 동기화한다.
	// (랙돌 본 바디는 여기 등록하지 않는다 — IPhysicsBodySync 로 별도 동기화.)
	virtual void AddBody(FBodyInstance* Body) = 0;
	virtual void RemoveBody(FBodyInstance* Body) = 0;

	// --- Raw physics actor path (PhysicsAsset / ragdoll) ---
	virtual FPhysicsActorHandle CreateActor(const FActorCreationParams& Params) = 0;
	virtual void ReleaseActor(FPhysicsActorHandle Actor) = 0;
	virtual bool IsActorValid(FPhysicsActorHandle Actor) const = 0;
	virtual bool AddGeometry(FPhysicsActorHandle Actor, const FGeometryAddParams& Params) = 0;
	virtual void SetActorGlobalPose(FPhysicsActorHandle Actor, const FTransform& WorldPose) = 0;
	virtual FTransform GetActorGlobalPose(FPhysicsActorHandle Actor) const = 0;
	virtual void SetActorKinematic(FPhysicsActorHandle Actor, bool bKinematic) = 0;
	virtual void SetActorKinematicTarget(FPhysicsActorHandle Actor, const FTransform& WorldPose) = 0;
	virtual void SetActorMass(FPhysicsActorHandle Actor, float Mass) = 0;
	// 액터의 모든 shape 에 self-collision 그룹 ID(filter word3)를 설정한다. 같은 ID 끼리는
	// KraftonFilterShader 가 충돌을 무시 → 랙돌 바디들이 서로(및 자기 캡슐과) 안 부딪치게.
	virtual void SetActorSelfCollisionGroup(FPhysicsActorHandle Actor, uint32 GroupId) = 0;

	// --- Aggregate (랙돌처럼 여러 raw actor 를 한 그룹으로 묶기) ---
	// MaxActors: 그룹에 들어갈 actor 수 상한. bSelfCollision=false 면 그룹 내부 바디끼리
	// 충돌하지 않는다(랙돌 self-collision 을 broad-phase 에서 구조적으로 차단).
	// CreateAggregate 직후 빈 상태로 씬에 추가되며, Aggregate 를 지정한 CreateActor 가
	// actor 를 이 그룹에 넣으면 자동으로 씬에도 반영된다.
	virtual FPhysicsAggregateHandle CreateAggregate(uint32 MaxActors, bool bSelfCollision) = 0;
	virtual void ReleaseAggregate(FPhysicsAggregateHandle Aggregate) = 0;

	// --- Raw physics constraint path (PhysicsAsset / ragdoll) ---
	virtual FPhysicsConstraintHandle CreateConstraint(const FConstraintCreationParams& Params) = 0;
	virtual void ReleaseConstraint(FPhysicsConstraintHandle Constraint) = 0;

	// --- 시뮬레이션 ---
	// Tick = StartSimulation + FinishSimulation. 게임 루프는 둘을 분리 호출해 tick group
	// (anim 등) 을 시뮬레이션 전후로 끼워넣는다(World::Tick). 에디터 프리뷰는 Tick 그대로 사용.
	//   StartSimulation  : dt 클램프 → pre-sync(엔진→PhysX) → simulate()
	//   FinishSimulation : fetchResults() → post-sync(PhysX→엔진) → 지연 이벤트 dispatch
	// 두 호출 사이(simulate 진행 중)에는 PxActor 를 건드리면 안 된다(향후 async 대비).
	virtual void StartSimulation(float DeltaTime) = 0;
	virtual void FinishSimulation() = 0;
	virtual void Tick(float DeltaTime) = 0;

	// 씬 주도 바디 동기화 핸들러 등록. 랙돌(USkeletalMeshComponent)처럼 raw actor 를 직접
	// 보유한 객체가 등록하면 Start/FinishSimulation 에서 Pre/PostPhysicsSimulate 가 호출된다.
	virtual void RegisterBodySync(IPhysicsBodySync* Sync) = 0;
	virtual void UnregisterBodySync(IPhysicsBodySync* Sync) = 0;

	// --- 힘/토크/속도/질량 (handle 경로) ---
	// 컴포넌트 바디와 랙돌 본(FBodyInstance)이 공유하는 단일 force 경로. handle 이 가리키는
	// PxRigidDynamic 에 직접 적용한다(static/kinematic 이면 no-op). SetActorMass 는 위쪽 raw 섹션 참조.
	// CenterOfMass 는 액터 local 좌표계 기준 offset.
	virtual void AddForce(FPhysicsActorHandle Actor, const FVector& Force) = 0;
	virtual void AddForceAtLocation(FPhysicsActorHandle Actor, const FVector& Force, const FVector& WorldLocation) = 0;
	virtual void AddTorque(FPhysicsActorHandle Actor, const FVector& Torque) = 0;
	virtual FVector GetLinearVelocity(FPhysicsActorHandle Actor) const = 0;
	virtual void SetLinearVelocity(FPhysicsActorHandle Actor, const FVector& Vel) = 0;
	virtual FVector GetAngularVelocity(FPhysicsActorHandle Actor) const = 0;
	virtual void SetAngularVelocity(FPhysicsActorHandle Actor, const FVector& Vel) = 0;
	virtual float GetMass(FPhysicsActorHandle Actor) const = 0;
	virtual void SetCenterOfMass(FPhysicsActorHandle Actor, const FVector& LocalOffset) = 0;
	virtual FVector GetCenterOfMass(FPhysicsActorHandle Actor) const = 0;

	// --- Raycast ---
	// TraceChannel: shape의 응답이 이 채널에 대해 Block일 때만 hit으로 인정 (UE 패턴).
	//   예: WorldStatic 채널로 trace → 응답이 WorldStatic Block인 shape만 hit.
	//   trigger flag가 set된 shape는 PhysX 측에서 자동 제외됨.
	// IgnoreActor: 자기 자신/소유 액터를 제외할 때 사용.
	virtual bool Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic,
		const AActor* IgnoreActor = nullptr) const = 0;

	// ObjectType 기반 Raycast — UE의 LineTraceSingleByObjectType 대응.
	//   ObjectTypeMask: bit i = ECollisionChannel(i)의 shape를 hit 후보로 둘지.
	//                   ObjectTypeBit(ECollisionChannel::WorldStatic) 처럼 헬퍼로 조합.
	// 채널 Raycast 는 "응답이 Block 인 모든 shape" 를 잡지만, 응답은 동적 객체/폰도 기본
	// Block 이라 의도와 어긋나기 쉽다. 본 함수는 shape의 ObjectType 자체를 마스크로 필터.
	//   예: 바닥 detection 은 ObjectTypeBit(WorldStatic) 만 → 다이내믹/폰을 바닥으로 잘못 잡지 않음.
	// Trigger flag shape 는 PhysX query 단계에서 자동 제외.
	virtual bool RaycastByObjectTypes(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		uint32 ObjectTypeMask, const AActor* IgnoreActor = nullptr) const = 0;

	// --- Sweep ---
	// 캡슐을 Start 에서 Dir 방향으로 MaxDist 만큼 쓸어(sweep) 최초 Block hit 을 반환.
	// 벽/장애물 충돌 해소(sweep-and-slide)용 — point raycast 와 달리 캡슐 반경을 고려한다.
	//   Radius/HalfHeight : UCapsuleComponent 와 동일 의미 (HalfHeight 는 반구 포함 전체 반높이).
	//   Rot               : 캡슐의 월드 회전(보통 컴포넌트 월드 회전). 캡슐 장축은 Rot 의 +Z 로 정렬.
	//   ObjectTypeMask    : RaycastByObjectTypes 와 동일 — hit 후보 shape 의 ObjectType 비트마스크 필터.
	//                       예: ObjectTypeBit(WorldStatic) | ObjectTypeBit(WorldDynamic).
	//   IgnoreActor       : 자기 자신/소유 액터 제외.
	// 시작 위치에서 이미 겹쳐 있으면(초기 침투) OutHit.Distance=0, OutHit.PenetrationDepth 에 침투
	// 깊이, ImpactNormal 에 depenetration 방향이 채워진다(eMTD).
	virtual bool SweepCapsuleByObjectTypes(const FVector& Start, const FQuat& Rot,
		float Radius, float HalfHeight, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		uint32 ObjectTypeMask, const AActor* IgnoreActor = nullptr) const = 0;
};
