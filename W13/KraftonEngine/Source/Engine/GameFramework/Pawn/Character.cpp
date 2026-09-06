#include "GameFramework/Pawn/Character.h"

#include "Component/Shape/CapsuleComponent.h"
#include "Component/Input/InputComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/World.h"
#include "Input/InputSystem.h"
#include "Math/Rotator.h"
#include "Mesh/MeshManager.h"
#include "Runtime/Engine.h"
#include "Core/Logging/Log.h"

#include <algorithm>
void ACharacter::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	// 1) Capsule — Root. CharacterMovement 의 UpdatedComponent 가 이걸 가리킴.
	CapsuleComponent = AddComponent<UCapsuleComponent>();
	SetRootComponent(CapsuleComponent);

	// 캡슐은 코드(CMC)가 움직이는 콜라이더 — kinematic 으로 등록되게 한다(static 으로 등록되어
	// 매 프레임 teleport 되면 다이내믹/랙돌을 제대로 못 밀어냄). ObjectType 은 Pawn — 다른
	// 캐릭터의 바닥 raycast(WorldStatic 마스크)가 이 캡슐을 바닥으로 오인하지 않게 한다.
	// CollisionEnabled 는 기본 NoCollision 유지 — QueryAndPhysics 로 켜면 kinematic 콜라이더로
	// 동작(랙돌을 밀어내고 sweep 대상이 됨). 켜는 건 씬/사용자 선택.
	CapsuleComponent->SetKinematic(true);
	CapsuleComponent->SetCollisionObjectType(ECollisionChannel::Pawn);

	// 2) SkeletalMesh — Capsule 의 자식.
	Mesh = AddComponent<USkeletalMeshComponent>();
	Mesh->AttachToComponent(CapsuleComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!SkeletalMeshFileName.empty())
	{
		USkeletalMesh* Asset = FMeshManager::LoadSkeletalMesh(SkeletalMeshFileName, Device);
		Mesh->SetSkeletalMesh(Asset);
	}

	// 3) CharacterMovement — non-scene. UpdatedComponent = Capsule.
	CharacterMovement = AddComponent<UCharacterMovementComponent>();
	CharacterMovement->SetUpdatedComponent(CapsuleComponent);
}

void ACharacter::PostDuplicate()
{
	Super::PostDuplicate();
	// 컴포넌트 트리 재발견 — Duplicate 후 멤버 포인터 복원.
	CapsuleComponent  = Cast<UCapsuleComponent>(GetRootComponent());
	Mesh              = GetComponentByClass<USkeletalMeshComponent>();
	CharacterMovement = GetComponentByClass<UCharacterMovementComponent>();
}

void ACharacter::AddMovementInput(const FVector& WorldDirection, float ScaleValue)
{
	if (CharacterMovement)
	{
		CharacterMovement->AddInputVector(WorldDirection, ScaleValue);
	}
}

void ACharacter::Jump()
{
	if (CharacterMovement)
	{
		CharacterMovement->Jump();
	}
}

void ACharacter::SetupInputComponent()
{
	Super::SetupInputComponent();

	// [1회용 데모] F/G/H = 태그별 타깃에 부위별 래그돌 히트. bAutoInputWASD 와 무관하게 등록.
	//   F → Target1 : 전신 래그돌, Head 에 force
	//   G → Target2 : 전신 래그돌, R Thigh 에 force
	//   H → Target3 : R Clavicle 서브트리만 부분 래그돌(팔만), R Hand 에 force
	if (InputComponent)
	{
		InputComponent->AddActionMapping("HitTarget1", 'F');
		InputComponent->BindAction("HitTarget1", EInputEvent::Pressed, [this]()
		{
			HitTaggedTarget(FName("Target1"), FName(), FName("Bip001 Head"), 20000.0f);
		});

		InputComponent->AddActionMapping("HitTarget2", 'G');
		InputComponent->BindAction("HitTarget2", EInputEvent::Pressed, [this]()
		{
			HitTaggedTarget(FName("Target2"), FName(), FName("Bip001 R Thigh"), 20000.0f);
		});

		InputComponent->AddActionMapping("HitTarget3", 'H');
		InputComponent->BindAction("HitTarget3", EInputEvent::Pressed, [this]()
		{
			HitTaggedTarget(FName("Target3"), FName("Bip001 R Clavicle"), FName("Bip001 R Hand"), 20000.0f);
		});
	}

	if (!bAutoInputWASD || !InputComponent) return;

	// Capsule (RootComponent) 기준 — yaw 회전이 곧 캐릭터 facing. mouse look 이 yaw 만
	// 변경 → forward/right vector 가 자동 회전 → WASD 가 "카메라 보는 방향" 으로 이동.
	InputComponent->AddAxisMapping("MoveForward", 'W',  1.0f);
	InputComponent->AddAxisMapping("MoveForward", 'S', -1.0f);
	InputComponent->AddAxisMapping("MoveRight",   'D',  1.0f);
	InputComponent->AddAxisMapping("MoveRight",   'A', -1.0f);

	// WASD 의 forward/right 는 ControlRotation.Yaw 기준 — capsule rotation 과 무관.
	// "카메라가 보는 방향" (yaw 만, pitch 무시) 으로 이동.
	InputComponent->BindAxis("MoveForward", [this](float Value)
	{
		if (Value == 0.0f) return;
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(YawOnly.GetForwardVector(), Value);
	});
	InputComponent->BindAxis("MoveRight", [this](float Value)
	{
		if (Value == 0.0f) return;
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(YawOnly.GetRightVector(), Value);
	});

	// Space = Jump (VK_SPACE = 0x20). Walking 중에만 effective (CharacterMovement::Jump 가 guard).
	InputComponent->AddActionMapping("Jump", 0x20);
	InputComponent->BindAction("Jump", EInputEvent::Pressed, [this]()
	{
		Jump();
	});
}

