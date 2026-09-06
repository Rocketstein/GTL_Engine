#include "WheeledVehicle.h"

#include "Component/Primitive/StaticMeshComponent.h"
#include "Component/Movement/WheeledVehicleMovementComponent.h"
#include "Component/Input/InputComponent.h"
#include "Component/Camera/SpringArmComponent.h"
#include "Component/Camera/CameraComponent.h"
#include "Object/FName.h"
#include "Math/Transform.h"

#include <string>

void AWheeledVehicle::BeginPlay()
{
	// 컴포넌트는 component-BeginPlay(= MC::CreateVehicle, UpdatedComponent/wheel bone 필요) 전에
	// 존재해야 한다. AActor::BeginPlay 가 OwnedComponents 의 BeginPlay 를 먼저 돌리므로 여기서 선보장.
	EnsureComponents();
	Super::BeginPlay();   // → AActor (component BeginPlays) → APawn (InputComponent + SetupInputComponent)
}

void AWheeledVehicle::PostDuplicate()
{
	Super::PostDuplicate();
	// dup/load 후 멤버 포인터 재획득 (이미 컴포넌트가 존재하므로 생성은 일어나지 않음).
	EnsureComponents();
}

void AWheeledVehicle::InitDefaultComponents()
{
	// Editor 배치 시 호출 — 컴포넌트를 만들어 두면 details 패널 설정 + 씬 저장이 된다.
	// 런타임엔 BeginPlay 의 EnsureComponents 가 동일 경로로 재획득하므로 idempotent.
	EnsureComponents();
}

void AWheeledVehicle::EnsureComponents()
{
	// 1) 차체 static mesh = Root. 최초엔 생성, 이후엔 root 재획득.
	BodyMesh = Cast<UStaticMeshComponent>(GetRootComponent());
	if (!BodyMesh)
	{
		BodyMesh = AddComponent<UStaticMeshComponent>();
		BodyMesh->SetFName(FName("VehicleBody"));
		SetRootComponent(BodyMesh);
		// TODO: 차체 static mesh 지정 (editor 의 Static Mesh 프로퍼티 또는 코드). 없어도 물리는 정상 구동.
	}

	// Wheel mesh 재획득: BodyMesh 의 static-mesh 자식을 순서대로. (컴포넌트 이름은 직렬화되지
	// 않으므로 FName 매칭은 load 후 깨진다 → 순서로 매칭. load 는 Children 배열 순서,
	// PIE-dup 은 GetChildren() 순회 순서를 보존하고, SpringArm/Camera 는 타입이 달라 제외된다.)
	for (int32 i = 0; i < NumWheels; ++i) WheelMesh[i] = nullptr;

	int32 Found = 0;
	for (USceneComponent* Child : BodyMesh->GetChildren())
	{
		if (Found >= NumWheels) break;
		if (UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Child))
			WheelMesh[Found++] = SM;
	}
	// 부족분만 생성 (최초 배치 시). 이름은 outliner 가독성용 — 매칭엔 안 쓴다.
	for (int32 i = Found; i < NumWheels; ++i)
	{
		WheelMesh[i] = AddComponent<UStaticMeshComponent>();
		WheelMesh[i]->SetFName(FName(FString("Wheel_") + std::to_string(i)));
		WheelMesh[i]->AttachToComponent(BodyMesh);
	}

	// 3) Movement component.
	VehicleMC = GetComponentByClass<UWheeledVehicleMovementComponent>();
	if (!VehicleMC)
	{
		VehicleMC = AddComponent<UWheeledVehicleMovementComponent>();
	}
	VehicleMC->SetUpdatedComponent(BodyMesh);

	// 4) 3인칭 chase 카메라 — BodyMesh → SpringArm → Camera. APawn::PossessedBy 가 Camera 를 활성화.
	//    uniquely-typed 라 재획득은 GetComponentByClass 로 충분.
	SpringArm = GetComponentByClass<USpringArmComponent>();
	if (!SpringArm)
	{
		SpringArm = AddComponent<USpringArmComponent>();
		SpringArm->AttachToComponent(BodyMesh);
		SpringArm->TargetArmLength          = 8.0f;                       // m — 차체 뒤 거리
		SpringArm->SocketOffset             = FVector(0.0f, 0.0f, 3.0f);  // 차체 위로
		SpringArm->bEnableCameraLag         = true;
		SpringArm->bEnableCameraRotationLag = true;
	}
	// bUsePawnControlRotation 은 UPROPERTY 가 아니라 직렬화되지 않는다 → 로드 후 default(true)로
	// 되돌아간다(그러면 ControlRotation 을 쓰는데 차량은 갱신 안 해 카메라가 스폰 방향에 고정).
	// 매번 명시적으로 false 로 둬서 SpringArm 이 차체(BodyMesh) heading 을 따라가게 한다.
	if (SpringArm) SpringArm->bUsePawnControlRotation = false;

	Camera = GetComponentByClass<UCameraComponent>();
	if (!Camera)
	{
		Camera = AddComponent<UCameraComponent>();
		Camera->AttachToComponent(SpringArm);
	}
}

void AWheeledVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!VehicleMC || !BodyMesh) return;

	// Output readback (post-fetch): chassis world pose 를 Root(BodyMesh) 에 반영.
	// chassis actor 는 BodyMappings 밖이라 scene post-sync 가 안 건드림 — 이 Tick 이 유일 writer.
	FTransform Chassis;
	if (VehicleMC->GetChassisWorldTransform(Chassis))
	{
		BodyMesh->SetWorldLocation(Chassis.Location);
		BodyMesh->SetRelativeRotation(Chassis.Rotation);   // Root: relative == world
	}

	// Wheel pose 반영 (suspension 변위 + steer/spin) — chassis-local pose 를 각 WheelMesh 의
	// relative transform 에 적용. scale 은 에디터 값 유지를 위해 location/rotation 만 쓴다.
	// WheelAlign(보정)을 먼저 적용(로컬 재정렬) 후 wheel local pose 를 합성: row-vector 컨벤션상
	// 왼쪽 피연산자가 먼저 적용된다 (boneToWorld = boneGlobal * componentToWorld 와 동일).
	const FQuat WheelAlign = WheelMeshRotationOffset.ToQuaternion();
	FTransform WheelPoses[NumWheels];
	const int32 Count = VehicleMC->GetWheelPoses(WheelPoses, NumWheels);
	for (int32 i = 0; i < Count && i < NumWheels; ++i)
	{
		if (!WheelMesh[i]) continue;
		WheelMesh[i]->SetRelativeLocation(WheelPoses[i].Location);
		WheelMesh[i]->SetRelativeRotation(WheelAlign * WheelPoses[i].Rotation);
	}
}

void AWheeledVehicle::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent || !VehicleMC) return;

	// 키보드 매핑 — W=accel, S=brake, A/D=steer, Space=handbrake.
	InputComponent->AddAxisMapping("Throttle", 'W',  1.0f);
	InputComponent->AddAxisMapping("Throttle", 'S', -1.0f);
	InputComponent->AddAxisMapping("Steer",    'D',  1.0f);
	InputComponent->AddAxisMapping("Steer",    'A', -1.0f);
	InputComponent->AddAxisMapping("Brake",    0xA0, 1.0f);
	InputComponent->AddActionMapping("Handbrake", 0x20);   // VK_SPACE

	// BindAxis 는 매 frame 합산값(0 포함)으로 호출 → 키를 떼면 자동으로 0 이 전달된다.
	InputComponent->BindAxis("Throttle", [this](float V) { if (VehicleMC) VehicleMC->SetThrottleInput(V); });
	InputComponent->BindAxis("Brake",    [this](float V) { if (VehicleMC) VehicleMC->SetBrakeInput(V); });
	InputComponent->BindAxis("Steer",    [this](float V) { if (VehicleMC) VehicleMC->SetSteeringInput(V); });

	InputComponent->BindAction("Handbrake", EInputEvent::Pressed,  [this]() { if (VehicleMC) VehicleMC->SetHandbrakeInput(true); });
	InputComponent->BindAction("Handbrake", EInputEvent::Released, [this]() { if (VehicleMC) VehicleMC->SetHandbrakeInput(false); });
}
