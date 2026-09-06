#include "CharacterMovementComponent.h"

#include "Animation/AnimInstance.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/SceneComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Types/PropertyTypes.h"
#include "Core/TickFunction.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/World.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Physics/IPhysicsScene.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>

UCharacterMovementComponent::UCharacterMovementComponent()
{
	// 순서 보장(group 단위): anim(PrePhysics) → simulate → CMC(PostPhysics).
	//  - PrePhysics: USkeletalMeshComponent 가 UpdateAnimation 으로 PendingRootMotion 을 채움.
	//  - CMC 는 그 뒤에 루트모션을 소비하고, 스윕/바닥 레이캐스트로 capsule 을 이동시킨다.
	// CMC 는 PhysX scene query(스윕/레이캐스트)와 body 이동을 하므로 simulate 진행 중인
	// TG_DuringPhysics 에 두면 안 된다(PxActor 접근 금지 구간). fetchResults 이후인
	// TG_PostPhysics 에 둬서 최신 물리 상태로 안전하게 쿼리한다.
	PrimaryComponentTick.SetTickGroup(TG_PostPhysics);
	PrimaryComponentTick.SetEndTickGroup(TG_PostPhysics);
}

void UCharacterMovementComponent::AddInputVector(const FVector& WorldDirection, float ScaleValue)
{
	AccumulatedInput = AccumulatedInput + WorldDirection * ScaleValue;
}

void UCharacterMovementComponent::ConsumeInputVector(FVector& Out)
{
	Out = AccumulatedInput;
	AccumulatedInput = FVector(0.0f, 0.0f, 0.0f);
}

void UCharacterMovementComponent::AddRootMotionDelta(const FTransform& LocalDelta)
{
	if (!bHasPendingRootMotion)
	{
		PendingRootMotion = LocalDelta;
		bHasPendingRootMotion = true;
		return;
	}

	// 누적 합성 — AnimInstance::AccumulateRootMotion 과 동일한 매트릭스 곱 패턴.
	// 같은 frame 에 base + montage 처럼 여러 소스가 push 할 수 있어 합성 보장 필요.
	const FMatrix M = LocalDelta.ToMatrix() * PendingRootMotion.ToMatrix();
	PendingRootMotion.Location = FVector(M.M[3][0], M.M[3][1], M.M[3][2]);
	PendingRootMotion.Rotation = (LocalDelta.Rotation * PendingRootMotion.Rotation).GetNormalized();
	// Scale 은 root motion 에서 보통 1 — 무시.
}

bool UCharacterMovementComponent::ConsumePendingRootMotion(FTransform& OutLocalDelta)
{
	if (!bHasPendingRootMotion)
	{
		OutLocalDelta = FTransform();   // Identity
		return false;
	}
	OutLocalDelta = PendingRootMotion;
	PendingRootMotion = FTransform();
	bHasPendingRootMotion = false;
	return true;
}

void UCharacterMovementComponent::SetMovementMode(EMovementMode NewMode)
{
	if (MovementMode == NewMode) return;
	MovementMode = NewMode;
	// 추후 OnMovementModeChanged delegate 위치.
}

void UCharacterMovementComponent::Jump()
{
	// Walking 중에만 점프 허용 — 공중 다단 점프 막음. (필요 시 자식 override.)
	if (MovementMode != EMovementMode::Walking) return;
	bWantsJump = true;
}

void UCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;
	if (DeltaTime <= 0.0f) return;

	// 매 Tick 회전 적용 상태 reset — 이번 frame 에 root motion 이 yaw 를 적용했는지를
	// 외부 (Character::Tick) 가 query 할 수 있어야 yaw 충돌 회피 가능.
	bAppliedRootMotionYawThisFrame = false;

	FVector Input;
	ConsumeInputVector(Input);
	Input.Z = 0.0f;   // XY 평면만 — Z 는 mode 가 결정.

	// 1) Input 처리 — XY velocity 갱신 (양 mode 공통).
	ApplyInputToVelocity(Input, DeltaTime);

	// 1.5) Owner Character 의 Mesh AnimInstance 가 누적해둔 root motion 을 가져와 자기 buffer 로 push.
	//      Mesh tick (TG_PrePhysics) 이 이미 끝나 PendingRootMotion 이 채워진 상태.
	//      Mode 가 Ignore 면 가져갈 필요 자체가 없음 (AccumulateRootMotion 측에서 누적도 안 됨).
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
		{
			if (UAnimInstance* AI = Mesh->GetAnimInstance())
			{
				if (AI->GetRootMotionMode() != ERootMotionMode::IgnoreRootMotion)
				{
					AddRootMotionDelta(AI->ConsumeRootMotion());
				}
			}
		}
	}

	// 2) Root motion 소비 — local delta 를 world frame 으로 변환 (Updated 의 yaw 기준).
	//    XY 만 mode 분기로 위임. Z 는 두 mode 모두 무시:
	//      Walking — floor stick 이 Z 결정
	//      Falling — gravity 가 Z 결정
	//    Climbing/Swimming 같은 mode 추가 시 그때 재검토.
	FTransform RootMotionDelta;
	const bool bHadRootMotion = ConsumePendingRootMotion(RootMotionDelta);
	FVector RootMotionWorldXY(0.0f, 0.0f, 0.0f);
	if (bHadRootMotion)
	{
		const FRotator ActorRot = Updated->GetWorldRotation();
		const FQuat    YawOnly  = FRotator(0.0f, 0.0f, ActorRot.Yaw).ToQuaternion();
		const FVector  World    = YawOnly.RotateVector(RootMotionDelta.Location);
		RootMotionWorldXY.X     = World.X;
		RootMotionWorldXY.Y     = World.Y;
	}

	// 3) Mode 별 Z 처리 + 위치 적용 (input velocity + root motion XY 합산).
	if (MovementMode == EMovementMode::Walking)
	{
		TickWalking(DeltaTime, RootMotionWorldXY);
	}
	else
	{
		TickFalling(DeltaTime, RootMotionWorldXY);
	}

	// 4) Root motion yaw 적용. yaw 만 추출 — root motion 의 pitch/roll 은 캐릭터 capsule
	//    회전에 일반적으로 의미 없음 (UE 도 yaw 만 적용).
	//    yaw 가 적용되면 bAppliedRootMotionYawThisFrame 을 켜서 PhysOrientToMovement /
	//    Character 의 control yaw 덮어쓰기 둘 다 같은 frame skip 되도록 한다.
	if (bHadRootMotion)
	{
		const FRotator DeltaRot = RootMotionDelta.Rotation.ToRotator();
		if (std::fabs(DeltaRot.Yaw) > 1e-4f)
		{
			FRotator R = Updated->GetRelativeRotation();
			R.Yaw += DeltaRot.Yaw;
			Updated->SetRelativeRotation(R);
			bAppliedRootMotionYawThisFrame = true;
		}
	}

	// 5) Orient yaw to movement direction. Root motion 이 yaw 를 잡고 있는 frame 은 skip —
	//    그렇지 않으면 PhysOrient 가 root motion 회전을 Velocity 방향으로 다시 lerp 해
	//    의도된 회전이 무효화된다 (turn-in-place anim 가장 큰 피해).
	if (bOrientRotationToMovement && !bAppliedRootMotionYawThisFrame)
	{
		PhysOrientToMovement(DeltaTime);
	}
}

void UCharacterMovementComponent::PhysOrientToMovement(float DeltaTime)
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;

	// 평면 속도 작으면 회전 skip — 마지막 facing 유지.
	const float SpeedSq2D = Velocity.X * Velocity.X + Velocity.Y * Velocity.Y;
	constexpr float MinSpeedSq = 1e-4f;
	if (SpeedSq2D < MinSpeedSq) return;

	// Target yaw — Velocity 방향. UE 의 atan2(Y, X) 는 +X 가 0°, +Y 가 90° (좌표계 가정).
	const float TargetYaw = std::atan2(Velocity.Y, Velocity.X) * (180.0f / 3.14159265f);

	FRotator R = Updated->GetRelativeRotation();
	const float CurrentYaw = R.Yaw;

	// 최단 회전 방향 (delta ∈ [-180, 180])
	float Delta = TargetYaw - CurrentYaw;
	while (Delta >  180.0f) Delta -= 360.0f;
	while (Delta < -180.0f) Delta += 360.0f;

	const float Step = RotationYawRate * DeltaTime;
	if (std::fabs(Delta) <= Step)
	{
		R.Yaw = TargetYaw;
	}
	else
	{
		R.Yaw = CurrentYaw + (Delta > 0.0f ? Step : -Step);
	}
	Updated->SetRelativeRotation(R);
}

