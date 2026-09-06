#include "PhysXVehicleManager.h"

#include "Component/Movement/WheeledVehicleMovementComponent.h"
#include "Core/Types/CollisionTypes.h"   // ECollisionChannel (drivable surface gate)

// PhysX 헤더는 .cpp 에서만 (엔진 표면 PhysX-free).
#include <PxPhysicsAPI.h>
#include <algorithm>

using namespace physx;

namespace 
{
	// 아날로그 입력 smoothing (rise/fall rate). 컴포넌트가 throttle/brake/steer 를
	// 아날로그([0,1] / [-1,1])로 보관하므로 digital 이 아니라 analog smoothing 을 쓴다.
	static const PxVehiclePadSmoothingData gPadSmoothingData =
	{	// ACCEL, BRAKE, HANDBRAKE, STEER_LEFT, STEER_RIGHT
		{  6.f,   6.f,   12.f,      3.0f,       3.0f },     // Rise rates — 조향은 천천히 인가해 고속 횡력 스파이크 완화
		{ 10.f,   10.f,  12.f,      7.5f,       7.5f },      // Fall rates
	};

	// 전진 속도가 오를수록 허용 최대 조향각을 줄인다(steer scale ×MaxSteerAngle). 고속에서 조향
	// 권한을 크게 낮춰 타이어 횡력 포화(고속+풀스티어 → 접지 상실/드리프트)를 방지하고, 저속 기동성은 유지.
	static const PxF32 gSteerVsForwardSpeedData[] =
	{
		0.0f,        0.75f,    //   0 km/h → 22.5°
		6.0f,        0.55f,    // ~22 km/h → 16.5°
		14.0f,       0.38f,    // ~50 km/h → 11.4°
		25.0f,       0.24f,    // ~90 km/h → 7.2°
		45.0f,       0.16f,    // ~160 km/h → 4.8°
		120.0f,      0.1f,    //          → 3.0°
	};
	static const PxFixedSizeLookupTable<8> gSteerVsForwardSpeedTable(gSteerVsForwardSpeedData, 6);

	// 서스펜션 raycast prefilter. PhysXPhysicsScene 의 filterData 레이아웃을 따른다
	// (word0=ObjectType, word3=owner UUID).
	//   1) 자기 차량(같은 owner UUID)의 chassis/wheel shape 는 무시.
	//   2) WorldStatic ObjectType 만 drivable 지면으로 인정 — 동적 객체/폰 위는 지면이 아니다.
	PxQueryHitType::Enum WheelRaycastPreFilter(
		PxFilterData queryFilterData, PxFilterData objectFilterData,
		const void*, PxU32, PxHitFlags&)
	{
		if (queryFilterData.word3 != 0 && queryFilterData.word3 == objectFilterData.word3)
			return PxQueryHitType::eNONE;
		if (objectFilterData.word0 == static_cast<PxU32>(ECollisionChannel::WorldStatic))
			return PxQueryHitType::eBLOCK;
		return PxQueryHitType::eNONE;
	}

} // anonymous namespce

struct FPhysXVehicleManager::FVehicleSceneQueryData
{
	PxBatchQuery* BatchQuery = nullptr;
	TArray<PxRaycastQueryResult>           RaycastResults;   // [총 바퀴]
	TArray<PxRaycastHit>                   RaycastHits;      // [총 바퀴]
	TArray<PxWheelQueryResult>             WheelResults;     // [총 바퀴]
	TArray<PxVehicleWheelQueryResult>      VehicleResults;   // [차량 수]
	TArray<UWheeledVehicleMovementComponent*> ActiveVehicles;