void ACharacter::HitTaggedTarget(const FName& Tag, const FName& PartialBone, const FName& ForceBone, float Strength)
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || Actor == this) continue;
		if (!Actor->HasTag(Tag)) continue;

		USkeletalMeshComponent* Mesh = Actor->GetComponentByClass<USkeletalMeshComponent>();
		if (!Mesh) continue;

		// 1) 래그돌 on. PartialBone 유효 → 그 본 서브트리만 부분 래그돌, 아니면 전신.
		//    (둘 다 UpdateBodySimulationState 로 대상 바디를 즉시 dynamic 화 → 같은 프레임 force 적용.)
		if (PartialBone.IsValid())
		{
			Mesh->SetBodyPhysicsBlendWeight(PartialBone, 1.0f, /*bIncludeChildren=*/true, /*bInterpolate=*/false);
		}
		else
		{
			Mesh->SetSimulatePhysics(true);
		}

		// 2) 밀어낼 방향: 나 → 타깃(수평) + 약간 위. 거리 0 이면 내 forward.
		FVector Dir = Actor->GetActorLocation() - GetActorLocation();
		Dir.Z = 0.0f;
		const float Len = Dir.Length();
		Dir = (Len > 1e-4f) ? (Dir * (1.0f / Len)) : GetActorRotation().GetForwardVector();
		const FVector Force = (Dir + FVector(0.0f, 0.0f, 0.35f)) * Strength;

		// 3) 지정 본에 force.
		Mesh->AddForceToBone(ForceBone, Force);

		UE_LOG("[Hit] '%s' bone=%s force=%.0f", Actor->GetName().c_str(), ForceBone.ToString().c_str(), Strength);
		return;   // 첫 타깃만 처리
	}
}

void ACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bAutoInputMouseLook)
	{
		const InputSystem& In = InputSystem::Get();
		const int DX = In.MouseDeltaX();
		const int DY = In.MouseDeltaY();
		if (DX != 0 || DY != 0)
		{
			// APawn::ControlRotation 누적. SpringArm 이 bUsePawnControlRotation 통해 이걸 사용.
			// capsule 회전은 옵션 (bUseControllerRotationYaw 등) — 아래 ApplyControllerRotationToRoot 가 처리.
			FRotator Rot = GetControlRotation();
			Rot.Yaw   += static_cast<float>(DX) * MouseSensitivity;
			Rot.Pitch += static_cast<float>(DY) * MouseSensitivity;
			Rot.Pitch  = std::clamp(Rot.Pitch, MinCameraPitch, MaxCameraPitch);
			SetControlRotation(Rot);
		}
	}

	// 같은 frame 안 ControlRotation 변경을 capsule (RootComponent) 에 즉시 반영 — 1 frame 지연 없음.
	// 옵션 충돌 가드:
	//   1) bOrientRotationToMovement = true → yaw 는 Movement::PhysOrientToMovement 가 처리.
	//   2) 직전 frame 에 root motion 이 yaw 를 적용했다 → 이번 frame 도 root motion 이 yaw 를
	//      이어받을 가능성이 큼. Character 가 control yaw 로 덮으면 root motion 회전이 즉시
	//      뒤집혀 토글링 됨 (turn-in-place / strafe anim 의 시각 손상). Movement 측에 양보.
	// 두 경우 모두 pitch/roll 만 apply, yaw 는 movement 에 양보.
	if (CapsuleComponent)
	{
		const bool bMovementHandlesYaw = CharacterMovement &&
			(CharacterMovement->bOrientRotationToMovement ||
			 CharacterMovement->HasYawDrivenByRootMotion());

		FRotator R = CapsuleComponent->GetRelativeRotation();
		bool bChanged = false;
		if (bUseControllerRotationYaw && !bMovementHandlesYaw)
		{
			R.Yaw   = ControlRotation.Yaw;
			bChanged = true;
		}
		if (bUseControllerRotationPitch)
		{
			R.Pitch = ControlRotation.Pitch;
			bChanged = true;
		}
		if (bUseControllerRotationRoll)
		{
			R.Roll  = ControlRotation.Roll;
			bChanged = true;
		}
		if (bChanged) CapsuleComponent->SetRelativeRotation(R);
	}
}
