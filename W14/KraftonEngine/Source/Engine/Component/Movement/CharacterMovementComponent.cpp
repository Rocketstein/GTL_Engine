#include "CharacterMovementComponent.h"

#include "Animation/AnimInstance.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/SceneComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
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

namespace
{
	constexpr float CharacterMoveSmallNumber = 1.0e-5f;
	constexpr float CharacterPenetrationProbeDistance = 0.02f;
	constexpr float CharacterMaxDepenetrationDistancePerFrame = 0.10f;


	int32 ClampIterationCount(float Value, int32 MinValue, int32 MaxValue)
	{
		return (std::max)(MinValue, (std::min)(MaxValue, static_cast<int32>(std::round(Value))));
	}
}

UCharacterMovementComponent::UCharacterMovementComponent()
{
	// AI input, root motion and player input are produced in TG_PrePhysics.
	// CharacterMovement consumes them immediately after, before the physics frame is submitted,
	// and writes the final kinematic capsule transform for the same frame.
	PrimaryComponentTick.SetTickGroup(TG_PrePhysicsMovement);
	PrimaryComponentTick.SetEndTickGroup(TG_PrePhysicsMovement);
}

void UCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	EnforceCharacterControllerPolicy();
	RecoverFromPenetration();
	SnapToFloor(FloorProbeDistance + GroundSnapDistance);
	bNeedsInitialGrounding = false;
}

void UCharacterMovementComponent::AddInputVector(const FVector& WorldDirection, float ScaleValue)
{
	if (!bMovementInputEnabled)
	{
		return;
	}
	AccumulatedInput = AccumulatedInput + WorldDirection * ScaleValue;
}

void UCharacterMovementComponent::ClearInputVector()
{
	AccumulatedInput = FVector::ZeroVector;
}

void UCharacterMovementComponent::SetMovementInputEnabled(bool bEnabled)
{
	if (bMovementInputEnabled == bEnabled)
	{
		return;
	}

	bMovementInputEnabled = bEnabled;
	if (!bMovementInputEnabled)
	{
		ClearInputVector();
	}
}

void UCharacterMovementComponent::StopMovementImmediately()
{
	StopMovementImmediately(false);
}

void UCharacterMovementComponent::StopMovementImmediately(bool bPreserveVerticalVelocity)
{
	AccumulatedInput = FVector::ZeroVector;

	if (bPreserveVerticalVelocity)
	{
		PendingExternalImpulse.X = 0.0f;
		PendingExternalImpulse.Y = 0.0f;
		Velocity.X = 0.0f;
		Velocity.Y = 0.0f;
		return;
	}

	PendingExternalImpulse = FVector::ZeroVector;
	Velocity = FVector::ZeroVector;
}

void UCharacterMovementComponent::StopHorizontalMovementImmediately()
{
	StopMovementImmediately(true);
}

void UCharacterMovementComponent::AddExternalImpulse(const FVector& WorldImpulse)
{
	PendingExternalImpulse = PendingExternalImpulse + WorldImpulse;
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

	const FMatrix M = LocalDelta.ToMatrix() * PendingRootMotion.ToMatrix();
	PendingRootMotion.Location = FVector(M.M[3][0], M.M[3][1], M.M[3][2]);
	PendingRootMotion.Rotation = (LocalDelta.Rotation * PendingRootMotion.Rotation).GetNormalized();
}