	// 총 바퀴 수가 바뀌면 (재)할당. BatchQuery 는 Scene 에서 생성.
	void Ensure(PxScene* Scene, uint32 NumVehicles, uint32 TotalWheels)
	{
		// Num. Wheel comparison
		bool bWheelComp = (WheelResults.size() == TotalWheels);
		// Num. Vehicle comparison
		bool bVehicleComp = (VehicleResults.size() == NumVehicles);
		if (bWheelComp && bVehicleComp && BatchQuery) return;

		Release();

		RaycastResults.resize(TotalWheels);
		RaycastHits.resize(TotalWheels);
		WheelResults.resize(TotalWheels);
		VehicleResults.resize(NumVehicles);

		PxBatchQueryDesc Desc(TotalWheels, 0, 0);
		Desc.queryMemory.userRaycastResultBuffer = RaycastResults.data();
		Desc.queryMemory.userRaycastTouchBuffer = RaycastHits.data();
		Desc.queryMemory.raycastTouchBufferSize = TotalWheels;
		Desc.preFilterShader = WheelRaycastPreFilter;
		BatchQuery = Scene->createBatchQuery(Desc);
	}

	void Release()
	{
		if (BatchQuery) { BatchQuery->release(); BatchQuery = nullptr; }
		RaycastResults.clear(); RaycastHits.clear();
		WheelResults.clear();   VehicleResults.clear();
	}
};


// ============================================================
// FPhysXVehicleManager — scene 소유 차량 레지스트리 + 배치 업데이트.
// (memory: physx-vehicle-integration) — register/unregister 핸드셰이크 + PhysX 컨텍스트
// 허브. PreTick/Tick/PostTick 의 PhysX 배치 경로는 part 2 에서 채운다.
// ============================================================

void FPhysXVehicleManager::Init(PxPhysics* InPhysics, PxScene* InScene, PxCooking* InCooking, PxMaterial* InDriveMaterial)
{
	Physics       = InPhysics;
	Scene         = InScene;
	Cooking       = InCooking;
	DriveMaterial = InDriveMaterial;

	// 드라이브 표면 ↔ 타이어 마찰 테이블 — minimal: 표면 1종(DriveMaterial) × 타이어 1종.
	// PxVehicleUpdates 가 이 테이블로 각 타이어의 마찰을 조회한다.
	if (DriveMaterial)
	{
		const PxU32 NumTireTypes    = 1;
		const PxU32 NumSurfaceTypes = 1;

		const PxMaterial* SurfaceMaterials[NumSurfaceTypes] = { DriveMaterial };
		PxVehicleDrivableSurfaceType SurfaceTypes[NumSurfaceTypes];
		SurfaceTypes[0].mType = 0;   // 표면 타입 0 == DriveMaterial

		FrictionPairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(NumTireTypes, NumSurfaceTypes);
		FrictionPairs->setup(NumTireTypes, NumSurfaceTypes, SurfaceMaterials, SurfaceTypes);
		FrictionPairs->setTypePairFriction(/*surfaceType*/0, /*tireType*/0, /*friction*/1.0f);
	}
}

