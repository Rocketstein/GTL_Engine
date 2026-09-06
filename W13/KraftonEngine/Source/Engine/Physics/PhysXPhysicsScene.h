#pragma once

#include "Physics/IPhysicsScene.h"
#include "Core/Types/CoreTypes.h"
#include <vector>

class AActor;

// Forward declarations — PhysX types
namespace physx
{
	class PxFoundation;
	class PxPhysics;
	class PxScene;
	class PxCooking;
	class PxDefaultCpuDispatcher;
	class PxMaterial;
	class PxRigidActor;
	class PxShape;
	class PxD6Joint;
	class PxAggregate;
}

class FPhysXSimulationCallback;
class FPhysXVehicleManager;
class IPhysicsBodySync;
struct FBodyInstance;

// ============================================================
// FPhysXPhysicsScene — PhysX 4.1 기반 물리 시스템
//
// IPhysicsScene 인터페이스 뒤에서 PhysX 세부 타입을 캡슐화한다.
//
// 등록 단위는 Actor — 한 액터의 여러 PrimitiveComponent는 하나의
// PxRigidActor에 compound shape로 합쳐진다. 각 shape의 LocalPose는
// 액터 RootComponent에 대한 상대 transform. 이로써 차체 Box + 바퀴
// Sphere 4개처럼 다중 콜라이더가 자연스럽게 한 강체로 동작한다.
// ============================================================
class FPhysXPhysicsScene : public IPhysicsScene
{
public:
	static bool CookTriangleMesh(const TArray<FVector>& Vertices, const TArray<int32>& Indices,
		TArray<uint8>& OutCookedData, FString* OutError = nullptr);

	void Initialize(UWorld* InWorld) override;
	void Shutdown() override;
	bool IsInitialized() const override { return Scene != nullptr && Physics != nullptr && DefaultMaterial != nullptr; }

	void AddBody(FBodyInstance* Body) override;
	void RemoveBody(FBodyInstance* Body) override;

	// --- Raw physics actor path ---
	// PhysicsAsset의 BodySetup 하나당 별도 PxRigidActor를 생성/제어하기 위한 저수준 경로.
	// 컴포넌트 바디(UPrimitiveComponent::BodyInstance)와 랙돌 본(FBodyInstance)이 모두 이 경로로
	// actor 를 만든다 — 한 바디가 여기서 반환된 FPhysicsActorHandle 을 보관한다.
	FPhysicsActorHandle CreateActor(const FActorCreationParams& Params) override;
	void ReleaseActor(FPhysicsActorHandle Actor) override;
	bool IsActorValid(FPhysicsActorHandle Actor) const override;
	// 생성된 actor에 UBodySetup::AggGeom 기반 shape를 붙인다.
	// UE의 FPhysicsInterface::CreateActor + AddGeometry 흐름을 따른다.
	bool AddGeometry(FPhysicsActorHandle Actor, const FGeometryAddParams& Params) override;
	// FBodyInstance / SkeletalMeshComponent가 본 포즈와 물리 포즈를 동기화할 때 사용한다.
	void SetActorGlobalPose(FPhysicsActorHandle Actor, const FTransform& WorldPose) override;
	FTransform GetActorGlobalPose(FPhysicsActorHandle Actor) const override;
	void SetActorKinematic(FPhysicsActorHandle Actor, bool bKinematic) override;
	void SetActorKinematicTarget(FPhysicsActorHandle Actor, const FTransform& WorldPose) override;
	void SetActorMass(FPhysicsActorHandle Actor, float Mass) override;
	void SetActorSelfCollisionGroup(FPhysicsActorHandle Actor, uint32 GroupId) override;

	FPhysicsAggregateHandle CreateAggregate(uint32 MaxActors, bool bSelfCollision) override;
	void ReleaseAggregate(FPhysicsAggregateHandle Aggregate) override;

	FPhysicsConstraintHandle CreateConstraint(const FConstraintCreationParams& Params) override;
	void ReleaseConstraint(FPhysicsConstraintHandle Constraint) override;

	void StartSimulation(float DeltaTime) override;
	void FinishSimulation() override;
	void Tick(float DeltaTime) override;