bool UCharacterMovementComponent::ConsumePendingRootMotion(FTransform& OutLocalDelta)
{
	if (!bHasPendingRootMotion)
	{
		OutLocalDelta = FTransform();
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
}

void UCharacterMovementComponent::Jump()
{
	if (MovementMode != EMovementMode::Walking) return;
	bWantsJump = true;
}

void UCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;
	if (DeltaTime <= 0.0f) return;

	EnforceCharacterControllerPolicy();

	if (bNeedsInitialGrounding)
	{
		RecoverFromPenetration();
		SnapToFloor(FloorProbeDistance + GroundSnapDistance);
		bNeedsInitialGrounding = false;
	}
	else
	{
		RecoverFromPenetration();
	}

	if (MovementMode == EMovementMode::Walking)
	{
		ApplyBasedMovement();
	}

	if (!PendingExternalImpulse.IsNearlyZero())
	{
		Velocity = Velocity + PendingExternalImpulse;
		PendingExternalImpulse = FVector::ZeroVector;
		if (Velocity.Z > 0.0f)
		{
			SetMovementMode(EMovementMode::Falling);
		}
	}

	bAppliedRootMotionYawThisFrame = false;

	FVector Input;
	ConsumeInputVector(Input);
	Input.Z = 0.0f;

	ApplyInputToVelocity(Input, DeltaTime);

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
		{
			if (UAnimInstance* AI = Mesh->GetAnimInstance())
			{
				if (AI->GetRootMotionMode() != ERootMotionMode::IgnoreRootMotion)
				{
					const FTransform ConsumedRootMotion = AI->ConsumeRootMotion();
					AddRootMotionDelta(ConsumedRootMotion);
				}
			}
		}
	}

	FTransform RootMotionDelta;
	const bool bHadRootMotion = ConsumePendingRootMotion(RootMotionDelta);
	FVector RootMotionWorldXY(0.0f, 0.0f, 0.0f);
	if (bHadRootMotion)
	{
		FVector Forward = Updated->GetForwardVector();
		FVector Right   = Updated->GetRightVector();
		Forward.Z = 0.0f;
		Right.Z   = 0.0f;

		if (!Forward.IsNearlyZero())
		{
			Forward.Normalize();
		}
		else
		{
			Forward = FVector::ForwardVector;
		}

		if (!Right.IsNearlyZero())
		{
			Right.Normalize();
		}
		else
		{
			Right = FVector(-Forward.Y, Forward.X, 0.0f);
		}

		const FVector World = Forward * RootMotionDelta.Location.X + Right * RootMotionDelta.Location.Y;
		RootMotionWorldXY.X = World.X;
		RootMotionWorldXY.Y = World.Y;
	}

	if (MovementMode == EMovementMode::Walking)
	{
		TickWalking(DeltaTime, RootMotionWorldXY);
	}
	else
	{
		TickFalling(DeltaTime, RootMotionWorldXY);
	}

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

	if (bOrientRotationToMovement && !bAppliedRootMotionYawThisFrame)
	{
		PhysOrientToMovement(DeltaTime);
	}
}