void UCharacterMovementComponent::ApplyInputToVelocity(const FVector& Input, float DeltaTime)
{
	const float InputLen = Input.Length();
	if (InputLen > 0.0f)
	{
		// 입력 방향으로 가속 (XY 만).
		const FVector Direction = Input * (1.0f / InputLen);
		Velocity.X += Direction.X * MaxAcceleration * DeltaTime;
		Velocity.Y += Direction.Y * MaxAcceleration * DeltaTime;
	}
	else if (MovementMode == EMovementMode::Walking)
	{
		// Walking 에선 input 없으면 braking. Falling 중 air control 없음 = 평면 속도 유지.
		FVector V2D(Velocity.X, Velocity.Y, 0.0f);
		const float Speed2D = V2D.Length();
		if (Speed2D > 0.0f)
		{
			const float NewSpeed = std::max(0.0f, Speed2D - BrakingFriction * DeltaTime);
			const FVector Dir    = V2D * (1.0f / Speed2D);
			Velocity.X = Dir.X * NewSpeed;
			Velocity.Y = Dir.Y * NewSpeed;
		}
	}

	// MaxWalkSpeed 클램프 (평면 속도만).
	FVector V2D(Velocity.X, Velocity.Y, 0.0f);
	const float Speed2D = V2D.Length();
	if (Speed2D > MaxWalkSpeed)
	{
		const FVector Dir = V2D * (1.0f / Speed2D);
		Velocity.X = Dir.X * MaxWalkSpeed;
		Velocity.Y = Dir.Y * MaxWalkSpeed;
	}
}

void UCharacterMovementComponent::TickWalking(float DeltaTime, const FVector& RootMotionWorldXY)
{
	USceneComponent* Updated = GetUpdatedComponent();

	// Jump 의도가 있으면 — Velocity.Z 박고 즉시 Falling 으로 전환. 이 frame 의 XY 는 그대로 진행.
	if (bWantsJump)
	{
		bWantsJump = false;
		Velocity.Z = JumpZVelocity;
		SetMovementMode(EMovementMode::Falling);
		// XY 이동은 Falling 분기로 위임 — 한 frame 안 mode 전환이라 즉시 falling tick.
		TickFalling(DeltaTime, RootMotionWorldXY);
		return;
	}

	// Walking 중 Z velocity 는 0 — floor stick 으로만 Z 결정.
	Velocity.Z = 0.0f;

	// XY 이동: input velocity * dt + root motion XY (이미 world frame).
	// sweep 으로 벽/장애물을 감지해 표면을 따라 미끄러진다(collide-and-slide). Z=0 평면 이동이라
	// 바닥(거의 수평면)은 보통 안 잡히고, 잡혀도 floor stick 이 아래 Z 를 재확정한다.
	const FVector XYOffset(
		Velocity.X * DeltaTime + RootMotionWorldXY.X,
		Velocity.Y * DeltaTime + RootMotionWorldXY.Y,
		0.0f);
	SweepCapsuleMove(XYOffset, /*bLiftOffGround=*/true);

	// 이동 직후 위치에서 캡슐 footprint 아래 바닥을 sweep 으로 찾는다.
	//  - 모서리: 캡슐 일부라도 바닥 위면 잡혀 Falling 으로 안 빠진다(중심 점 레이캐스트의 빗나감 해소).
	//  - 경사: 면 법선으로 walkable 판정 — 너무 가파르면 못 서고 Falling 으로 미끄러진다.
	FFloorResult Floor;
	if (!FindFloor(Updated->GetWorldLocation(), Floor) || !Floor.bWalkable)
	{
		// 발 아래 walkable floor 없음 (절벽 끝 / 너무 가파른 면) → falling.
		SetMovementMode(EMovementMode::Falling);
		return;
	}

	// Floor stick — sweep 으로 잰 실제 간격만큼만 보정(경사에서 파묻거나 띄우지 않음).
	SnapToFloor(Floor);
}

