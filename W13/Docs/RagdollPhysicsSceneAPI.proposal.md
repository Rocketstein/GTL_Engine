# IPhysicsScene 랙돌 확장 — 발주안 (B → A)

> 일감 3(랙돌) / 3-1(랙돌 ↔ 애니 융화) 구현을 위해 B 가 A 에게 요청하는
> `IPhysicsScene` 확장 제안서. 데이터 계층/Scene 의 오너는 A 이므로 실제 적용은 A 가 한다.
> 핸들 타입은 `Engine/Physics/PhysicsHandles.h` 에 B 가 미리 작성해 두었다.

## 왜 컴포넌트 경로가 아니라 핸들 API 인가

현재 `IPhysicsScene` 는 **컴포넌트 단위**다. PhysX 백엔드는 *한 AActor 의 모든
PrimitiveComponent 를 하나의 `PxRigidActor` 로 compound* 한다(IPhysicsScene.h 주석 명시).
하지만 랙돌은 **한 스켈레탈 메시 안에 N 개의 독립 `PxRigidDynamic` + 그 사이 D6 조인트**가
필요하다. 즉:

1. **바디 granularity 충돌** — actor 당 1 compound actor 의미와 정면충돌.
2. **조인트 개념 부재** — 기존 컴포넌트 API 에 D6Joint 를 표현할 자리가 없음.
3. **팀 오너십** — "컴포넌트 넘기면 Scene 이 알아서" 로 가면 랙돌 조립 로직이
   `PhysXPhysicsScene`(A 파일) 안으로 들어가 B 의 핵심 기능이 A 파일에 살게 된다.

→ 따라서 **A 는 스켈레톤을 모르는 generic 바디/조인트 프리미티브만 제공**하고,
**조립(본 순회 → 바디 생성 → 조인트 연결 → 포즈 동기화)은 B 가 `USkeletalMeshComponent`
안에서** 수행한다. 이 generic 프리미티브는 차량 등 다른 articulation 에도 재사용 가능하다.

## 추가 요청 메서드 (`IPhysicsScene` 에 순수 가상으로 추가)

```cpp
#include "Physics/PhysicsHandles.h"
#include "Physics/Asset/PhysicsAssetTypes.h"   // FKAggregateGeom
#include "Physics/Asset/ConstraintSetup.h"     // FConstraintSetup

// --- 랙돌용 raw body (컴포넌트 등록 경로 우회) ---
// Geom 은 바디 로컬 공간 프리미티브, WorldPose 는 초기 월드 포즈.
// bKinematic=true 로 만들면 애니가 끌고 가는 상태(= 융화 시작점).
virtual FPhysicsBodyHandle CreateBody(
    const FKAggregateGeom& Geom, const FTransform& WorldPose, float Mass, bool bKinematic) = 0;
virtual void ReleaseBody(FPhysicsBodyHandle Body) = 0;

// 애니 → 물리 : 키네마틱 타깃 지정(서브스텝 보간으로 충돌 안정).
virtual void       SetBodyKinematicTarget(FPhysicsBodyHandle Body, const FTransform& WorldPose) = 0;
// 텔레포트(즉시 이동, 속도 0). 랙돌 초기화/리셋용.
virtual void       SetBodyPose(FPhysicsBodyHandle Body, const FTransform& WorldPose) = 0;
// 물리 → 애니 : 다이내믹 시뮬 결과 읽기.
virtual FTransform GetBodyPose(FPhysicsBodyHandle Body) const = 0;
// 3-1 모드 전환 : 키네마틱 ↔ 다이내믹.
virtual void       SetBodyKinematic(FPhysicsBodyHandle Body, bool bKinematic) = 0;

// --- D6 조인트 ---
// Parent/Child 바디를 FConstraintSetup(Twist/Swing 제한 + 드라이브)대로 연결.
virtual FPhysicsConstraintHandle CreateConstraint(
    FPhysicsBodyHandle Parent, FPhysicsBodyHandle Child, const FConstraintSetup& Setup) = 0;
virtual void ReleaseConstraint(FPhysicsConstraintHandle C) = 0;
// 3-1 블렌딩 : 조인트 드라이브의 목표 포즈(child local)를 매 프레임 갱신.
virtual void SetConstraintDrivePose(FPhysicsConstraintHandle C, const FTransform& ChildLocalTarget) = 0;
```

## 백엔드별 구현 책임 (A)

| 백엔드 | 동작 |
|---|---|
| **PhysX** | `handle.Internal` ↔ `PxRigidDynamic*` / `PxD6Joint*` 캐스팅. `PxD6JointCreate` + `setMotion(eTWIST/eSWING1/eSWING2, eLIMITED)` + `setTwistLimit` / `setSwingLimit`. 드라이브는 `PxD6JointDrive`. |
| **Native** | 랙돌 미지원. `CreateBody`/`CreateConstraint` 는 빈 핸들 반환, 나머지는 no-op. (D6 조인트는 PhysX 전용이라 추상화에 그대로 반영.) |

## 조율 체크리스트

- [ ] `FKAggregateGeom` / `FConstraintSetup` / `UBodySetup` 시그니처 확정 (B 제안안 검토 → A 확정)
- [ ] 위 메서드 시그니처 확정 (특히 `SetBodyKinematicTarget` 서브스텝 보간 여부)
- [ ] `FBodyInstance` 시그니처 확정 — B 의 `USkeletalMeshComponent::Bodies` 가 이 위에 올라감
- [ ] `USkeletalMesh::PhysicsAsset` 필드 추가 (경로 직렬화, 기존 `FSoftObjectPtr` 패턴)
- [ ] PhysX 단위계 ↔ 엔진 단위계(스케일) 합의 — 캡슐 radius/halfHeight 환산