void UCharacterMovementComponent::EnforceCharacterControllerPolicy()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter && OwnerCharacter->IsInRagdoll())
	{
		return;
	}

	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(GetUpdatedComponent()))
	{
		// Locomotion capsule is a kinematic query/controller body. It must not become
		// a dynamic PhysicsToEngine body, because CharacterMovement owns its transform.
		Primitive->SetSimulatePhysics(false);
		Primitive->SetKinematic(true);
		Primitive->SetCanEverAffectNavigation(false);
		if (Primitive->GetCollisionObjectType() != ECollisionChannel::Pawn)
		{
			Primitive->SetCollisionObjectType(ECollisionChannel::Pawn);
		}
		if (Primitive->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
		{
			// Keep the capsule as a kinematic simulation obstacle for dynamic bodies while
			// CharacterMovement remains the sole transform owner. Static/character collision
			// is still resolved by sweeps and floor probes, not by rigid-body integration.
			Primitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}

	if (OwnerCharacter)
	{
		if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
		{
			// The visual mesh is not the locomotion body. Ragdoll uses the physics-asset
			// instance, not the mesh component primitive body.
			Mesh->SetSimulatePhysics(false);
			Mesh->SetCanEverAffectNavigation(false);
			if (Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}

void UCharacterMovementComponent::PhysOrientToMovement(float DeltaTime)
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return;

	const float SpeedSq2D = Velocity.X * Velocity.X + Velocity.Y * Velocity.Y;
	constexpr float MinSpeedSq = 1e-4f;
	if (SpeedSq2D < MinSpeedSq) return;

	const float TargetYaw = std::atan2(Velocity.Y, Velocity.X) * (180.0f / 3.14159265f);

	FRotator R = Updated->GetRelativeRotation();
	const float CurrentYaw = R.Yaw;

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
		const FVector Direction = Input * (1.0f / InputLen);
		Velocity.X += Direction.X * MaxAcceleration * DeltaTime;
		Velocity.Y += Direction.Y * MaxAcceleration * DeltaTime;
	}
	else if (MovementMode == EMovementMode::Walking)
	{
		if (bStopImmediatelyWhenNoInput)
		{
			Velocity.X = 0.0f;
			Velocity.Y = 0.0f;
			return;
		}

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

	// 수평 속도 상한은 "지상 이동" 또는 "입력으로 조향 중"일 때만 적용한다.
	// 공중에서 입력 없이 받은 외부 임펄스(백점프/넉백)는 자르지 않아야 임펄스가 그대로 거리로 환산된다.
	// (이전엔 모드/입력 무관하게 매 틱 클램프해, LeapHorizontalForce 가 MaxWalkSpeed 로 깎여 "제자리 점프" 가 났다.)
	const bool bClampToWalkSpeed = (MovementMode == EMovementMode::Walking) || (InputLen > 0.0f);
	if (bClampToWalkSpeed)
	{
		FVector V2D(Velocity.X, Velocity.Y, 0.0f);
		const float Speed2D = V2D.Length();
		if (Speed2D > MaxWalkSpeed)
		{
			const FVector Dir = V2D * (1.0f / Speed2D);
			Velocity.X = Dir.X * MaxWalkSpeed;
			Velocity.Y = Dir.Y * MaxWalkSpeed;
		}
	}
}

void UCharacterMovementComponent::TickWalking(float DeltaTime, const FVector& RootMotionWorldXY)
{
	if (bWantsJump)
	{
		bWantsJump = false;
		Velocity.Z = JumpZVelocity;
		SetMovementMode(EMovementMode::Falling);
		TickFalling(DeltaTime, RootMotionWorldXY);
		return;
	}

	Velocity.Z = 0.0f;

	// Walking is only valid while we can establish a walkable support. Do this before
	// the horizontal move as well, so actors loaded slightly sunk into the floor are
	// pulled into a stable controller pose before the first AI input is consumed.
	if (!SnapToFloor(FloorProbeDistance + GroundSnapDistance))
	{
		SetMovementMode(EMovementMode::Falling);
		TickFalling(DeltaTime, RootMotionWorldXY);
		return;
	}

	const FVector XYOffset(
		Velocity.X * DeltaTime + RootMotionWorldXY.X,
		Velocity.Y * DeltaTime + RootMotionWorldXY.Y,
		0.0f);
	MoveWithSlide(XYOffset);

	if (!SnapToFloor(FloorProbeDistance + GroundSnapDistance))
	{
		SetMovementMode(EMovementMode::Falling);
		return;
	}
}

void UCharacterMovementComponent::TickFalling(float DeltaTime, const FVector& RootMotionWorldXY)
{
	Velocity.Z -= Gravity * DeltaTime;

	const FVector Offset(
		Velocity.X * DeltaTime + RootMotionWorldXY.X,
		Velocity.Y * DeltaTime + RootMotionWorldXY.Y,
		Velocity.Z * DeltaTime);

	FHitResult MoveHit;
	MoveWithSlide(Offset, &MoveHit);

	if (Velocity.Z > 0.0f) return;

	// Falling must not use the walking ground-stick distance.  GroundSnapDistance is
	// intentionally generous so walking characters stay glued to small ramps/steps,
	// but using it while airborne makes the character teleport to the floor as soon
	// as the floor comes within that probe range.  Landing is allowed only after a
	// real downward sweep hit on walkable floor or when the remaining air gap is
	// inside a very small landing snap tolerance.
	const bool bHitWalkableFloor = MoveHit.bHit && IsWalkableFloor(MoveHit);
	if (bHitWalkableFloor)
	{
		SnapToFloor(FallingLandingSnapDistance);
		Velocity.Z = 0.0f;
		SetMovementMode(EMovementMode::Walking);
		return;
	}

	if (SnapToFloor(FallingLandingSnapDistance))
	{
		Velocity.Z = 0.0f;
		SetMovementMode(EMovementMode::Walking);
	}
}

bool UCharacterMovementComponent::ProbePenetration(FHitResult& OutHit) const
{
	OutHit = FHitResult();

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return false;
	AActor* Owner = GetOwner();
	if (!Owner) return false;
	UWorld* World = Owner->GetWorld();
	if (!World) return false;

	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Updated);
	if (!Capsule) return false;

	const float Radius     = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	if (Radius <= 0.0f || HalfHeight <= 0.0f) return false;

	ECollisionChannel TraceChannel = ECollisionChannel::Pawn;
	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Updated))
	{
		TraceChannel = Primitive->GetCollisionObjectType();
	}

	const FVector Start = Updated->GetWorldLocation();
	const FVector End   = Start + FVector(0.0f, 0.0f, CharacterPenetrationProbeDistance);
	const FQuat   Rot   = Updated->GetWorldMatrix().ToQuatWithoutScale();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	if (!World->PhysicsSweep(Start, End, Rot, Shape, OutHit, TraceChannel, Owner))
	{
		return false;
	}

	return OutHit.bStartPenetrating;
}