	void RegisterBodySync(IPhysicsBodySync* Sync) override;
	void UnregisterBodySync(IPhysicsBodySync* Sync) override;

	// 힘/토크/속도/질량 — handle 경로 (컴포넌트 바디 + 랙돌 본 공용)
	void AddForce(FPhysicsActorHandle Actor, const FVector& Force) override;
	void AddForceAtLocation(FPhysicsActorHandle Actor, const FVector& Force, const FVector& WorldLocation) override;
	void AddTorque(FPhysicsActorHandle Actor, const FVector& Torque) override;
	FVector GetLinearVelocity(FPhysicsActorHandle Actor) const override;
	void SetLinearVelocity(FPhysicsActorHandle Actor, const FVector& Vel) override;
	FVector GetAngularVelocity(FPhysicsActorHandle Actor) const override;
	void SetAngularVelocity(FPhysicsActorHandle Actor, const FVector& Vel) override;
	float GetMass(FPhysicsActorHandle Actor) const override;
	void SetCenterOfMass(FPhysicsActorHandle Actor, const FVector& LocalOffset) override;
	FVector GetCenterOfMass(FPhysicsActorHandle Actor) const override;

	bool Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic,
		const AActor* IgnoreActor = nullptr) const override;

	bool RaycastByObjectTypes(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		uint32 ObjectTypeMask, const AActor* IgnoreActor = nullptr) const override;

	bool SweepCapsuleByObjectTypes(const FVector& Start, const FQuat& Rot,
		float Radius, float HalfHeight, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		uint32 ObjectTypeMask, const AActor* IgnoreActor = nullptr) const override;

	//=============================================================
	// Vehicles
	//=============================================================
	FPhysXVehicleManager* GetVehicleManager() { return VehicleManager; }

private:
	UWorld* World = nullptr;

	// PhysX core objects
	physx::PxFoundation* Foundation = nullptr;
	physx::PxPhysics* Physics = nullptr;
	physx::PxScene* Scene = nullptr;
	physx::PxCooking* Cooking = nullptr;   // 공유 PxCooking (convex hull cooking — vehicles)
	physx::PxDefaultCpuDispatcher* Dispatcher = nullptr;
	physx::PxMaterial* DefaultMaterial = nullptr;
	FPhysXSimulationCallback* EventCallback = nullptr;
	FPhysXVehicleManager* VehicleManager = nullptr;

	// UPrimitiveComponent 가 소유한 FBodyInstance 들. Start/FinishSimulation 이 순회하며
	// OwnerComponent ↔ PhysX 트랜스폼을 동기화한다. 씬은 소유권이 없다 — 컴포넌트가 TermBody 후
	// RemoveBody 로 빠진다.
	std::vector<FBodyInstance*> ComponentBodies;

	// 씬이 소유권/수명을 추적하는 PhysX 리소스 — CreateActor/CreateAggregate/CreateConstraint 로
	// 만든 것들. 씬이 소유권/가시성을 갖게 해 일관성을 맞춘다(생성/해제 짝, Shutdown 일괄 정리,
	// 디버그 열거, 향후 멀티스레드 전후처리 순회). 타입·해제순서가 달라 한 컨테이너로 합치지 않고
	// 묶음(struct)으로만 정리한다.
	struct FSceneOwnedResources
	{
		std::vector<physx::PxRigidActor*> Actors;       // CreateActor 로 만든 actor (컴포넌트 바디 + 랙돌 본)
		std::vector<physx::PxAggregate*>  Aggregates;   // 래그돌 그룹
		std::vector<physx::PxD6Joint*>    Constraints;  // 래그돌 조인트(D6)
	};
	FSceneOwnedResources Owned;

	// 씬 주도 sync 핸들러(랙돌 등). Start/FinishSimulation 에서 Pre/PostPhysicsSimulate 호출.
	std::vector<IPhysicsBodySync*> BodySyncs;
	// StartSimulation 이 simulate 한 dt 를 FinishSimulation/PostSync 로 전달. 0 = 이번 프레임
	// 시뮬 안 함(FinishSimulation 이 fetchResults 를 건너뛰는 가드).
	float CurrentSimDeltaTime = 0.0f;
};
