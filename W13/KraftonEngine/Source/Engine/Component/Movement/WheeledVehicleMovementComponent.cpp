#include "WheeledVehicleMovementComponent.h"

#include "Physics/PhysXPhysicsScene.h"
#include "Physics/PhysXVehicleManager.h"
#include "Component/SceneComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Core/Types/CollisionTypes.h"   // ECollisionChannel, ObjectTypeBit
#include "Core/Logging/Log.h"
#include "Math/Quat.h"
#include "Audio/AudioManager.h"

// PhysX 헤더는 .cpp 에서만 — 엔진 표면을 PhysX-free 로 유지 (PhysXPhysicsScene.cpp 와 동일).
#include <PxPhysicsAPI.h>
#include <algorithm>
#include <cmath>

using namespace physx;

namespace
{
	// 점 집합을 convex hull 로 쿠킹 → PxConvexMesh. insertion callback 경로라 stream 불필요.
	PxConvexMesh* CookConvexMesh(PxCooking* Cooking, PxPhysics* Physics, const PxVec3* Verts, PxU32 Count)
	{
		PxConvexMeshDesc Desc;
		Desc.points.count  = Count;
		Desc.points.stride = sizeof(PxVec3);
		Desc.points.data   = Verts;
		Desc.flags         = PxConvexFlag::eCOMPUTE_CONVEX;
		return Cooking->createConvexMesh(Desc, Physics->getPhysicsInsertionCallback());
	}

	// 차체 박스 (half extents) → 8-vertex convex.
	PxConvexMesh* CreateChassisConvex(PxCooking* Cooking, PxPhysics* Physics, float HalfX, float HalfY, float HalfZ)
	{
		const PxVec3 Verts[8] = {
			PxVec3(-HalfX, -HalfY, -HalfZ), PxVec3(-HalfX, -HalfY,  HalfZ),
			PxVec3(-HalfX,  HalfY, -HalfZ), PxVec3(-HalfX,  HalfY,  HalfZ),
			PxVec3( HalfX, -HalfY, -HalfZ), PxVec3( HalfX, -HalfY,  HalfZ),
			PxVec3( HalfX,  HalfY, -HalfZ), PxVec3( HalfX,  HalfY,  HalfZ),
		};
		return CookConvexMesh(Cooking, Physics, Verts, 8);
	}

	// 휠 원통 — 회전축은 측면(Y), 단면 원은 X-Z 평면. (basis: up=Z, forward=X)
	PxConvexMesh* CreateWheelConvex(PxCooking* Cooking, PxPhysics* Physics, float Radius, float Width)
	{
		const PxU32 Segments = 16;
		PxVec3 Verts[Segments * 2];
		const float HalfW = Width * 0.5f;
		for (PxU32 i = 0; i < Segments; ++i)
		{
			const float Angle = (PxTwoPi * i) / Segments;
			const float Cx = Radius * PxCos(Angle);
			const float Cz = Radius * PxSin(Angle);
			Verts[i * 2 + 0] = PxVec3(Cx, -HalfW, Cz);
			Verts[i * 2 + 1] = PxVec3(Cx,  HalfW, Cz);
		}
		return CookConvexMesh(Cooking, Physics, Verts, Segments * 2);
	}
}

// ============================================================
// 입력 — PlayerController/Lua 가 매 프레임 세팅. manager PreTick 이 읽어 PxVehicle 에 적용한다.
// 값은 PhysX 가 기대하는 범위로 clamp 해서 보관 (analog: throttle/brake/handbrake [0,1], steer [-1,1]).
// ============================================================
void UWheeledVehicleMovementComponent::SetThrottleInput(float Throttle)
{
	// 전/후진 통합 축: +1=전진, -1=후진. gear(eFIRST/eREVERSE) + accel/brake 변환은
	// FPhysXVehicleManager::PreTick 이 현재 전진속도와 함께 arcade auto-reverse 로 판단한다.
	ThrottleInput = std::clamp(Throttle, -0.5f, 1.0f);
}

void UWheeledVehicleMovementComponent::SetBrakeInput(float Brake)
{
	BrakeInput = std::clamp(Brake, 0.0f, 1.0f);
}

void UWheeledVehicleMovementComponent::SetSteeringInput(float Steering)
{
	SteeringInput = std::clamp(Steering, -1.0f, 1.0f);
}