void UCharacterMovementComponent::TickFalling(float DeltaTime, const FVector& RootMotionWorldXY)
{
	USceneComponent* Updated = GetUpdatedComponent();

	// Gravity — Z 만. (양수 Gravity → -Z 가속)
	Velocity.Z -= Gravity * DeltaTime;

	// Velocity * dt 의 XY 에 root motion XY 합산. Z 는 gravity 가 책임이라 root motion 무시.
	// 낙하 중에도 sweep 으로 벽/천장/장애물을 통과하지 않고 미끄러진다. 아래 floor 체크가
	// 착지(Z 보정 + Walking 전환)를 별도로 처리하므로 여기선 벽 막힘만 담당.
	const FVector Offset(
		Velocity.X * DeltaTime + RootMotionWorldXY.X,
		Velocity.Y * DeltaTime + RootMotionWorldXY.Y,
		Velocity.Z * DeltaTime);
	SweepCapsuleMove(Offset, /*bLiftOffGround=*/false);

	// 올라가는 중 (점프 arc 상승) 엔 floor 체크 skip — 안 그러면 점프 직후 1 frame 의
	// 작은 상승 (≈ JumpZVelocity * dt) 이 raycast probe 거리 안에 있어 즉시 착지로 잡힘.
	// UE 도 동일 — Velocity.Z > 0 이면 ground 안 잡음.
	if (Velocity.Z > 0.0f) return;

	// 떨어지는 중에만 floor 체크. walkable 면일 때만 착지 — 가파른 면은 착지하지 않고 계속
	// 미끄러진다(위 sweep 이 면을 따라 downhill 로 흘려보냄).
	FFloorResult Floor;
	if (!FindFloor(Updated->GetWorldLocation(), Floor) || !Floor.bWalkable) return;

	// 착지 — sweep 으로 잰 간격만큼 캡슐을 면에 붙이고 Walking 전환 + Velocity.Z = 0.
	SnapToFloor(Floor);
	Velocity.Z = 0.0f;
	SetMovementMode(EMovementMode::Walking);
}

float UCharacterMovementComponent::GetWalkableFloorZ() const
{
	// 경사각 → 법선 Z 임계값. 0°=1.0(평지만), 90°=0.0(벽까지). 안전 클램프.
	const float Deg = std::clamp(WalkableFloorAngle, 0.0f, 90.0f);
	return std::cos(Deg * (3.14159265f / 180.0f));
}

bool UCharacterMovementComponent::FindFloor(const FVector& CapsuleCenter, FFloorResult& OutFloor) const
{
	OutFloor = FFloorResult();

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return false;
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || !World->GetPhysicsScene()) return false;

	const float Radius     = GetCapsuleRadius();
	const float HalfHeight = GetCapsuleHalfHeight();
	if (Radius <= 0.0f || HalfHeight <= 0.0f) return false;   // capsule 아니면 floor 의미 없음

	// 캡슐을 그대로 아래로 sweep — footprint 전체 아래에서 면을 찾는다(모서리에서도 일부가 바닥
	// 위면 잡힘). 하향 sweep 이라 옆의 수직 벽은 닿지 않고(거리 유지), 면 법선으로 walkable 판정.
	// 바닥 후보는 WorldStatic ObjectType 만 — 동적/폰을 바닥으로 오인하지 않도록.
	constexpr float Skin = 0.01f;
	const float     SweepDist = std::max(FloorProbeDistance, 0.0f) + Skin;
	const FVector   Down(0.0f, 0.0f, -1.0f);
	const FQuat     Rot = Updated->GetWorldRotation().ToQuaternion();
	constexpr uint32 FloorMask = ObjectTypeBit(ECollisionChannel::WorldStatic);

	FHitResult Hit;
	if (!World->PhysicsSweepCapsuleByObjectTypes(CapsuleCenter, Rot, Radius, HalfHeight, Down, SweepDist, Hit, FloorMask, Owner))
	{
		return false;   // 캡슐 아래 SweepDist 안에 면 없음 → 바닥 없음(절벽/공중).
	}

	OutFloor.bBlockingHit = true;
	OutFloor.Normal       = Hit.ImpactNormal;
	OutFloor.Location     = Hit.WorldHitLocation;
	OutFloor.bWalkable    = (Hit.ImpactNormal.Z >= GetWalkableFloorZ());
	// 초기 침투(박힘)면 음수 거리로 표기 → SnapToFloor 가 위로 밀어낸다.
	OutFloor.FloorDist    = (Hit.PenetrationDepth > 0.0f) ? -Hit.PenetrationDepth : Hit.Distance;
	return true;
}

