#pragma once

#include "Object/Object.h"
#include "Core/Types/CoreTypes.h"
#include "Physics/Asset/BodySetup.h"
#include "Physics/Asset/ConstraintSetup.h"
#include "Animation/Skeleton/SkeletonTypes.h"   // FSkeletonBinding
#include "Object/Reflection/ObjectMacros.h"

#include "Source/Engine/Physics/Asset/PhysicsAsset.generated.h"

// =====================================================================================
// UPhysicsAsset — 발제의 최상위 물리 에셋. 한 스켈레톤에 대한 바디(UBodySetup) 배열 +
// 조인트(FConstraintSetup) 배열을 보유한다.
//   - 저작 : PhysicsAsset 에디터(B)
//   - 소비 : 랙돌(B) — USkeletalMeshComponent 가 인스턴스화 / 디버그 바디 렌더(C)
//
//   직렬화: BodySetups 는 UPROPERTY(Edit, Save, Instanced) 리플렉션 자동
//   (FArrayProperty + inner FObjectProperty(PF_InstancedReference)).
//   ConstraintSetups 도 FStructProperty 배열로 자동 직렬화된다.
// =====================================================================================
UCLASS()
class UPhysicsAsset : public UObject
{
public:
	GENERATED_BODY()
	UPhysicsAsset() = default;
	~UPhysicsAsset() override = default;

	// 직렬화:
	//   - SkeletonBinding   : 비-USTRUCT(plain) → operator<< 수동
	//   - BodySetups        : UPROPERTY(Edit, Save, Instanced) → inner FObjectProperty 가
	//                         PF_InstancedReference 를 받아 각 바디를 재귀 직렬화.
	//   - ConstraintSetups  : UPROPERTY(Save) → 리플렉션(FStructProperty 배열) 자동.
	void Serialize(FArchive& Ar) override;

	// BoneName 으로 바디/조인트 조회 — 랙돌 인스턴스화·에디터 선택용.
	int32 FindBodyIndex(FName BoneName) const;
	int32 FindConstraintIndex(FName ChildBone) const;

	// 저장 경로(.uasset). 다른 에셋 매니저 패턴과 동일하게 자체 보유.
	void           SetSourcePath(const FString& InPath) { SourcePath = InPath; }
	const FString& GetSourcePath() const { return SourcePath; }

public:
	// 바디(본별 1개). Instanced UObject 라 path 가 아닌 포인터로 소유.
	// Instanced 지정자로 배열 inner FObjectProperty 가 PF_InstancedReference 를 받아,
	// SerializeProperties 가 각 바디를 ClassName + Properties 로 재귀 직렬화한다.
	UPROPERTY(Edit, Save, Instanced, Category="Physics", DisplayName="Bodies")
	TArray<UBodySetup*>      BodySetups;
	// 부모-자식 본을 잇는 D6 조인트. 리플렉션으로 자동 직렬화.
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Constraints")
	TArray<FConstraintSetup> ConstraintSetups;

	// 어느 스켈레톤 기준인지 — 기존 스켈레톤-참조 에셋과 동일한 바인딩 패턴 재사용.
	FSkeletonBinding SkeletonBinding;

private:
	FString SourcePath;
};