void UWheeledVehicleMovementComponent::SetHandbrakeInput(bool bHandbrake)
{
	bHandbrakeInput = bHandbrake;
}

float UWheeledVehicleMovementComponent::GetEngineRotationSpeed() const
{
	return PVehicle ? PVehicle->mDriveDynData.getEngineRotationSpeed() : 0.0f;
}
float UWheeledVehicleMovementComponent::GetEngineRotationRatio() const
{
	return EngineMaxOmega > 0.0f
		? std::clamp(GetEngineRotationSpeed() / EngineMaxOmega, 0.0f, 1.0f) : 0.0f;
}

float UWheeledVehicleMovementComponent::GetForwardSpeed() const
{
	return PVehicle ? PVehicle->computeForwardSpeed() : 0.0f;
}

bool UWheeledVehicleMovementComponent::GetChassisWorldTransform(FTransform& Out) const
{
	if (!PVehicleActor)
	{
		return false;
	}
	// parametric: actor(PxRigidDynamic) origin == 차체 컴포넌트 origin (스폰 시 UpdatedComponent
	// 월드 포즈로 생성). 따라서 actor world pose 를 그대로 차체 world transform 으로 반환한다.
	const PxTransform T = PVehicleActor->getGlobalPose();
	Out = FTransform(
		FVector(T.p.x, T.p.y, T.p.z),
		FQuat(T.q.x, T.q.y, T.q.z, T.q.w),
		FVector(1.0f, 1.0f, 1.0f));
	return true;
}

// ============================================================
// Lifecycle
// ============================================================
void UWheeledVehicleMovementComponent::BeginPlay()
{
	// UMovementComponent::BeginPlay 가 UpdatedComponent 를 resolve (owner root) 한다.
	UPawnMovementComponent::BeginPlay();

	if (CreateVehicle())
	{
		RegisterWithManager();

		EngineLoopName = "VehicleEngine_" + std::to_string(GetOwner()->GetUUID());

		FAudioManager::Get().PlayAudio(EngineStartKey, 1.0f);
		FAudioManager::Get().PlayLoop(EngineLoopKey, EngineLoopName,
			/*Volume=*/0.0f, /*Pitch=*/IdlePitch);
	}
}

void UWheeledVehicleMovementComponent::EndPlay()
{
	if (!EngineLoopName.empty())
	{
		FAudioManager::Get().StopLoop(EngineLoopName);
		EngineLoopName.clear();
	}
	UnregisterFromManager();
	DestroyVehicle();
	UPawnMovementComponent::EndPlay();
}

void UWheeledVehicleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UPawnMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 차량은 입력 phase(manager PreTick, pre-sim)와 출력 phase(actor Tick, post-fetch)가
	// 컴포넌트 Tick 바깥에서 돈다. 컴포넌트 단독 per-frame 로직이 생기면 여기 둔다.

	const float Ratio = GetEngineRotationRatio();                 // [0,1] vs EngineMaxOmega
	const float Pitch = std::lerp(IdlePitch, MaxPitch, Ratio);
	const float Volume = std::lerp(0.0f, 1.0f, Ratio);

	FAudioManager::Get().SetLoopPitch(EngineLoopName, Pitch);
	FAudioManager::Get().SetLoopVolume(EngineLoopName, Volume);
}

// ============================================================
// Manager 핸드셰이크
// ============================================================
void UWheeledVehicleMovementComponent::RegisterWithManager()
{
	VehicleManager = ResolveVehicleManager();
	if (VehicleManager)
	{
		VehicleManager->RegisterVehicleMC(this);
	}
}

void UWheeledVehicleMovementComponent::UnregisterFromManager()
{
	if (VehicleManager)
	{
		VehicleManager->UnRegisterVehicleMC(this);
		VehicleManager = nullptr;
	}
}

FPhysXVehicleManager* UWheeledVehicleMovementComponent::ResolveVehicleManager() const
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	IPhysicsScene* Scene = World ? World->GetPhysicsScene() : nullptr;
	return Scene ? static_cast<FPhysXPhysicsScene*>(Scene)->GetVehicleManager() : nullptr;
}