void FPhysXVehicleManager::PreTick(float DeltaTime)
{
	// 등록된 각 차량의 아날로그 입력을 PxVehicleDrive4WRawInputData 로 옮겨 smoothing → setAnalogInputs.
	// 이후 Tick 의 PxVehicleSuspensionRaycasts + PxVehicleUpdates 가 소비.
	for (size_t Idx = 0; Idx < Vehicles.size(); ++Idx)
	{
		UWheeledVehicleMovementComponent* MC = Vehicles[Idx];
		if (!MC) continue;

		PxVehicleDrive4W* Vehicle = MC->GetPxVehicle();
		if (!Vehicle) continue;

		// Arcade auto-reverse: throttle 은 [-1,1] 전/후진 통합 축.
		// PhysX accel 은 항상 '현재 기어 방향'으로만 민다 → 후진하려면 eREVERSE 로 기어를 강제해야 한다.
		// 진행 방향과 입력이 반대면 먼저 제동(brake), 거의 멈춘 뒤에 반대 기어로 전환한다.
		const float Axis         = MC->GetThrottleInput();        // +전진 / -후진
		const float ForwardSpeed = Vehicle->computeForwardSpeed(); // m/s, +면 차체 전방
		const float SpeedDeadzone = 0.5f;                          // m/s — 정지 간주 임계

		float Accel = 0.0f;
		float Brake = MC->GetBrakeInput();                         // 명시적 풋브레이크(있으면)와 합산
		PxVehicleGearsData::Enum DesiredGear = PxVehicleGearsData::eFIRST;   // 기본 전진 기어

		if (Axis > 0.01f)        // 전진 의도
		{
			if (ForwardSpeed < -SpeedDeadzone) Brake += Axis;     // 후진 중 → 제동
			else { DesiredGear = PxVehicleGearsData::eFIRST;   Accel = Axis; }
		}
		else if (Axis < -0.01f)  // 후진 의도
		{
			if (ForwardSpeed > SpeedDeadzone)  Brake += -Axis;    // 전진 중 → 제동
			else { DesiredGear = PxVehicleGearsData::eREVERSE; Accel = -Axis; }
		}

		// autobox 는 전진 기어만 올리고 reverse 로는 못 넘어간다 → reverse↔first 전환만 직접 강제.
		PxVehicleDriveDynData& Dyn = Vehicle->mDriveDynData;
		const PxU32 CurGear = Dyn.getCurrentGear();
		if (DesiredGear == PxVehicleGearsData::eREVERSE && CurGear != PxVehicleGearsData::eREVERSE)
			Dyn.forceGearChange(PxVehicleGearsData::eREVERSE);
		else if (DesiredGear != PxVehicleGearsData::eREVERSE && CurGear == PxVehicleGearsData::eREVERSE)
			Dyn.forceGearChange(PxVehicleGearsData::eFIRST);

		PxVehicleDrive4WRawInputData RawInputData;
		RawInputData.setAnalogAccel(Accel);
		RawInputData.setAnalogBrake(std::min(Brake, 1.0f));
		RawInputData.setAnalogSteer(MC->GetSteeringInput());
		RawInputData.setAnalogHandbrake(MC->GetHandbrakeInput() ? 1.0f : 0.0f);

		// in-air 여부는 직전 프레임 wheel query 결과에서 — 공중이면 steer 보정/smoothing 이 달라진다.
		// 버퍼는 다음 Tick 까지 유효. 첫 프레임엔 결과가 없어 false. (ActiveVehicles[i] ↔ VehicleResults[i])
		bool bInAir = false;
		if (SqData)
		{
			for (size_t i = 0; i < SqData->ActiveVehicles.size(); ++i)
			{
				if (SqData->ActiveVehicles[i] == MC && SqData->VehicleResults[i].wheelQueryResults)
				{
					bInAir = PxVehicleIsInAir(SqData->VehicleResults[i]);
					break;
				}
			}
		}

		PxVehicleDrive4WSmoothAnalogRawInputsAndSetAnalogInputs(
			gPadSmoothingData, gSteerVsForwardSpeedTable, RawInputData, DeltaTime, bInAir, *Vehicle);
	}
}

