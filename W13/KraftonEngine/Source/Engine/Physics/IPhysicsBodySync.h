#pragma once

// ============================================================
// IPhysicsBodySync — 씬 주도 바디 동기화 콜백
//
// 랙돌처럼 raw actor 를 직접 보유하는 객체(예: USkeletalMeshComponent)가 구현해
// 씬에 등록(RegisterBodySync)한다. 씬은 시뮬레이션 phase 경계에서 등록된 핸들러를
// 일괄 호출한다 — 컴포넌트 경로(BodyMappings)와 동일하게 sync 를 씬이 한곳에서 구동.
//
// 호출 시점(불변식):
//   - PrePhysicsSimulate  : simulate() 직전. 엔진→PhysX 쓰기(키네마틱 타깃 등)만.
//   - PostPhysicsSimulate : fetchResults() 직후. PhysX→엔진 읽기(시뮬 포즈 반영)만.
// 이 두 시점 사이(simulate~fetchResults)에는 어떤 PxActor 도 건드리지 않는다 →
// 향후 async simulate(멀티스레드)에서 game-thread 접근이 시뮬 윈도우와 겹치지 않게 한다.
// ============================================================
class IPhysicsBodySync
{
public:
	virtual ~IPhysicsBodySync() = default;

	virtual void PrePhysicsSimulate(float DeltaTime) = 0;
	virtual void PostPhysicsSimulate(float DeltaTime) = 0;
};
