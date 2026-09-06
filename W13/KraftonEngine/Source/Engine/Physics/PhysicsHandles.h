#pragma once

// =====================================================================================
// 물리 백엔드 불투명 핸들.
//
// IPhysicsScene 는 PhysX 두 백엔드를 갖는 어댑터다. 랙돌 바디/조인트를
// 가리키되 PhysX 타입(PxRigidDynamic* / PxD6Joint*)을 인터페이스 경계 밖으로
// 노출하지 않기 위한 seam. (UE 의 FPhysicsActorHandle 패턴과 동일한 의도.)
//   - PhysX 백엔드 : Internal = PxRigidDynamic* / PxD6Joint*
// raw void* 대신 빈 래퍼 struct 로 두어 Body/Constraint 핸들의 혼용을 컴파일
// 단계에서 차단한다.
//
// [B 제안] 랙돌(B) 과 PhysXScene(A) 공용 타입. A 가 IPhysicsScene 확장 시 채택.
// =====================================================================================
struct FPhysicsBodyHandle
{
	void* Internal = nullptr;
	bool IsValid() const { return Internal != nullptr; }
};

using FPhysicsActorHandle = FPhysicsBodyHandle;

struct FPhysicsConstraintHandle
{
	void* Internal = nullptr;
	bool IsValid() const { return Internal != nullptr; }
};

// 여러 raw actor 를 한 그룹으로 묶는 핸들(랙돌 = 한 SkeletalMesh 의 본 바디 전체).
//   - PhysX 백엔드 : Internal = PxAggregate*
// selfCollision=false 로 만들면 그룹 내부 바디끼리는 broad-phase 단계에서 충돌 제외돼
// 인접/비인접 본 바디가 서로 부딪쳐 폭발하는 문제를 구조적으로 막는다.
struct FPhysicsAggregateHandle
{
	void* Internal = nullptr;
	bool IsValid() const { return Internal != nullptr; }
};
