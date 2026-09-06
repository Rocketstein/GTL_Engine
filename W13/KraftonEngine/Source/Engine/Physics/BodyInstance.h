#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Physics/BodyInstanceCore.h"
#include "Physics/PhysicsHandles.h"

class IPhysicsScene;
class UBodySetup;
class UPrimitiveComponent;

struct FBodyInstance : public FBodyInstanceCore
{
	FPhysicsActorHandle ActorHandle;
	int32 InstanceBoneIndex = -1;
	FVector Scale3D = FVector(1.0f, 1.0f, 1.0f);

	// 이 바디를 소유한 프리미티브 컴포넌트(컴포넌트 경로). 씬이 매 프레임 트랜스폼을
	// 이 컴포넌트로 write-back 하고, 필터/owner-UUID 도 여기서 끌어온다. 랙돌 본 바디는
	// nullptr 로 두며, 동기화는 USkeletalMeshComponent 의 IPhysicsBodySync 가 담당한다.
	UPrimitiveComponent* OwnerComponent = nullptr;

	float MassInKgOverride = 0.0f;

	// 부분 래그돌용 per-body 블렌드 가중치(0=anim 키네마틱, 1=물리 시뮬). 컴포넌트가
	// 매 프레임 Target 으로 보간하고, 이 값으로 본 포즈를 anim↔시뮬 블렌드한다.
	float PhysicsBlendWeight = 0.0f;
	float PhysicsBlendWeightTarget = 0.0f;

	// bStatic=true 면 PxRigidStatic 으로 생성한다(월드 정적 지오메트리). false 면 dynamic 으로
	// 만들고 ShouldInstanceSimulatingPhysics() 에 따라 kinematic 여부를 결정한다(랙돌·시뮬 바디).
	bool InitBody(UBodySetup* Setup, const FTransform& Transform, IPhysicsScene* InRBScene, int32 BoneIndex = -1,
		FPhysicsAggregateHandle Aggregate = {}, bool bStatic = false);
	void TermBody(IPhysicsScene* InRBScene);

	bool IsValidBodyInstance() const { return ActorHandle.IsValid(); }

	FPhysicsActorHandle& GetPhysicsActorHandle() { return ActorHandle; }
	const FPhysicsActorHandle& GetPhysicsActorHandle() const { return ActorHandle; }
	UBodySetup* GetBodySetup() const;

	FTransform GetUnrealWorldTransform(IPhysicsScene* InRBScene) const;
	void SetBodyTransform(IPhysicsScene* InRBScene, const FTransform& NewTransform);
	void SetInstanceSimulatePhysics(IPhysicsScene* InRBScene, bool bSimulate);
	void SetKinematicTarget(IPhysicsScene* InRBScene, const FTransform& NewTarget);
	void UpdateMassProperties(IPhysicsScene* InRBScene);
};