bool UCharacterMovementComponent::RecoverFromPenetration()
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return false;

	FHitResult Hit;
	if (!ProbePenetration(Hit))
	{
		return true;
	}

	if (ClampIterationCount(MaxDepenetrationIterations, 0, 1) == 0)
	{
		return false;
	}

	FVector Normal = Hit.ImpactNormal;
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;
	}
	Normal.Normalize();

	const float Depth = std::max(Hit.PenetrationDepth, 0.0f);
	const float CorrectionDistance = std::clamp(
		Depth + DepenetrationSkin,
		0.0f,
		CharacterMaxDepenetrationDistancePerFrame);

	if (CorrectionDistance <= CharacterMoveSmallNumber)
	{
		return false;
	}

	Updated->SetWorldLocation(
		Updated->GetWorldLocation() + Normal * CorrectionDistance);

	FHitResult FinalHit;
	return !ProbePenetration(FinalHit);
}

bool UCharacterMovementComponent::SafeMoveUpdatedComponent(const FVector& Delta, FHitResult* OutHit)
{
	if (OutHit)
	{
		*OutHit = FHitResult();
	}

	if (Delta.Length() <= 1.e-6f)
	{
		return true;
	}

	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Updated);
	if (!World || !Capsule)
	{
		Updated->SetWorldLocation(Updated->GetWorldLocation() + Delta);
		return true;
	}

	const float Radius     = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	if (Radius <= 0.0f || HalfHeight <= 0.0f)
	{
		Updated->SetWorldLocation(Updated->GetWorldLocation() + Delta);
		return true;
	}

	RecoverFromPenetration();

	ECollisionChannel TraceChannel = ECollisionChannel::Pawn;
	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Updated))
	{
		TraceChannel = Primitive->GetCollisionObjectType();
	}

	FVector Start = Updated->GetWorldLocation();
	FVector End   = Start + Delta;
	const FQuat   Rot   = Updated->GetWorldMatrix().ToQuatWithoutScale();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	FHitResult Hit;
	if (!World->PhysicsSweep(Start, End, Rot, Shape, Hit, TraceChannel, Owner))
	{
		Updated->SetWorldLocation(End);
		return true;
	}

	if (Hit.bStartPenetrating)
	{
		const bool bRecovered = RecoverFromPenetration();
		Start = Updated->GetWorldLocation();
		End   = Start + Delta;
		Hit   = FHitResult();
		if (!World->PhysicsSweep(Start, End, Rot, Shape, Hit, TraceChannel, Owner))
		{
			Updated->SetWorldLocation(End);
			return true;
		}
		if (!bRecovered && Hit.bStartPenetrating)
		{
			if (OutHit)
			{
				*OutHit = Hit;
			}
			return false;
		}
	}

	if (OutHit)
	{
		*OutHit = Hit;
	}

	const FVector MoveDir = Delta.Normalized();
	const float SafeDistance = (std::max)(0.0f, Hit.Distance - SweepPullbackDistance);
	Updated->SetWorldLocation(Start + MoveDir * SafeDistance);

	if (UPrimitiveComponent* MovingPrimitive = Cast<UPrimitiveComponent>(Updated))
	{
		MovingPrimitive->NotifyComponentHit(
			MovingPrimitive,
			Hit.HitActor,
			Hit.HitComponent,
			FVector::ZeroVector,
			Hit
		);
	}

	HandleBlockingHitPhysicsInteraction(Delta, Hit);

	if (!Hit.ImpactNormal.IsNearlyZero())
	{
		const float VelocityIntoSurface = Velocity.Dot(Hit.ImpactNormal);
		if (VelocityIntoSurface < 0.0f)
		{
			Velocity = Velocity - Hit.ImpactNormal * VelocityIntoSurface;
		}
	}

	return false;
}
bool UCharacterMovementComponent::MoveWithSlide(const FVector& Delta, FHitResult* OutHit)
{
	if (OutHit)
	{
		*OutHit = FHitResult();
	}

	FVector Remaining = Delta;
	bool bMovedAtLeastOnce = false;
	const int32 Iterations = ClampIterationCount(MaxSlideIterations, 1, 8);

	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		if (Remaining.Length() <= CharacterMoveSmallNumber)
		{
			return true;
		}

		const FVector Before = GetUpdatedComponent() ? GetUpdatedComponent()->GetWorldLocation() : FVector::ZeroVector;
		FHitResult Hit;
		const bool bMovedFreely = SafeMoveUpdatedComponent(Remaining, &Hit);
		const FVector After = GetUpdatedComponent() ? GetUpdatedComponent()->GetWorldLocation() : Before;
		const float Progress = FVector::Distance(Before, After);
		bMovedAtLeastOnce = bMovedAtLeastOnce || Progress > CharacterMoveSmallNumber;

		if (bMovedFreely)
		{
			return true;
		}

		if (OutHit)
		{
			*OutHit = Hit;
		}

		if (MovementMode == EMovementMode::Walking && TryStepUp(Remaining, Hit))
		{
			return true;
		}

		FVector Normal = Hit.ImpactNormal;
		if (Normal.IsNearlyZero())
		{
			Normal = FVector::UpVector;
		}
		Normal.Normalize();

		const float RemainingLen = Remaining.Length();
		const float UsedFraction = RemainingLen > CharacterMoveSmallNumber
			? (std::max)(0.0f, (std::min)(1.0f, Progress / RemainingLen))
			: 1.0f;
		Remaining = Remaining * (1.0f - UsedFraction);

		const float IntoSurface = Remaining.Dot(Normal);
		if (IntoSurface < 0.0f)
		{
			Remaining = Remaining - Normal * IntoSurface;
		}

		if (!bMovedAtLeastOnce && Remaining.Length() <= CharacterMoveSmallNumber)
		{
			return false;
		}
	}

	return bMovedAtLeastOnce;
}

