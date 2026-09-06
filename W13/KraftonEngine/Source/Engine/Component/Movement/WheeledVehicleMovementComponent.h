#pragma once
#include "PawnMovementComponent.h"
#include "Math/Transform.h"

#include "Source/Engine/Component/Movement/WheeledVehicleMovementComponent.generated.h"

// PhysX vehicle 타입 — 엔진 헤더가 PhysX 에 직접 의존하지 않도록 forward declare 만 한다.
// 실제 PhysX 헤더는 .cpp 에서만 include (FPhysXPhysicsScene 와 동일한 캡슐화 패턴).
namespace physx
{
	class PxVehicleDrive4W;
	class PxRigidDynamic;
}

class FPhysXVehicleManager;

// 휠 1개의 setup — 스티어/핸드브레이크 적용 여부 (UE FWheelSetup 의 minimal subset).
// 휠 위치는 parametric (chassis 박스 4코너) 로 계산하므로 bone/mesh 매핑은 없다.
USTRUCT()
struct FWheelSetup
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Wheel", DisplayName="Affected By Steering")
	bool bAffectedBySteering = false;
	UPROPERTY(Edit, Save, Category="Wheel", DisplayName="Affected By Handbrake")
	bool bAffectedByHandbrake = false;
};

// ============================================================
// UWheeledVehicleMovementComponent — PxVehicleDrive4W 기반 4륜 차량 이동 컴포넌트.
// UE4 pre-Chaos UWheeledVehicleMovementComponent 대응.
//
// 역할 분담 (memory: physx-vehicle-integration):
//   - 입력      : PlayerController/Lua 가 SetThrottleInput/SetSteeringInput/... 로 매 프레임 세팅.
//   - 입력 phase: FPhysXVehicleManager::PreTick 이 이 컴포넌트의 입력을 읽어 PxVehicle 에 적용한다
//                 (Scene->simulate() 직전의 pre-sim hook).
//   - 출력 phase: 시뮬레이션 fetch 후 chassis/wheel pose 를 컴포넌트로 push 한다
//                 (AWheeledVehicle::Tick — actor 가 자기 FPhysVehicle 을 읽어 적용).
//
// PxVehicleDrive4W 는 PhysX SDK 가 소유한다 — 이 컴포넌트는 핸들만 보관하고,
// 생성/파괴 + manager register/unregister 핸드셰이크를 담당한다.
// ============================================================
UCLASS()
class UWheeledVehicleMovementComponent : public UPawnMovementComponent
{
public:
	GENERATED_BODY()

	UWheeledVehicleMovementComponent() = default;
	~UWheeledVehicleMovementComponent() override = default;

	// --- UActorComponent ---
	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	// --- 입력 (UE UWheeledVehicleMovementComponent API) ---
	// throttle 은 [-1,1] (전/후진 통합 축, +전진/-후진), brake/handbrake 는 [0,1],
	// steer 는 [-1,1] 로 clamp 되어 보관된다.
	void SetThrottleInput(float Throttle);
	void SetBrakeInput(float Brake);
	void SetSteeringInput(float Steering);
	void SetHandbrakeInput(bool bHandbrake);

	float GetThrottleInput()  const { return ThrottleInput; }
	float GetBrakeInput()     const { return BrakeInput; }
	float GetSteeringInput()  const { return SteeringInput; }
	bool  GetHandbrakeInput() const { return bHandbrakeInput; }

	float GetEngineRotationSpeed() const;   // rad/s, 0 if no vehicle
	float GetEngineRotationRatio() const;   // omega / EngineMaxOmega, [0,1]

	// 현재 전진 속도 (m/s, 차체 forward 축 투영). UI/기어 로직용. 미생성 시 0.
	float GetForwardSpeed() const;

	// --- FPhysXVehicleManager 연동 (manager 가 호출) ---
	// 배치 업데이트 대상 PhysX 차량 핸들. 미생성 시 nullptr.
	physx::PxVehicleDrive4W* GetPxVehicle() const { return PVehicle; }

	// 시뮬레이션 후 chassis 의 world transform. AWheeledVehicle::Tick 의 output readback 용
	// (chassis = PxRigidDynamic, 컴포넌트 소유). 미생성 시 false.
	bool GetChassisWorldTransform(FTransform& Out) const;

	// 이번 프레임 각 wheel 의 chassis-local pose 를 OutPoses 에 채운다 (PxVehicle wheelQueryResults.localPose,
	// vehicle actor 공간). 반환 = 채운 개수. AWheeledVehicle::Tick 이 wheel static mesh 에 적용한다.
	int32 GetWheelPoses(FTransform* OutPoses, int32 Max) const;

	static constexpr int32 NumWheels = 4;

protected:
	// PxVehicleDrive4W + 연결된 PxRigidDynamic 을 tunable 파라미터로 생성. 성공 시 PVehicle 세팅.
	bool CreateVehicle();
	// PxVehicle 해제 + 핸들 정리.
	void DestroyVehicle();

