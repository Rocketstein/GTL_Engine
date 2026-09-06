#include "GameFramework/Pawn/LuaCharacter.h"

#include "Component/Camera/CameraComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Script/LuaScriptComponent.h"
#include "Component/Camera/SpringArmComponent.h"
#include "Component/Input/InputComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/World.h"
#include "Core/Logging/Log.h"
void ALuaCharacter::InitDefaultComponents(const FString& SkeletalMeshFileName, const FString& ScriptFile)
{
	Super::InitDefaultComponents(SkeletalMeshFileName);

	// Third-person camera rotation is consumed by the spring arm, not the capsule.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 3인칭 카메라 체인 — Capsule → SpringArm → Camera. lag 적용해 부드럽게 따라옴.
	SpringArm = AddComponent<USpringArmComponent>();
	SpringArm->AttachToComponent(CapsuleComponent);
	SpringArm->TargetArmLength       = 10.0f;
	SpringArm->SocketOffset          = FVector(0.0f, 0.0f, 3.0f);
	SpringArm->bEnableCameraLag      = true;
	SpringArm->bEnableCameraRotationLag = true;

	// mouse look 이 capsule rotation 안 건드리고 카메라만 회전 — UE ThirdPerson 패턴.
	// ACharacter::Tick 이 APawn::ControlRotation 누적 → SpringArm 이 이걸 inherit.
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bInheritPitch           = true;
	SpringArm->bInheritYaw             = true;
	SpringArm->bInheritRoll            = false;

	Camera = AddComponent<UCameraComponent>();
	Camera->AttachToComponent(SpringArm);

	LuaScriptComponent = AddComponent<ULuaScriptComponent>();
	if (!ScriptFile.empty())
	{
		LuaScriptComponent->SetScriptFile(ScriptFile);
	}
}

void ALuaCharacter::SetupInputComponent()
{
	// WASD/Jump 등 기본 바인딩 먼저 등록.
	Super::SetupInputComponent();
	if (!InputComponent) return;

	// [임시 디버그] R 키 = 전신 래그돌 토글. 켜면 물리 시뮬레이션, 끄면 키네마틱으로
	// 복귀해 다음 틱부터 anim 포즈를 다시 추종한다. ('R' == VK code)
	InputComponent->AddActionMapping("ToggleRagdoll", 'R');
	InputComponent->BindAction("ToggleRagdoll", EInputEvent::Pressed, [this]()
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetSimulatePhysics(!MeshComp->IsSimulatingPhysics());
		}
	});

	// [임시 디버그] T 키 = 부분 래그돌 토글. 오른팔(UpperArm 이하)만 물리로 풀고
	// 나머지 몸통/다리는 anim 을 계속 재생한다(히트 리액션 데모). 캡처한 bOn 으로 토글.
	InputComponent->AddActionMapping("TogglePartialRagdoll", 'T');
	InputComponent->BindAction("TogglePartialRagdoll", EInputEvent::Pressed, [this, bOn = false]() mutable
	{
		bOn = !bOn;
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetBodyPhysicsBlendWeight("Bip001 R UpperArm", bOn ? 1.0f : 0.0f, true);
		}
	});

	// [임시 디버그/측정] Y 키 = 월드의 모든 SkeletalMesh 를 동시에 전신 랙돌 토글.
	// Physics.FetchBlock 부하를 키워 오버랩 실익을 측정하기 위한 예제 — 한 번에 N 개를
	// 다이내믹으로 전환한다. (각 액터의 첫 SkeletalMeshComponent 대상.)
	InputComponent->AddActionMapping("ToggleMassRagdoll", 'Y');
	InputComponent->BindAction("ToggleMassRagdoll", EInputEvent::Pressed, [this, bOn = false]() mutable
	{
		bOn = !bOn;
		UWorld* World = GetWorld();
		if (!World) return;

		int32 Count = 0;
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor) continue;
			if (USkeletalMeshComponent* SkelComp = Actor->GetComponentByClass<USkeletalMeshComponent>())
			{
				SkelComp->SetSimulatePhysics(bOn);   // 전체 바디 weight 0/1 보간 전환
				++Count;
			}
		}
		UE_LOG("[MassRagdoll] %s - %d skeletal meshes", bOn ? "ON" : "OFF", Count);
	});
}

void ALuaCharacter::PostDuplicate()
{
	Super::PostDuplicate();
	LuaScriptComponent = GetComponentByClass<ULuaScriptComponent>();
	SpringArm          = GetComponentByClass<USpringArmComponent>();
	Camera             = GetComponentByClass<UCameraComponent>();
}
