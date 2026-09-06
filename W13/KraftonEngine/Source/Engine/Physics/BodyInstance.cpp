#include "Physics/BodyInstance.h"

#include "Physics/Asset/BodySetup.h"
#include "Physics/IPhysicsScene.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "Component/PrimitiveComponent.h"

bool FBodyInstance::InitBody(UBodySetup* Setup, const FTransform& Transform, IPhysicsScene* InRBScene, int32 BoneIndex,
	FPhysicsAggregateHandle Aggregate, bool bStatic)
{
	if (!Setup || !InRBScene)
	{
		return false;
	}
	// 콜라이더가 있어야 한다 — AggGeom(convex/primitive) 또는 (static 일 때) 쿡된 trimesh.
	const bool bHasGeom = Setup->AggGeom.GetElementCount() > 0 || (bStatic && Setup->HasTriMeshCollision());
	if (!bHasGeom)
	{
		return false;
	}

	// 저작된 기본 인스턴스 플래그(Simulate/EnableGravity/StartAwake/OverrideMass 등)를 설정에서
	// 복사한다. 아래 PhysicsType 스위치가 bSimulatePhysics 를 최종 결정(Default 면 이 복사값 유지).
	// DefaultInstance.BodySetup 은 null 이므로 복사 직후 this->BodySetup 을 다시 세팅한다.
	static_cast<FBodyInstanceCore&>(*this) = Setup->DefaultInstance;

	BodySetup = Setup;
	InstanceBoneIndex = BoneIndex;
	switch (Setup->PhysicsType)
	{
	case PhysType_Simulated:
		bSimulatePhysics = true;
		break;
	case PhysType_Kinematic:
		bSimulatePhysics = false;
		break;
	case PhysType_Default:
	default:
		// 컴포넌트 경로면 아래에서 컴포넌트 플래그로 덮어쓴다. 랙돌은 복사값(DefaultInstance) 유지.
		break;
	}

	// 컴포넌트 경로(OwnerComponent 유효): 시뮬레이션 여부·질량을 컴포넌트가 최종 결정한다.
	// (BodySetup 의 DefaultInstance/DefaultMass 기본값을 컴포넌트 프로퍼티로 오버라이드.)
	if (OwnerComponent)
	{
		bSimulatePhysics = OwnerComponent->GetSimulatePhysics();
		bOverrideMass = true;
		MassInKgOverride = OwnerComponent->GetMass();
	}

	FActorCreationParams ActorParams;
	ActorParams.InitialTM = Transform;
	ActorParams.bStatic = bStatic;
	ActorParams.bSimulatePhysics = ShouldInstanceSimulatingPhysics();
	ActorParams.bStartAwake = bStartAwake;
	ActorParams.bEnableGravity = bEnableGravity;
	// PxActor->userData: 컴포넌트 경로는 소유 AActor*(raycast IgnoreActor 매칭용), 랙돌은 FBodyInstance*.
	ActorParams.UserData = OwnerComponent ? static_cast<void*>(OwnerComponent->GetOwner()) : static_cast<void*>(this);
	ActorParams.Aggregate = Aggregate;   // 유효하면 랙돌 aggregate 그룹에 묶여 생성

	ActorHandle = InRBScene->CreateActor(ActorParams);
	if (!ActorHandle.IsValid())
	{
		BodySetup = nullptr;
		InstanceBoneIndex = -1;
		return false;
	}

	// 컴포넌트 경로면 Component 모드(채널/응답/트리거 필터를 OwnerComponent 에서)로, 랙돌은 RawBody.
	// bAllowTriMesh = bStatic — trimesh 는 static/kinematic actor 만 백업 가능.
	const EShapeSetupMode ShapeMode = OwnerComponent ? EShapeSetupMode::Component : EShapeSetupMode::RawBody;
	if (!Setup->AddShapesToRigidActor(InRBScene, ActorHandle, Scale3D, FTransform(), Transform, this,
		ShapeMode, OwnerComponent, bStatic))
	{
		InRBScene->ReleaseActor(ActorHandle);
		ActorHandle = {};
		BodySetup = nullptr;
		InstanceBoneIndex = -1;
		return false;
	}

	InRBScene->SetActorKinematic(ActorHandle, !ShouldInstanceSimulatingPhysics());
	UpdateMassProperties(InRBScene);
	// 컴포넌트 경로: COM 오프셋도 컴포넌트 값으로(UpdateMassProperties 의 setMassAndUpdateInertia 가
	// COM 을 shape 분포로 재계산하므로 그 뒤에 명시 설정). 구 ApplyRootMassAndCOM 과 동일 의미.
	if (OwnerComponent)
	{
		InRBScene->SetCenterOfMass(ActorHandle, OwnerComponent->GetCenterOfMass());
	}
	return true;
}

void FBodyInstance::TermBody(IPhysicsScene* InRBScene)
{
	if (InRBScene && ActorHandle.IsValid())
	{
		InRBScene->ReleaseActor(ActorHandle);
	}

	ActorHandle = {};
	BodySetup = nullptr;
	InstanceBoneIndex = -1;
}

UBodySetup* FBodyInstance::GetBodySetup() const
{
	return static_cast<UBodySetup*>(BodySetup);
}

FTransform FBodyInstance::GetUnrealWorldTransform(IPhysicsScene* InRBScene) const
{
	if (!InRBScene || !ActorHandle.IsValid())
	{
		return FTransform();
	}

	return InRBScene->GetActorGlobalPose(ActorHandle);
}

void FBodyInstance::SetBodyTransform(IPhysicsScene* InRBScene, const FTransform& NewTransform)
{
	if (InRBScene && ActorHandle.IsValid())
	{
		InRBScene->SetActorGlobalPose(ActorHandle, NewTransform);
	}
}

void FBodyInstance::SetInstanceSimulatePhysics(IPhysicsScene* InRBScene, bool bSimulate)
{
	bSimulatePhysics = bSimulate;
	if (InRBScene && ActorHandle.IsValid())
	{
		InRBScene->SetActorKinematic(ActorHandle, !ShouldInstanceSimulatingPhysics());
	}
}

void FBodyInstance::SetKinematicTarget(IPhysicsScene* InRBScene, const FTransform& NewTarget)
{
	if (InRBScene && ActorHandle.IsValid())
	{
		InRBScene->SetActorKinematicTarget(ActorHandle, NewTarget);
	}
}

void FBodyInstance::UpdateMassProperties(IPhysicsScene* InRBScene)
{
	UBodySetup* Setup = GetBodySetup();
	if (InRBScene && ActorHandle.IsValid() && Setup)
	{
		const float EffectiveMass = (bOverrideMass && MassInKgOverride > 0.0f) ? MassInKgOverride : Setup->DefaultMass;
		InRBScene->SetActorMass(ActorHandle, EffectiveMass);
	}
}