void FPhysXVehicleManager::Tick(float DeltaTime)
{
	if (Vehicles.empty()) return;
	PxScene* Scene = GetScene();
	if (!Scene) return;

	if (!SqData) SqData = new FVehicleSceneQueryData();
	SqData->ActiveVehicles.clear();

	// 1. 활성 차량 + 총 바퀴 수 수집 (PostTick 과 동일 순서로 정렬 보장).
	TArray<PxVehicleWheels*> PxVehicles;
	uint32 TotalWheelCount = 0;
	for (uint16 Idx = 0; Idx < Vehicles.size(); ++Idx)
	{
		UWheeledVehicleMovementComponent* MC = Vehicles[Idx];
		if (!MC) continue;
		PxVehicleDrive4W* Vehicle = MC->GetPxVehicle();
		if (!Vehicle) continue;
		PxVehicles.push_back(Vehicle);
		SqData->ActiveVehicles.push_back(MC);
		TotalWheelCount += Vehicle->mWheelsSimData.getNbWheels();
	}
	if (PxVehicles.empty()) return;

	// 2. 버퍼 (재)할당 + per-vehicle wheelQueryResults 를 WheelResults 서브레인지에 연결.
	SqData->Ensure(Scene, (uint32)PxVehicles.size(), TotalWheelCount);
	if (!SqData->BatchQuery) return;

	uint32 WheelOffset = 0;
	for (uint16 i = 0; i < PxVehicles.size(); ++i)
	{
		const uint32 nb = PxVehicles[i]->mWheelsSimData.getNbWheels();
		SqData->VehicleResults[i].nbWheelQueryResults = nb;
		SqData->VehicleResults[i].wheelQueryResults = &SqData->WheelResults[WheelOffset];
		WheelOffset += nb;
	}

	// 3. 서스펜션 레이캐스트
	PxVehicleSuspensionRaycasts(
		SqData->BatchQuery, static_cast<PxU32>(PxVehicles.size()), PxVehicles.data(),
		static_cast<PxU32>(SqData->RaycastResults.size()), SqData->RaycastResults.data());

	// 4. 차량 물리 업데이트
	const PxVec3 Gravity = Scene->getGravity();
	PxVehicleUpdates(
		DeltaTime, Gravity, *GetFrictionPairs(),
		static_cast<PxU32>(PxVehicles.size()), PxVehicles.data(),
		SqData->VehicleResults.data());
}


void FPhysXVehicleManager::Release()
{
	// FrictionPairs 만 manager 소유 — 나머지 핸들(Physics/Scene/Cooking/Material)은 Scene 소유.
	if (FrictionPairs)
	{
		FrictionPairs->release();
		FrictionPairs = nullptr;
	}
	if (SqData)
	{
		SqData->Release(); delete SqData; SqData = nullptr;
	}
	Vehicles.clear();
	Physics       = nullptr;
	Scene         = nullptr;
	Cooking       = nullptr;
	DriveMaterial = nullptr;
}

void FPhysXVehicleManager::RegisterVehicleMC(UWheeledVehicleMovementComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}
	if (std::find(Vehicles.begin(), Vehicles.end(), InComponent) != Vehicles.end())
	{
		return;   // 중복 등록 방지
	}
	Vehicles.push_back(InComponent);
}

void FPhysXVehicleManager::UnRegisterVehicleMC(UWheeledVehicleMovementComponent* InComponent)
{
	Vehicles.erase(std::remove(Vehicles.begin(), Vehicles.end(), InComponent), Vehicles.end());
}

int32 FPhysXVehicleManager::GetWheelLocalPoses(const UWheeledVehicleMovementComponent* MC, FTransform* Out, int32 Max) const
{
	if (!SqData || !MC || !Out || Max <= 0) return 0;

	// ActiveVehicles[i] ↔ VehicleResults[i] (Tick 에서 같은 순서로 구성). MC 의 결과만 복사.
	for (size_t i = 0; i < SqData->ActiveVehicles.size(); ++i)
	{
		if (SqData->ActiveVehicles[i] != MC) continue;

		const PxVehicleWheelQueryResult& VR = SqData->VehicleResults[i];
		if (!VR.wheelQueryResults) return 0;

		const int32 Count = std::min<int32>(Max, static_cast<int32>(VR.nbWheelQueryResults));
		for (int32 w = 0; w < Count; ++w)
		{
			const PxTransform& L = VR.wheelQueryResults[w].localPose;
			Out[w] = FTransform(
				FVector(L.p.x, L.p.y, L.p.z),
				FQuat(L.q.x, L.q.y, L.q.z, L.q.w),
				FVector(1.0f, 1.0f, 1.0f));
		}
		return Count;
	}
	return 0;
}
