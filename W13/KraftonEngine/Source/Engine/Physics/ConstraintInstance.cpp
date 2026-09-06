#include "Physics/ConstraintInstance.h"

#include "Physics/Asset/ConstraintSetup.h"
#include "Physics/BodyInstance.h"
#include "Physics/IPhysicsScene.h"
#include "Physics/PhysicsInterfaceTypes.h"

bool FConstraintInstance::InitConstraint(
	IPhysicsScene* Scene,
	FBodyInstance* InBodyInstance1,
	FBodyInstance* InBodyInstance2,
	const FConstraintSetup* InConstraintSetup,
	const FVector& Scale)
{
	TermConstraint(Scene);

	if (!Scene || !InBodyInstance1 || !InBodyInstance2 || !InConstraintSetup)
	{
		return false;
	}
	if (!InBodyInstance1->IsValidBodyInstance() || !InBodyInstance2->IsValidBodyInstance())
	{
		return false;
	}

	BodyInstance1 = InBodyInstance1;
	BodyInstance2 = InBodyInstance2;
	ConstraintSetup = InConstraintSetup;

	ConstraintBone1 = InConstraintSetup->ChildBone;
	ConstraintBone2 = InConstraintSetup->ParentBone;
	JointName = ConstraintBone1;
	RefFrame1 = InConstraintSetup->ChildFrame;
	RefFrame2 = InConstraintSetup->ParentFrame;

	// ChildFrame(RefFrame1) 앵커 오프셋을 바디 월드 스케일에 맞춘다. 바디 actor 는 스케일된
	// 본 월드 변환에 놓이는데 프레임 translation 은 본-로컬(언스케일)이라 스케일을 곱해야 한다.
	RefFrame1.Location = FVector(RefFrame1.Location.X * Scale.X, RefFrame1.Location.Y * Scale.Y, RefFrame1.Location.Z * Scale.Z);

	// ParentFrame(RefFrame2)은 저장값을 쓰지 않고 바디 bind 월드 + ChildFrame 으로부터 재계산해
	// 두 프레임이 정확히 일치하게 한다. 저장된 ParentFrame 의 회전 합성 규약 오차로 인한 90° 스냅
	// (특히 머리/쇄골처럼 로컬 회전이 큰 본)을 원천 차단하고, 스케일도 월드 변환에서 자연히 일관된다.
	//   jointWorld = RefFrame1 ∘ childBodyWorld,  RefFrame2 = jointWorld ∘ parentBodyWorld^-1
	{
		const FMatrix ChildW  = InBodyInstance1->GetUnrealWorldTransform(Scene).ToMatrix();
		const FMatrix ParentW = InBodyInstance2->GetUnrealWorldTransform(Scene).ToMatrix();
		const FMatrix JointWorld = RefFrame1.ToMatrix() * ChildW;        // row-vector: frame-local → world
		RefFrame2 = FTransform(JointWorld * ParentW.GetInverse());        // → parent local
		RefFrame2.Scale = FVector(1.0f, 1.0f, 1.0f);
	}

	FConstraintCreationParams Params;
	Params.Actor1 = InBodyInstance1->GetPhysicsActorHandle();
	Params.Actor2 = InBodyInstance2->GetPhysicsActorHandle();
	Params.LocalFrame1 = RefFrame1;
	Params.LocalFrame2 = RefFrame2;
	Params.ConstraintSetup = InConstraintSetup;
	Params.UserData = this;

	ConstraintHandle = Scene->CreateConstraint(Params);
	if (!ConstraintHandle.IsValid())
	{
		ResetRuntimeState();
		return false;
	}

	return true;
}

void FConstraintInstance::TermConstraint(IPhysicsScene* Scene)
{
	if (Scene && ConstraintHandle.IsValid())
	{
		Scene->ReleaseConstraint(ConstraintHandle);
	}

	ResetRuntimeState();
}

bool FConstraintInstance::IsValidConstraintInstance() const
{
	return ConstraintHandle.IsValid() && BodyInstance1 && BodyInstance2 && ConstraintSetup;
}

void FConstraintInstance::ResetRuntimeState()
{
	BodyInstance1 = nullptr;
	BodyInstance2 = nullptr;
	ConstraintSetup = nullptr;
	ConstraintHandle = {};
}
