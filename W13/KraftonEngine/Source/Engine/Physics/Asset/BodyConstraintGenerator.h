#pragma once

#include "Core/Types/CoreTypes.h"

class UPhysicsAsset;
class UBodySetup;
struct FSkeletalMesh;

// =====================================================================================
// FBodyConstraintGenerator — 스켈레톤 ref pose 로부터 PhysicsAsset 의 바디(UBodySetup)와
// 조인트(FConstraintSetup) 초기값을 생성/피팅한다.
//   - 바디 캡슐 : 본→첫 자식 방향/거리로 Center/Length/Radius/Rotation 피팅(leaf 는 기본).
//   - 조인트 프레임 : 앵커를 자식 본 원점에 — ParentFrame=자식의 부모-로컬 포즈, ChildFrame=identity.
// 에디터 저작 + 향후 런타임 자동 생성에서 공용으로 쓰도록 데이터 로직만 담는다(UI/에디터 비의존).
// =====================================================================================
namespace FBodyConstraintGenerator
{
	// 본 BoneIndex 에 바디 1개 생성(캡슐을 본 형상에 피팅) 후 Asset->BodySetups 에 추가한다.
	// 이미 해당 본 바디가 있거나 인자가 유효치 않으면 nullptr.
	UBodySetup* GenerateBody(UPhysicsAsset* Asset, const FSkeletalMesh* Mesh, int32 BoneIndex);

	// 자식 본 ChildBoneIndex → 부모 본 조인트 1개 생성(프레임 피팅) 후 Asset->ConstraintSetups 에 추가.
	// 추가된 인덱스 반환. 루트/중복/무효면 -1.
	int32 GenerateConstraint(UPhysicsAsset* Asset, const FSkeletalMesh* Mesh, int32 ChildBoneIndex);

	// 전체 스켈레톤에 대해 바디 + 조인트를 일괄 생성한다(이미 있는 본/조인트는 건너뜀).
	void GenerateAll(UPhysicsAsset* Asset, const FSkeletalMesh* Mesh);
}