bool UCharacterMovementComponent::TryStepUp(const FVector& MoveDelta, const FHitResult& BlockingHit)
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return false;
	if (MaxStepHeight <= CharacterMoveSmallNumber) return false;
	if (BlockingHit.ImpactNormal.Z > WalkableFloorZ) return false;

	const FVector Horizontal = FVector(MoveDelta.X, MoveDelta.Y, 0.0f);
	if (Horizontal.Length() <= CharacterMoveSmallNumber) return false;

	const FVector OriginalLocation = Updated->GetWorldLocation();

	FHitResult UpHit;
	if (!SafeMoveUpdatedComponent(FVector(0.0f, 0.0f, MaxStepHeight), &UpHit))
	{
		Updated->SetWorldLocation(OriginalLocation);
		return false;
	}

	FHitResult ForwardHit;
	if (!SafeMoveUpdatedComponent(Horizontal, &ForwardHit))
	{
		Updated->SetWorldLocation(OriginalLocation);
		return false;
	}

	if (!SnapToFloor(MaxStepHeight + FloorProbeDistance + GroundSnapDistance))
	{
		Updated->SetWorldLocation(OriginalLocation);
		return false;
	}

	return true;
}

bool UCharacterMovementComponent::IsWalkableFloor(const FHitResult& Hit) const
{
	if (!Hit.bHit)
	{
		return false;
	}

	FVector Normal = Hit.ImpactNormal;
	if (Normal.IsNearlyZero())
	{
		Normal = Hit.WorldNormal;
	}
	if (Normal.IsNearlyZero())
	{
		return true;
	}
	Normal.Normalize();
	return Normal.Z >= WalkableFloorZ;
}

bool UCharacterMovementComponent::SnapToFloor(float ProbeDistance)
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return false;

	FHitResult Floor;
	const bool bHasFloor = TraceFloorWithProbeDistance(ProbeDistance, Floor);

	if (!bHasFloor || !IsWalkableFloor(Floor))
	{
		ClearMovementBase();
		return false;
	}

	FVector NewLoc = Updated->GetWorldLocation();
	NewLoc.Z = Floor.WorldHitLocation.Z + GetCapsuleHalfHeight();
	Updated->SetWorldLocation(NewLoc);
	UpdateMovementBaseFromFloor(Floor);
	return true;
}

bool UCharacterMovementComponent::TraceFloor(FHitResult& OutHit) const
{
	return TraceFloorWithProbeDistance(FloorProbeDistance, OutHit);
}