void UCharacterMovementComponent::SnapToFloor(const FFloorResult& Floor)
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated || !Floor.bBlockingHit) return;

	constexpr float Skin = 0.01f;
	FVector Loc = Updated->GetWorldLocation();

	if (Floor.FloorDist > Skin)
	{
		// 면에서 떠 있음 — Skin 만 남기고 내려 붙인다(경사에선 sweep 이 잰 실제 간격이라 파묻지 않음).
		Loc.Z -= (Floor.FloorDist - Skin);
	}
	else if (Floor.FloorDist < 0.0f)
	{
		// 박힘 — 법선 Z 성분을 고려해 수직으로 밀어올린다. 한 번에 과하게 튀지 않게 반경으로 클램프.
		const float NormalZ = std::max(Floor.Normal.Z, 0.5f);
		const float PushZ   = (-Floor.FloorDist) / NormalZ + Skin;
		Loc.Z += std::min(PushZ, GetCapsuleRadius());
	}
	// |FloorDist| ≤ Skin 이면 접촉 상태 — 보정 안 함(미세 진동 방지).

	Updated->SetWorldLocation(Loc);
}

float UCharacterMovementComponent::GetCapsuleHalfHeight() const
{
	// UpdatedComponent 가 capsule 이라야 의미 있음 — 다른 shape 면 0.
	if (UCapsuleComponent* Cap = Cast<UCapsuleComponent>(GetUpdatedComponent()))
	{
		return Cap->GetScaledCapsuleHalfHeight();
	}
	return 0.0f;
}

float UCharacterMovementComponent::GetCapsuleRadius() const
{
	if (UCapsuleComponent* Cap = Cast<UCapsuleComponent>(GetUpdatedComponent()))
	{
		return Cap->GetScaledCapsuleRadius();
	}
	return 0.0f;
}

