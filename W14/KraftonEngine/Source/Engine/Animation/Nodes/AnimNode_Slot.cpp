#include "AnimNode_Slot.h"

#include "Animation/AnimInstance.h"
#include "Animation/Montage/AnimMontageInstance.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/PoseContext.h"
#include "Object/GarbageCollection.h"
#include "Object/Object.h"

void FAnimNode_Slot::Initialize(const FAnimationInitializeContext& Context)
{
	OwnerAnimInstance = Context.AnimInstance;
	if (InputPose) InputPose->Initialize(Context);
}

void FAnimNode_Slot::OnBecomeRelevant(const FAnimationInitializeContext& Context)
{
	if (InputPose) InputPose->OnBecomeRelevant(Context);
}

void FAnimNode_Slot::Update(const FAnimationUpdateContext& Context)
{
	if (InputPose)
	{
		InputPose->Update(Context);
		InputLastRM = InputPose->GetLastRootMotionDelta();
	}
	else
	{
		InputLastRM = FTransform();
	}

	// Slot runtime이 outgoing pose를 갱신하고 active montage의 root motion만 반영한다.
	if (IsValid(OwnerAnimInstance))
	{
		OwnerAnimInstance->UpdateMontageSlot(SlotName, Context.DeltaSeconds, InputLastRM);
	}
}

float FAnimNode_Slot::GetEffectiveBlendWeight() const
{
	if (!IsValid(OwnerAnimInstance)) return 0.0f;
	return OwnerAnimInstance->GetMontageSlotBlendWeight(SlotName);
}

void FAnimNode_Slot::Evaluate(FPoseContext& Output)
{
	// 1) InputPose 평가 — base pose 가 Output 에 들어감. 없으면 ref pose fallback.
	if (InputPose)
	{
		InputPose->Evaluate(Output);
	}
	else
	{
		Output.ResetToRefPose();
	}

	// 2) 이 slot의 active/outgoing montage pose를 합성한다.
	if (!IsValid(OwnerAnimInstance)) return;
	OwnerAnimInstance->EvaluateMontageSlot(SlotName, Output);
}


void FAnimNode_Slot::AddReferencedObjects(FReferenceCollector& Collector)
{
	// OwnerAnimInstance is a non-owning back pointer; do not add it.
	if (InputPose)
	{
		InputPose->AddReferencedObjects(Collector);
	}
}