bool UCharacterMovementComponent::TraceFloorWithProbeDistance(float ProbeDistance, FHitResult& OutHit) const
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated) return false;
	AActor* Owner = GetOwner();
	if (!Owner) return false;
	UWorld* World = Owner->GetWorld();
	if (!World) return false;

	const float HalfHeight = GetCapsuleHalfHeight();
	if (HalfHeight <= 0.0f) return false;

	const FVector  Start = Updated->GetWorldLocation();
	const FVector  Dir(0.0f, 0.0f, -1.0f);
	const float    MaxDist = HalfHeight + (std::max)(0.0f, ProbeDistance);

	return World->PhysicsRaycastByObjectTypes(Start, Dir, MaxDist, OutHit,
		GetFloorObjectTypeMask(), Owner);
}

void UCharacterMovementComponent::ApplyBasedMovement()
{
	USceneComponent* Updated = GetUpdatedComponent();
	if (!Updated || !bHasMovementBase)
	{
		return;
	}

	UPrimitiveComponent* Base = CurrentMovementBase.Get();
	if (!Base || !IsValid(Base))
	{
		ClearMovementBase();
		return;
	}

	const FVector BaseLocation = Base->GetWorldLocation();
	const FVector Delta = BaseLocation - LastMovementBaseLocation;
	LastMovementBaseLocation = BaseLocation;

	if (Delta.Length() <= CharacterMoveSmallNumber)
	{
		return;
	}

	Updated->SetWorldLocation(Updated->GetWorldLocation() + Delta);
	RecoverFromPenetration();
}

void UCharacterMovementComponent::UpdateMovementBaseFromFloor(const FHitResult& FloorHit)
{
	UPrimitiveComponent* NewBase = FloorHit.HitComponent;
	if (!NewBase || !IsValid(NewBase))
	{
		ClearMovementBase();
		return;
	}

	CurrentMovementBase = NewBase;
	LastMovementBaseLocation = NewBase->GetWorldLocation();
	bHasMovementBase = true;
}

void UCharacterMovementComponent::ClearMovementBase()
{
	CurrentMovementBase.Reset();
	LastMovementBaseLocation = FVector::ZeroVector;
	bHasMovementBase = false;
}

void UCharacterMovementComponent::HandleBlockingHitPhysicsInteraction(const FVector& AttemptedMove, const FHitResult& Hit)
{
	if (!bEnablePhysicsInteraction || !Hit.HitComponent || !Hit.HitComponent->GetSimulatePhysics())
	{
		return;
	}

	FVector PushDir(AttemptedMove.X, AttemptedMove.Y, 0.0f);
	if (PushDir.Length() <= CharacterMoveSmallNumber)
	{
		PushDir = FVector(Velocity.X, Velocity.Y, 0.0f);
	}
	const float PushSpeed = PushDir.Length();
	if (PushSpeed <= PhysicsPushMinSpeed)
	{
		return;
	}
	PushDir = PushDir * (1.0f / PushSpeed);

	float NormalScale = 1.0f;
	if (!Hit.ImpactNormal.IsNearlyZero())
	{
		NormalScale = (std::max)(0.0f, -PushDir.Dot(Hit.ImpactNormal.Normalized()));
		if (NormalScale <= CharacterMoveSmallNumber)
		{
			return;
		}
	}

	const FVector Impulse = PushDir * (PhysicsPushImpulseScale * PushSpeed * NormalScale);
	Hit.HitComponent->AddImpulse(Impulse);
}

uint32 UCharacterMovementComponent::GetFloorObjectTypeMask() const
{
	// Floor can be static geometry or kinematic/world-dynamic platforms. Pawns are
	// intentionally excluded; character-character contact is solved by sweeps and
	// depenetration, not by treating another capsule as walkable ground.
	return ObjectTypeBit(ECollisionChannel::WorldStatic) |
		ObjectTypeBit(ECollisionChannel::WorldDynamic);
}

float UCharacterMovementComponent::GetCapsuleHalfHeight() const
{
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
	Ar << SweepPullbackDistance;
	Ar << GroundSnapDistance;
	Ar << MaxStepHeight;
	Ar << WalkableFloorZ;
	Ar << MaxSlideIterations;
	Ar << MaxDepenetrationIterations;
	Ar << DepenetrationSkin;
}