// ============================================================
// PxVehicleDrive4W 생성/파괴
// ============================================================
bool UWheeledVehicleMovementComponent::CreateVehicle()
{
	// PhysX 컨텍스트는 scene-owned manager 가 단일 허브로 제공 (seed-the-manager).
	VehicleManager = ResolveVehicleManager();
	if (!VehicleManager)
	{
		UE_LOG("[WheeledVehicleMC] CreateVehicle: vehicle manager unavailable (physics scene not ready).");
		return false;
	}

	PxPhysics*  Physics  = VehicleManager->GetPhysics();
	PxCooking*  Cooking  = VehicleManager->GetCooking();
	PxScene*    PScene   = VehicleManager->GetScene();
	PxMaterial* Material = VehicleManager->GetDriveMaterial();
	if (!Physics || !Cooking || !PScene || !Material)
	{
		UE_LOG("[WheeledVehicleMC] CreateVehicle: incomplete PhysX context.");
		return false;
	}

	using WO = PxVehicleDrive4WWheelOrder;
	const PxU32 NW = static_cast<PxU32>(NumWheels);

	// --- 차체/휠 치수는 배치된 mesh 의 로컬 바운드에서 유도 (property 는 mesh 없을 때의 fallback) ---
	// +X=forward, Y=side, +Z=up. 차체 = MC 의 UpdatedComponent(= AWheeledVehicle 의 Root BodyMesh),
	// 휠 = 그 BodyMesh 의 static-mesh 자식(순서 = PxVehicleDrive4WWheelOrder: 0=FL,1=FR,2=RL,3=RR).
	UStaticMeshComponent* BodyMesh = Cast<UStaticMeshComponent>(GetUpdatedComponent());

	// 차체 박스 half-extent + 메시 지오메트리 중심(컴포넌트 원점 대비). 기본은 property fallback.
	float  HalfX = ChassisLength * 0.5f;
	float  HalfY = ChassisWidth  * 0.5f;
	float  HalfZ = ChassisHeight * 0.5f;
	PxVec3 ChassisShapeCenter(0.0f);   // mesh pivot 이 지오메트리 중심과 다를 때의 보정 (shape localPose)
	{
		FVector BC, BE;
		if (BodyMesh && BodyMesh->GetLocalBounds(BC, BE))
		{
			const FVector S = BodyMesh->GetWorldScale();   // 메시-로컬 바운드를 월드 치수로
			HalfX = BE.X * PxAbs((float)S.X);
			HalfY = BE.Y * PxAbs((float)S.Y);
			HalfZ = BE.Z * PxAbs((float)S.Z);
			ChassisShapeCenter = PxVec3(BC.X * (float)S.X, BC.Y * (float)S.Y, BC.Z * (float)S.Z);
		}
	}

	// 휠 컴포넌트 수집 (BodyMesh 의 static-mesh 자식, 순서대로 — actor EnsureComponents 와 동일 규칙).
	UStaticMeshComponent* WheelComps[4] = { nullptr, nullptr, nullptr, nullptr };
	if (BodyMesh)
	{
		int32 Found = 0;
		for (USceneComponent* Child : BodyMesh->GetChildren())
		{
			if (Found >= NumWheels) break;
			if (UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Child))
				WheelComps[Found++] = SM;
		}
	}

	// 휠 반경/폭: 첫 번째 "메시가 지정된" 휠의 바운드에서 유도 (없으면 property fallback).
	// 휠 컨벤션: 허브=컴포넌트 원점, 회전축=로컬 Y, 단면 원=X-Z → radius=X·Z half-extent 의 max, width=Y 전폭.
	float DerivedWheelRadius = WheelRadius;
	float DerivedWheelWidth  = WheelWidth;

	// 휠 중심: 우선 parametric 4코너로 채운 뒤, 메시가 지정된 휠은 실제 배치 위치(relative location)로 덮어쓴다.
	const float FrontX = HalfX - DerivedWheelRadius;
	const float TrackY = HalfY;
	const float WheelZ = -HalfZ;

	PxVec3 WheelCenters[4];
	WheelCenters[WO::eFRONT_LEFT]  = PxVec3( FrontX,  TrackY, WheelZ);
	WheelCenters[WO::eFRONT_RIGHT] = PxVec3( FrontX, -TrackY, WheelZ);
	WheelCenters[WO::eREAR_LEFT]   = PxVec3(-FrontX,  TrackY, WheelZ);
	WheelCenters[WO::eREAR_RIGHT]  = PxVec3(-FrontX, -TrackY, WheelZ);

	bool bWheelSizeFromMesh = false;
	for (PxU32 i = 0; i < NW; ++i)
	{
		UStaticMeshComponent* WC = WheelComps[i];
		FVector WBC, WBE;
		if (!WC || !WC->GetLocalBounds(WBC, WBE))
			continue;   // 메시 미지정 휠은 parametric 코너 유지

		// 실제 배치 위치 = BodyMesh(=actor origin) 기준 relative location → 휠 중심 오프셋(actor frame).
		const FVector L = WC->GetRelativeLocation();
		WheelCenters[i] = PxVec3((float)L.X, (float)L.Y, (float)L.Z);

		if (!bWheelSizeFromMesh)
		{
			const FVector WS = WC->GetWorldScale();
			DerivedWheelRadius = std::max(WBE.X * PxAbs((float)WS.X), WBE.Z * PxAbs((float)WS.Z));
			DerivedWheelWidth  = 2.0f * WBE.Y * PxAbs((float)WS.Y);
			bWheelSizeFromMesh = true;
		}
	}

	// CoM 은 지오메트리 중심(X,Y) 위에 두고 Z 만 CenterOfMassOffsetZ 로 낮춰 전복 안정성↑.
	const PxVec3 ChassisCM(ChassisShapeCenter.x, ChassisShapeCenter.y, ChassisShapeCenter.z + CenterOfMassOffsetZ);

	const PxU32 OwnerUUID = GetOwner() ? GetOwner()->GetUUID() : 0;

	// --- 필터 ---
	// suspension-raycast prefilter 와 same-owner 충돌 억제 모두 word3=ownerUUID 에 의존.
	PxFilterData VehicleQryFilter;
	VehicleQryFilter.word0 = static_cast<PxU32>(ECollisionChannel::WorldDynamic);
	VehicleQryFilter.word3 = OwnerUUID;

	// 휠은 sim 충돌 OFF — 서스펜션 raycast 가 지면 접촉을 담당.
	PxFilterData WheelSimFilter;
	WheelSimFilter.word0 = static_cast<PxU32>(ECollisionChannel::WorldDynamic);
	WheelSimFilter.word3 = OwnerUUID;

	// 차체는 WorldStatic/WorldDynamic 과 충돌.
	PxFilterData ChassisSimFilter;
	ChassisSimFilter.word0 = static_cast<PxU32>(ECollisionChannel::WorldDynamic);
	ChassisSimFilter.word1 = ObjectTypeBit(ECollisionChannel::WorldStatic) | ObjectTypeBit(ECollisionChannel::WorldDynamic);
	ChassisSimFilter.word3 = OwnerUUID;

	// --- chassis/wheel convex cook (mutation 전에 먼저 — 실패 시 깨끗이 abort) ---
	PxConvexMesh* WheelMesh   = CreateWheelConvex(Cooking, Physics, DerivedWheelRadius, DerivedWheelWidth);
	PxConvexMesh* ChassisMesh = CreateChassisConvex(Cooking, Physics, HalfX, HalfY, HalfZ);
	if (!WheelMesh || !ChassisMesh)
	{
		UE_LOG("[WheeledVehicleMC] CreateVehicle: convex cooking failed.");
		if (WheelMesh)   WheelMesh->release();
		if (ChassisMesh) ChassisMesh->release();
		return false;
	}

	// --- chassis actor + chassis shape (parametric box convex — actor 를 우리가 만들고 소유) ---
	PxRigidDynamic* Actor = Physics->createRigidDynamic(PxTransform(PxIdentity));
	PxShape* ChassisShape = PxRigidActorExt::createExclusiveShape(*Actor, PxConvexMeshGeometry(ChassisMesh), *Material);
	ChassisShape->setSimulationFilterData(ChassisSimFilter);
	ChassisShape->setQueryFilterData(VehicleQryFilter);
	ChassisShape->setLocalPose(PxTransform(ChassisShapeCenter));   // mesh pivot↔지오메트리 중심 보정

	// 스폰 위치 — UpdatedComponent 월드 트랜스폼.
	if (USceneComponent* Updated = GetUpdatedComponent())
	{
		const FVector P = Updated->GetWorldLocation();
		const FQuat   Q = FQuat::FromMatrix(Updated->GetWorldMatrix());
		Actor->setGlobalPose(PxTransform(PxVec3(P.X, P.Y, P.Z), PxQuat(Q.X, Q.Y, Q.Z, Q.W)));
	}

	// --- wheel shapes: 항상 procedural, chassis shapes 다음 인덱스에 추가 ---
	const PxU32 WheelShapeBase = Actor->getNbShapes();
	for (PxU32 i = 0; i < NW; ++i)
	{
		PxShape* WheelShape = PxRigidActorExt::createExclusiveShape(*Actor, PxConvexMeshGeometry(WheelMesh), *Material);
		WheelShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);   // 서스펜션이 지면 접촉 담당
		WheelShape->setSimulationFilterData(WheelSimFilter);
		WheelShape->setQueryFilterData(VehicleQryFilter);
		WheelShape->setLocalPose(PxTransform(PxIdentity));            // PxVehicleUpdates 가 매 프레임 갱신
	}

	// shape 들이 mesh ref 를 잡았으므로 로컬 생성 ref 해제 (cook 한 것만).
	WheelMesh->release();
	if (ChassisMesh) ChassisMesh->release();

	// --- mass/inertia: ChassisMass. 관성 텐서는 chassis(sim) shape 로 계산하고 CoM 은 낮춤 값 유지.
	//     (wheels 는 eSIMULATION_SHAPE=false → 관성 계산에서 자동 제외.) ---
	const float Mass = ChassisMass;
	PxRigidBodyExt::setMassAndUpdateInertia(*Actor, Mass, &ChassisCM);

	// --- wheels sim data ---
	PxVehicleWheelsSimData* WheelsSimData = PxVehicleWheelsSimData::allocate(NW);

	float SprungMasses[4];
	PxVehicleComputeSprungMasses(NW, WheelCenters, ChassisCM, Mass, /*gravityDir=Z*/2, SprungMasses);

	const float WheelMOI    = 0.5f * WheelMass * DerivedWheelRadius * DerivedWheelRadius;
	const float MaxSteerRad = MaxSteerAngle * (PxPi / 180.0f);

	for (PxU32 i = 0; i < NW; ++i)
	{
		// steer/handbrake 적용은 WheelSetups 플래그로 (설정 부족 시 index 기본: 앞=steer, 뒤=handbrake).
		const bool bHasSetup = (static_cast<int32>(i) < static_cast<int32>(WheelSetups.size()));
		const bool bSteer = bHasSetup ? WheelSetups[i].bAffectedBySteering  : (i == WO::eFRONT_LEFT || i == WO::eFRONT_RIGHT);
		const bool bHand  = bHasSetup ? WheelSetups[i].bAffectedByHandbrake : (i == WO::eREAR_LEFT  || i == WO::eREAR_RIGHT);

		PxVehicleWheelData Wheel;
		Wheel.mMass               = WheelMass;
		Wheel.mMOI                = WheelMOI;
		Wheel.mRadius             = DerivedWheelRadius;
		Wheel.mWidth              = DerivedWheelWidth;
		Wheel.mMaxBrakeTorque     = 1500.0f;
		Wheel.mMaxSteer           = bSteer ? MaxSteerRad : 0.0f;
		Wheel.mMaxHandBrakeTorque = bHand  ? 4000.0f : 0.0f;

		PxVehicleTireData Tire;
		Tire.mType = 0;   // FrictionPairs 의 tire 타입 0

		PxVehicleSuspensionData Susp;
		Susp.mMaxCompression   = 0.3f;
		Susp.mMaxDroop         = 0.1f;
		Susp.mSpringStrength   = 35000.0f;
		Susp.mSpringDamperRate = 4500.0f;
		Susp.mSprungMass       = SprungMasses[i];

		const PxVec3 CMOffset = WheelCenters[i] - ChassisCM;

		WheelsSimData->setWheelData(i, Wheel);
		WheelsSimData->setTireData(i, Tire);
		WheelsSimData->setSuspensionData(i, Susp);
		WheelsSimData->setSuspTravelDirection(i, PxVec3(0.0f, 0.0f, -1.0f));
		WheelsSimData->setWheelCentreOffset(i, CMOffset);
		WheelsSimData->setSuspForceAppPointOffset(i, PxVec3(CMOffset.x, CMOffset.y, -0.3f));
		WheelsSimData->setTireForceAppPointOffset(i, PxVec3(CMOffset.x, CMOffset.y, -0.3f));
		WheelsSimData->setSceneQueryFilterData(i, VehicleQryFilter);
		// Wheel shapes are appended after the parametric chassis shape.
		WheelsSimData->setWheelShapeMapping(i, PxI32(WheelShapeBase + i));
	}

	// --- drive sim data ---
	PxVehicleDriveSimData4W DriveSimData;

	PxVehicleDifferential4WData Diff;
	Diff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
	DriveSimData.setDiffData(Diff);

	PxVehicleEngineData Engine;
	Engine.mPeakTorque = EnginePeakTorque;
	Engine.mMaxOmega   = EngineMaxOmega;
	DriveSimData.setEngineData(Engine);

	PxVehicleGearsData Gears;
	Gears.mSwitchTime = 0.5f;
	DriveSimData.setGearsData(Gears);

	PxVehicleClutchData Clutch;
	Clutch.mStrength = 10.0f;
	DriveSimData.setClutchData(Clutch);

	PxVehicleAckermannGeometryData Ackermann;
	Ackermann.mAccuracy       = 1.0f;
	Ackermann.mAxleSeparation = PxAbs(WheelCenters[WO::eFRONT_LEFT].x - WheelCenters[WO::eREAR_LEFT].x);
	Ackermann.mFrontWidth     = PxAbs(WheelCenters[WO::eFRONT_LEFT].y - WheelCenters[WO::eFRONT_RIGHT].y);
	Ackermann.mRearWidth      = PxAbs(WheelCenters[WO::eREAR_LEFT].y  - WheelCenters[WO::eREAR_RIGHT].y);
	DriveSimData.setAckermannGeometryData(Ackermann);

	// --- PxVehicleDrive4W ---
	PxVehicleDrive4W* Vehicle = PxVehicleDrive4W::allocate(NW);
	Vehicle->setup(Physics, Actor, *WheelsSimData, DriveSimData, /*nbNonDrivenWheels*/0);
	WheelsSimData->free();   // setup 이 deep-copy 함

	if (!Vehicle->getRigidDynamicActor())
	{
		UE_LOG("[WheeledVehicleMC] CreateVehicle: PxVehicleDrive4W::setup failed (invalid wheel/sim data) — aborting.");
		Vehicle->free();
		Actor->release();
		return false;
	}

	Vehicle->setToRestState();
	Vehicle->mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);
	Vehicle->mDriveDynData.setUseAutoGears(true);

	// actor 를 scene 에 추가. actor 는 BodyMappings 밖이라 scene post-sync 가 transform 을 안 건드린다 —
	// 이 vehicle(manager Tick) 이 유일한 writer, AWheeledVehicle::Tick 이 readback 한다.
	PScene->addActor(*Actor);

	PVehicle      = Vehicle;
	PVehicleActor = Actor;

	UE_LOG("[WheeledVehicleMC] CreateVehicle: PxVehicleDrive4W created (chassis %.2fx%.2fx%.2f m, wheel r=%.2f w=%.2f, wheels-from-mesh=%d, mass=%.1f kg).",
		HalfX * 2.0f, HalfY * 2.0f, HalfZ * 2.0f, DerivedWheelRadius, DerivedWheelWidth, bWheelSizeFromMesh ? 1 : 0, Mass);
	return true;
}

void UWheeledVehicleMovementComponent::DestroyVehicle()
{
	if (PVehicle)
	{
		PVehicle->free();
		PVehicle = nullptr;
	}

	// parametric — actor 는 우리가 소유 → release.
	if (PVehicleActor)
	{
		PVehicleActor->release();
	}
	PVehicleActor = nullptr;
}

// ============================================================
// Wheel pose 출력 — manager 의 이번 프레임 wheel local pose(chassis 공간)를 그대로 반환.
// AWheeledVehicle::Tick 이 받아 각 wheel static mesh 컴포넌트의 relative transform 에 적용한다.
// ============================================================
int32 UWheeledVehicleMovementComponent::GetWheelPoses(FTransform* OutPoses, int32 Max) const
{
	if (!VehicleManager || !OutPoses || Max <= 0) return 0;
	return VehicleManager->GetWheelLocalPoses(this, OutPoses, Max);
}
