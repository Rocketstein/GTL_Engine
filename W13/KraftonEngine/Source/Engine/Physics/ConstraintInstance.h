#pragma once

#include "Math/Transform.h"
#include "Object/FName.h"
#include "Physics/PhysicsHandles.h"

class IPhysicsScene;
struct FBodyInstance;
struct FConstraintSetup;

struct FConstraintInstance
{
	FName JointName;
	FName ConstraintBone1;
	FName ConstraintBone2;

	FTransform RefFrame1;
	FTransform RefFrame2;

	FBodyInstance* BodyInstance1 = nullptr;
	FBodyInstance* BodyInstance2 = nullptr;
	const FConstraintSetup* ConstraintSetup = nullptr;
	FPhysicsConstraintHandle ConstraintHandle;

	// Scale: 컴포넌트 월드 스케일. 조인트 프레임 translation 은 본-로컬(언스케일) 공간이라
	// 바디 actor 가 스케일된 월드 위치에 놓이면 프레임 오프셋도 같은 배율로 키워야 한다.
	// (안 하면 조인트가 바디들을 1/Scale 간격으로 끌어당겨 중앙으로 수축한다.)
	bool InitConstraint(
		IPhysicsScene* Scene,
		FBodyInstance* InBodyInstance1,
		FBodyInstance* InBodyInstance2,
		const FConstraintSetup* InConstraintSetup,
		const FVector& Scale = FVector(1.0f, 1.0f, 1.0f));

	void TermConstraint(IPhysicsScene* Scene);

	bool IsValidConstraintInstance() const;

	FPhysicsConstraintHandle& GetPhysicsConstraintRef() { return ConstraintHandle; }
	const FPhysicsConstraintHandle& GetPhysicsConstraintRef() const { return ConstraintHandle; }

private:
	void ResetRuntimeState();
};
