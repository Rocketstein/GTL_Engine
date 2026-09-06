#include "Physics/Asset/PhysicsAsset.h"

// =====================================================================================
// UPhysicsAsset 직렬화.
//   - SkeletonBinding : 비-USTRUCT(plain struct) → operator<< 수동.
//   - BodySetups      : UPROPERTY(Edit, Save, Instanced) → 배열 inner FObjectProperty 가
//                       PF_InstancedReference 를 받아 각 바디를 ClassName + Properties 로 재귀
//                       직렬화. 로드 시 FObjectFactory 가 Outer=this 로 생성.
//   - ConstraintSetups: UPROPERTY(Save) → FStructProperty 배열 리플렉션 자동.
//   BodySetups/ConstraintSetups 는 SerializeProperties 한 번으로 모두 처리된다.
// =====================================================================================
void UPhysicsAsset::Serialize(FArchive& Ar)
{
	// SkeletonBinding 만 plain struct 라 operator<< 로 수동 직렬화.
	Ar << SkeletonBinding;

	// BodySetups(Instanced) + ConstraintSetups — UPROPERTY(Save) 리플렉션으로 자동 직렬화.
	SerializeProperties(Ar, PF_Save);
}

int32 UPhysicsAsset::FindBodyIndex(FName BoneName) const
{
	for (int32 i = 0; i < static_cast<int32>(BodySetups.size()); ++i)
	{
		if (BodySetups[i] && BodySetups[i]->BoneName == BoneName)
		{
			return i;
		}
	}
	return -1;
}

int32 UPhysicsAsset::FindConstraintIndex(FName ChildBone) const
{
	for (int32 i = 0; i < static_cast<int32>(ConstraintSetups.size()); ++i)
	{
		if (ConstraintSetups[i].ChildBone == ChildBone)
		{
			return i;
		}
	}
	return -1;
}