	// Scene 이 소유한 vehicle manager 를 찾아 자신을 register/unregister 한다 (핸드셰이크).
	void RegisterWithManager();
	void UnregisterFromManager();
	FPhysXVehicleManager* ResolveVehicleManager() const;

	// --- PhysX 핸들 (비-reflected) ---
	// parametric: PVehicleActor(PxRigidDynamic) 는 우리가 만들고 소유/해제한다 (source of truth).
	physx::PxVehicleDrive4W* PVehicle      = nullptr;
	physx::PxRigidDynamic*   PVehicleActor = nullptr;   // GetChassisWorldTransform 가 읽음
	FPhysXVehicleManager*    VehicleManager = nullptr;

	// --- 현재 입력 상태 (manager PreTick 이 읽음) ---
	float ThrottleInput   = 0.0f;
	float BrakeInput      = 0.0f;
	float SteeringInput   = 0.0f;
	bool  bHandbrakeInput = false;

	// per-wheel 속성 (steer/handbrake). 순서 = PxVehicleDrive4WWheelOrder (0=FL, 1=FR, 2=RL, 3=RR),
	// 정확히 NumWheels(4) 개. wheel 위치는 chassis 박스 4코너에서 parametric 으로 계산된다.
	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Wheel Setups")
	TArray<FWheelSetup> WheelSetups = {
		FWheelSetup{ true,  false },
		FWheelSetup{ true,  false },
		FWheelSetup{ false, true  },
		FWheelSetup{ false, true  },
	};

	// --- Editor-tunable setup (UE FVehicleEngineData / FWheelSetup 의 minimal subset) ---
	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Chassis Mass", Min=1.0f, Max=10000.0f, Speed=1.0f)
	float ChassisMass = 1500.0f;        // kg

	// 휠 반경/폭 — 휠 static mesh 가 지정돼 있으면 그 로컬 바운드에서 유도되고, 이 값은 fallback.
	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Wheel Radius (fallback)", Min=0.01f, Max=2.0f, Speed=0.01f)
	float WheelRadius = 0.35f;          // m

	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Wheel Width (fallback)", Min=0.01f, Max=2.0f, Speed=0.01f)
	float WheelWidth = 0.25f;           // m

	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Wheel Mass", Min=0.1f, Max=200.0f, Speed=0.1f)
	float WheelMass = 20.0f;            // kg

	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Peak Engine Torque", Min=1.0f, Max=2000.0f, Speed=1.0f)
	float EnginePeakTorque = 500.0f;    // Nm

	// The absolute maximum angular velocity the engine is allowed to reach
	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Max Engine Omega", Min=100.0f, Max=20000.0f, Speed=10.0f)
	float EngineMaxOmega = 600.0f;      // rad/s

	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Max Steer Angle (deg)", Min=0.0f, Max=90.0f, Speed=0.5f)
	float MaxSteerAngle = 30.0f;        // deg

	// 차체 박스 치수 — 차체 static mesh(= UpdatedComponent) 가 지정돼 있으면 그 로컬 바운드에서
	// 유도되고, 이 값들은 mesh 없을 때의 fallback. +X=forward, Y=side, +Z=up.
	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Chassis Length (X, fallback)", Min=0.1f, Max=20.0f, Speed=0.05f)
	float ChassisLength = 4.0f;         // m

	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Chassis Width (Y, fallback)", Min=0.1f, Max=10.0f, Speed=0.05f)
	float ChassisWidth = 2.0f;          // m

	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="Chassis Height (Z, fallback)", Min=0.1f, Max=10.0f, Speed=0.05f)
	float ChassisHeight = 1.0f;         // m

	// 무게중심 Z 오프셋 (actor 중심 기준) — 보통 음수로 낮춰 전복 안정성↑.
	UPROPERTY(Edit, Save, Category="Vehicle", DisplayName="CoM Offset Z", Min=-5.0f, Max=5.0f, Speed=0.01f)
	float CenterOfMassOffsetZ = -0.5f;  // m

	// --- Audio Property ---
	UPROPERTY(Edit, Save, Category = "Vehicle|Audio", DisplayName = "Engine Loop Key")
	FString EngineLoopKey = "CarEngineLoop";       // FAudioManager::LoadDefaultAudios

	UPROPERTY(Edit, Save, Category = "Vehicle|Audio", DisplayName = "Engine Start Key")
	FString EngineStartKey = "CarEngineStart";     // one-shot ignition

	UPROPERTY(Edit, Save, Category = "Vehicle|Audio", DisplayName = "Idle Pitch")
	float IdlePitch = 0.6f;
	UPROPERTY(Edit, Save, Category = "Vehicle|Audio", DisplayName = "Max Pitch")
	float MaxPitch = 2.0f;

	FString EngineLoopName;
};
