#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Transform.h"
#include "Math/MathUtils.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"
#include "Serialization/Archive.h"

#include "Source/Engine/Physics/Asset/ConstraintSetup.generated.h"

// D6 조인트 각 축의 자유도 처리. UE 의 ELinear/EAngularConstraintMotion 대응.
//   Locked  : 완전 고정
//   Limited : 한계값(라디안/거리)까지 허용
//   Free    : 무제한
// 첫 값이 reflection 콤보의 기본값 — 선형은 Locked, 각도는 Limited 가 기본(랙돌 관절 통상).
UENUM()
enum ELinearConstraintMotion
{
	LCM_Locked,
	LCM_Limited,
	LCM_Free,
};

UENUM()
enum EAngularConstraintMotion
{
	ACM_Limited,
	ACM_Locked,
	ACM_Free,
};

// =====================================================================================
// FConstraintSetup — D6Joint 1개의 저작 데이터. 부모-자식 본을 잇고 각 축의 모션
// (Locked/Limited/Free)과 한계를 정의한다. 발제 PxD6Joint 매핑:
//   Linear X/Y/Z     -> PxD6Axis::eX/eY/eZ      + (Limited 면) setDistanceLimit
//   TwistMotion      -> PxD6Axis::eTWIST        + (Limited 면) setTwistLimit(±TwistLimit)
//   Swing1/2 Motion  -> eSWING1 / eSWING2       + (Limited 면) setSwingLimit(cone)
//   Parent/ChildFrame-> PxD6JointCreate 의 localFrameParent / localFrameChild
// DriveStiffness / DriveDamping 은 3-1 융화(애니 포즈로 끌어당기는 드라이브)용.
// =====================================================================================
USTRUCT()
struct FConstraintSetup
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Constraint", DisplayName="Parent Bone")
	FName ParentBone;
	UPROPERTY(Edit, Save, Category="Constraint", DisplayName="Child Bone")
	FName ChildBone;

	// 본 로컬 프레임 = 조인트 앵커
	UPROPERTY(Edit, Save, Category="Frame", DisplayName="Parent Frame")
	FTransform ParentFrame;
	UPROPERTY(Edit, Save, Category="Frame", DisplayName="Child Frame")
	FTransform ChildFrame;

	// 선형 축 모션. 랙돌은 통상 전부 Locked. Limited 면 LinearLimit(거리) 적용.
	UPROPERTY(Edit, Save, Category="Linear", DisplayName="Linear X Motion", Enum=ELinearConstraintMotion)
	ELinearConstraintMotion LinearXMotion = LCM_Locked;
	UPROPERTY(Edit, Save, Category="Linear", DisplayName="Linear Y Motion", Enum=ELinearConstraintMotion)
	ELinearConstraintMotion LinearYMotion = LCM_Locked;
	UPROPERTY(Edit, Save, Category="Linear", DisplayName="Linear Z Motion", Enum=ELinearConstraintMotion)
	ELinearConstraintMotion LinearZMotion = LCM_Locked;
	UPROPERTY(Edit, Save, Category="Linear", DisplayName="Linear Limit", Min=0.f, Speed=0.1f)
	float LinearLimit = 0.f;

	// 각도 축 모션 + 한계(도, degree). Limited 일 때만 해당 한계 적용. CreateConstraint 와
	// 시각화에서 라디안으로 변환해 사용한다(UE PhysAT 과 동일하게 저작은 도 단위).
	UPROPERTY(Edit, Save, Category="Angular", DisplayName="Twist Motion", Enum=EAngularConstraintMotion)
	EAngularConstraintMotion TwistMotion = ACM_Limited;
	UPROPERTY(Edit, Save, Category="Angular", DisplayName="Twist Limit (deg)", Min=0.f, Max=180.f, Speed=1.f)
	float TwistLimit  = 45.f;   // eTWIST
	UPROPERTY(Edit, Save, Category="Angular", DisplayName="Swing1 Motion", Enum=EAngularConstraintMotion)
	EAngularConstraintMotion Swing1Motion = ACM_Limited;
	UPROPERTY(Edit, Save, Category="Angular", DisplayName="Swing1 Limit (deg)", Min=0.f, Max=180.f, Speed=1.f)
	float Swing1Limit = 30.f;   // eSWING1
	UPROPERTY(Edit, Save, Category="Angular", DisplayName="Swing2 Motion", Enum=EAngularConstraintMotion)
	EAngularConstraintMotion Swing2Motion = ACM_Limited;
	UPROPERTY(Edit, Save, Category="Angular", DisplayName="Swing2 Limit (deg)", Min=0.f, Max=180.f, Speed=1.f)
	float Swing2Limit = 30.f;   // eSWING2

	// 3-1 융화용 드라이브. 0 이면 순수 제한 조인트. 본 포즈 ↔ 시뮬 포즈 블렌딩 시
	// 애니 타깃으로 끌어당기는 스프링 강성/감쇠.
	UPROPERTY(Edit, Save, Category="Drive", DisplayName="Drive Stiffness", Min=0.f, Speed=1.f)
	float DriveStiffness = 0.f;
	UPROPERTY(Edit, Save, Category="Drive", DisplayName="Drive Damping", Min=0.f, Speed=1.f)
	float DriveDamping   = 0.f;

	// 직렬화는 리플렉션(FStructProperty)이 전담한다. FName/FTransform 포함 모든 멤버를
	// 코드젠된 RegisterProperties + FProperty::SerializeValue 가 재귀 처리하므로
	// 커스텀 operator<< 는 불필요(EPropertyType::Transform 추가로 FTransform 도 지원됨).
};
