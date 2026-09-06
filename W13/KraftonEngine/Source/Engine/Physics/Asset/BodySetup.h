#pragma once

#include "Object/Object.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Transform.h"
#include "Physics/Asset/PhysicsAssetTypes.h"
#include "Physics/PhysicsHandles.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "Physics/BodyInstanceCore.h"
#include "Object/Reflection/ObjectMacros.h"

#include "Source/Engine/Physics/Asset/BodySetup.generated.h"

class IPhysicsScene;
class UPrimitiveComponent;
struct FStaticMesh;

UENUM()
enum EPhysicsType
{
	// Follow owner component simulation state.
	PhysType_Default,
	// Kinematic body: moves from animation/explicit target, not simulation.
	PhysType_Kinematic,
	// Simulated body: driven by physics.
	PhysType_Simulated
};

// =====================================================================================
// UBodySetupCore / UBodySetup — UE BodySetup 계층의 최소형.
//   UBodySetupCore : PhysicsAsset 에서 본 연결과 기본 physics type 을 보유.
//   UBodySetup     : 공유되는 collision geometry 와 기본 질량 데이터.
//   FBodyInstance  : 월드에 생성된 actor handle 과 런타임 상태.
// =====================================================================================
UCLASS()
class UBodySetupCore : public UObject
{
public:
	GENERATED_BODY()
	UBodySetupCore() = default;
	~UBodySetupCore() override = default;

	UPROPERTY(Edit, Save, Category="BodySetup", DisplayName="Bone Name")
	FName BoneName;
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Physics Type", Enum=EPhysicsType)
	EPhysicsType PhysicsType = PhysType_Default;
};

UCLASS()
class UBodySetup : public UBodySetupCore
{
public:
	GENERATED_BODY()
	UBodySetup() = default;
	~UBodySetup() override = default;

	// 직렬화는 소유자 UPhysicsAsset::Serialize 가 SerializeProperties(Ar, PF_Save) 를 직접 호출해
	// 처리한다(인스턴스드 UObject 배열 — ObjectName 등 identity 는 굽지 않는다). 별도 Serialize
	// 오버라이드는 불필요. SerializeProperties 가 부모 UBodySetupCore(BoneName/PhysicsType)까지
	// 순회하며 FStructProperty(AggGeom)·FArrayProperty·Vec3·Rotator 를 모두 재귀 처리한다.

	// ShapeSetupMode/FilterSourceComponent: 컴포넌트 경로면 Component 모드로 채널/응답/트리거 필터를
	//   FilterSourceComponent 에서 끌어온다(shape userData 도 그 컴포넌트). 기본 RawBody = 랙돌.
	// bAllowTriMesh: 바디가 static/kinematic 이라 트라이앵글 메시 shape 를 붙여도 되는 경우 true.
	//   true 이고 HasTriMeshCollision() 이면 trimesh 를, 아니면 AggGeom(convex) 를 사용한다.
	bool AddShapesToRigidActor(IPhysicsScene* Scene, FPhysicsActorHandle ActorHandle,
		const FVector& Scale3D = FVector(1.0f, 1.0f, 1.0f),
		const FTransform& RelativeTM = FTransform(),
		const FTransform& WorldTransform = FTransform(),
		void* UserData = nullptr,
		EShapeSetupMode ShapeSetupMode = EShapeSetupMode::RawBody,
		UPrimitiveComponent* FilterSourceComponent = nullptr,
		bool bAllowTriMesh = false) const;
	bool BuildTriMeshFromStaticMesh(const FStaticMesh& Mesh, FString* OutError = nullptr);
	bool HasTriMeshCollision() const { return TriMesh.HasCookedData(); }

	UPROPERTY(Edit, Save, Category="BodySetup", DisplayName="Primitives", Type=Struct)
	FKAggregateGeom AggGeom;
	FTriMeshCollisionData TriMesh;
	UPROPERTY(Edit, Save, Category="BodySetup", DisplayName="Default Mass", Min=0.f, Speed=0.1f)
	float DefaultMass = 1.f;
	// 본별 기본 바디 인스턴스 설정(Simulate / Enable Gravity / Start Awake / Override Mass 등).
	// FBodyInstance::InitBody 가 런타임 인스턴스 생성 시 여기서 플래그를 복사한다.
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Default Instance", Type=Struct)
	FBodyInstanceCore DefaultInstance;
};