bool UCharacterMovementComponent::SweepCapsuleMove(const FVector& Delta, bool bLiftOffGround)
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return false;

	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	const float Radius     = GetCapsuleRadius();
	const float HalfHeight = GetCapsuleHalfHeight();

	const FVector Start = Updated->GetWorldLocation();

	// 물리 씬/캡슐이 없거나 wall collision off → 기존 minimal 동작(직접 이동) fallback.
	if (!bUseWallCollision || !World || !World->GetPhysicsScene() || Radius <= 0.0f || HalfHeight <= 0.0f)
	{
		Updated->SetWorldLocation(Start + Delta);
		return false;
	}

	const float TotalDist = Delta.Length();
	if (TotalDist <= 1e-5f) return false;

	// 벽 후보: 정적 월드(WorldStatic)만. 동적(WorldDynamic=랙돌 등)은 제외 — 캐릭터가 바닥에
	// 가라앉아 박힌 동적 바디에 끌려다니는 사고를 막고, 목표(맵의 벽 충돌)에 집중한다.
	// 동적 장애물 충돌이 필요해지면 별도로 신중히 추가.
	constexpr uint32 WallMask = ObjectTypeBit(ECollisionChannel::WorldStatic);
	// 표면에서 살짝 떨어뜨려(skin) 다음 sweep 이 접촉 상태에서 시작해 0 거리로 끼는 걸 방지.
	constexpr float ContactOffset = 0.01f;
	constexpr int   MaxSlides     = 4;
	// 초기 침투 시 한 프레임에 밀어내는 최대량(작게) — 깊이 박혀도 부드럽게 빠져나오게 해
	// 수평 폭주/드래그를 막는다. 박힘은 여러 프레임에 걸쳐 서서히 해소.
	const float MaxDepenStep = std::max(2.0f * ContactOffset, Radius * 0.05f);

	// 걷기 수평 이동에서 "발밑 바닥(서 있는 면)"이 벽처럼 잡혀 캐릭터가 끼거나 튕기는 것을
	// 막기 위해 sweep 캡슐 바닥을 살짝 띄운다(중심 ↑c, 반높이 ↓c → 바닥 +2c, 천장 그대로).
	// 작은 값이라 벽 충돌엔 영향이 거의 없다. 낙하 중엔 바닥이 하강을 막아야 하므로 0.
	const float GroundClearance = bLiftOffGround
		? std::max(2.0f * ContactOffset, Radius * 0.02f)
		: 0.0f;
	const FVector SweepCenterOffset(0.0f, 0.0f, GroundClearance);
	const float   SweepHalfHeight = std::max(HalfHeight - GroundClearance, Radius);

	const FQuat Rot = Updated->GetWorldRotation().ToQuaternion();

	// 한 프레임 변위(슬라이드 + 침투 해소)를 "이번 이동량 + 침투 해소 한 스텝 + 약간" 으로
	// 제한한다. 이보다 크게 튀면(박힘 깊을 때) floor probe 밖으로 나가 FindFloor 가 바닥을
	// 놓치고 Falling 에 갇혀 폭주한다. 침투 해소를 작게 두므로 이 상한도 작게 유지된다.
	const float MoveCap = TotalDist + MaxDepenStep + 2.0f * ContactOffset;

	FVector Position    = Start;
	FVector Remaining   = Delta;
	bool    bHitAny     = false;

	for (int Iter = 0; Iter < MaxSlides; ++Iter)
	{
		const float Dist = Remaining.Length();
		if (Dist <= 1e-5f) break;
		const FVector Dir = Remaining * (1.0f / Dist);

		FHitResult Hit;
		const bool bBlocked = World->PhysicsSweepCapsuleByObjectTypes(
			Position + SweepCenterOffset, Rot, Radius, SweepHalfHeight, Dir, Dist, Hit, WallMask, Owner);
		if (!bBlocked)
		{
			Position = Position + Remaining;   // 막힘 없음 — 전부 이동하고 종료.
			break;
		}

		const FVector Normal = Hit.ImpactNormal;
		// 퇴화된 normal(0 또는 비단위) — 더 진행하지 않고 안전하게 종료(잘못된 방향 누적 방지).
		if (Normal.Dot(Normal) < 0.5f) break;

		bHitAny = true;

		if (Hit.PenetrationDepth > 0.0f)
		{
			// 시작부터 겹침(초기 침투). 핵심: 이걸 "이동 차단"으로 처리하면 안 된다 —
			// 깊이 박혔을 때 내려가려는(낙하) 이동이 통째로 수평 밀어내기로 바뀌어 캐릭터가
			// 한 방향으로 끌려가며 바닥에 가라앉는다. 대신:
			//  1) 표면 밖으로 "작게" 한 스텝만 부드럽게 밀어낸다(MaxDepenStep). 깊이 박혀도
			//     여러 프레임에 걸쳐 서서히 빠져나온다.
			//  2) 남은 이동 중 표면으로 더 "파고드는" 성분만 제거(접선/바깥 이동은 허용 →
			//     낙하·정상 이동은 그대로 진행). 그래야 가라앉지 않고 자연히 빠져나온다.
			//  3) 같은 프레임에 남은 이동을 적용하고 종료(재-sweep 으로 같은 겹침을 다시 잡아
			//     무한히 밀어내는 ooze 방지).
			const float Push = std::min(Hit.PenetrationDepth, MaxDepenStep);
			Position = Position + Normal * Push;
			const float Into = Remaining.Dot(Normal);
			if (Into < 0.0f)
			{
				Remaining = Remaining - Normal * Into;   // 파고드는 성분만 제거(슬라이드)
			}
			Position = Position + Remaining;
			break;
		}

		// 표면 직전(skin 만큼 여유)까지 이동.
		const float Travel = std::max(Hit.Distance - ContactOffset, 0.0f);
		Position = Position + Dir * Travel;

		// 남은 이동량을 벽 평면에 투영 → 표면을 따라 미끄러짐(slide).
		FVector LeftOver = Remaining - Dir * Travel;
		LeftOver = LeftOver - Normal * LeftOver.Dot(Normal);
		Remaining = LeftOver;
	}

	// 안전 클램프: 어떤 경로로도 한 프레임 이동량(MoveCap) 이상으로 튀지 않게 해
	// sweep/normal/침투 이상에 의한 폭주를 구조적으로 차단.
	FVector Disp = Position - Start;
	const float DispLen = Disp.Length();
	if (DispLen > MoveCap && DispLen > 1e-5f)
	{
		Position = Start + Disp * (MoveCap / DispLen);
	}

	Updated->SetWorldLocation(Position);
	return bHitAny;
}

void UCharacterMovementComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << MaxWalkSpeed;
	Ar << MaxAcceleration;
	Ar << BrakingFriction;
	Ar << Gravity;
	Ar << FloorProbeDistance;
	Ar << JumpZVelocity;
	Ar << bOrientRotationToMovement;
	Ar << RotationYawRate;
	// 뒤에 추가된 필드는 맨 끝에 append (수동 직렬화 — 기존 저장 데이터의 정렬 보존).
	Ar << WalkableFloorAngle;
}
