#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Physics/Asset/PhysicsAssetTypes.h"
#include "Physics/PhysicsHandles.h"

struct FConstraintSetup;
class UPrimitiveComponent;

// shape 의 필터/트리거/userData 셋업 정책.
//   RawBody   : PhysicsAsset/랙돌 경로. block-all 필터 + userData = FGeometryAddParams::UserData.
//   Component : UPrimitiveComponent 경로. 채널/응답/트리거/owner-UUID 필터 + shape userData = 컴포넌트.
// 두 경로가 같은 지오메트리 빌더(AddGeometry)를 공유하되 이 정책으로만 차이를 표현한다.
enum class EShapeSetupMode
{
	RawBody,
	Component
};

struct FActorCreationParams
{
	FTransform InitialTM;
	bool bStatic = false;
	bool bQueryOnly = false;
	bool bEnableGravity = true;
	bool bSimulatePhysics = false;
	bool bStartAwake = true;
	const char* DebugName = nullptr;
	void* UserData = nullptr;
	// 유효하면 actor 를 씬에 직접 추가하는 대신 이 aggregate 에 넣는다(랙돌 그룹핑).
	FPhysicsAggregateHandle Aggregate;
};

struct FGeometryAddParams
{
	bool bDoubleSided = false;
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);
	FTransform LocalTransform;
	FTransform WorldTransform;
	const FKAggregateGeom* Geometry = nullptr;
	// 설정 시 쿡된 트라이앵글 메시 shape 로 attach 한다(정적/키네마틱 actor 전용). Geometry 와
	// 둘 중 하나만 유효하면 된다 — trimesh 가 있으면 trimesh, 없으면 Geometry(convex) 를 쓴다.
	const TArray<uint8>* CookedTriMesh = nullptr;
	void* UserData = nullptr;
	// shape 셋업 정책. 기본 RawBody = 기존 랙돌 거동 그대로.
	EShapeSetupMode ShapeSetupMode = EShapeSetupMode::RawBody;
	// Component 모드일 때 필터/트리거 응답을 끌어올 소스 컴포넌트(shape userData 도 이 컴포넌트로 설정).
	UPrimitiveComponent* FilterSourceComponent = nullptr;
};

struct FConstraintCreationParams
{
	FPhysicsActorHandle Actor1;
	FPhysicsActorHandle Actor2;
	FTransform LocalFrame1;
	FTransform LocalFrame2;
	const FConstraintSetup* ConstraintSetup = nullptr;
	const char* DebugName = nullptr;
	void* UserData = nullptr;
};
