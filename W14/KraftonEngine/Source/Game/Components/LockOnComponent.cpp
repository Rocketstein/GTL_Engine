#include "LockOnComponent.h"
#include "Engine/GameFramework/World.h"
#include "Engine/GameFramework/AActor.h"
#include "Engine/Component/Camera/CameraComponent.h"
#include "Engine/Component/Primitive/BillboardComponent.h"
#include "Engine/Component/Shape/CapsuleComponent.h"
#include "Engine/Component/Camera/SpringArmComponent.h"
#include "Engine/Materials/Material.h"
#include "Engine/Materials/MaterialManager.h"
#include "Component/Combat/HealthComponent.h"
#include "GameFramework/Pawn/EnemyCharacter.h"
#include "LockOnMarkerComponent.h"

namespace
{
	// 각도를 (-180, 180] 로 정규화 — 최단 경로 보간용.
	float NormalizeDegrees(float Angle)
	{
		Angle = fmodf(Angle + 180.0f, 360.0f);
		if (Angle < 0.0f) Angle += 360.0f;
		return Angle - 180.0f;
	}

	// 지수 감쇠 기반 각도 보간 — 프레임률에 독립적이고 최단 경로로 감긴다.
	float AngleInterpTo(float Current, float Target, float DeltaTime, float Speed)
	{
		if (DeltaTime <= 0.0f || Speed <= 0.0f) return Current;
		const float Delta = NormalizeDegrees(Target - Current);
		const float Alpha = 1.0f - expf(-Speed * DeltaTime);
		return NormalizeDegrees(Current + Delta * Alpha);
	}

	float FloatInterpTo(float Current, float Target, float DeltaTime, float Speed)
	{
		if (DeltaTime <= 0.0f || Speed <= 0.0f) return Current;
		const float Alpha = 1.0f - expf(-Speed * DeltaTime);
		return FMath::Lerp(Current, Target, Alpha);
	}

	FVector VectorInterpTo(const FVector& Current, const FVector& Target, float DeltaTime, float Speed)
	{
		if (DeltaTime <= 0.0f || Speed <= 0.0f) return Current;
		const float Alpha = 1.0f - expf(-Speed * DeltaTime);
		return FVector::Lerp(Current, Target, Alpha);
	}
}

void ULockOnComponent::BeginPlay()
{
	Super::BeginPlay();

	// 락온 마커 빌보드 1회 생성 — 평소엔 숨기고, 락온 시점에 타깃 위로 띄운다.
	// ULockOnMarkerComponent 는 NeverCull 프록시라 적 메시 뒤에서도 컬링되지 않는다.
	AActor* Owner = GetOwner();
	if (!Owner) return;

	BindOwnerHealthEvents();

	UBillboardComponent* Marker = Owner->AddComponent<ULockOnMarkerComponent>();
	if (!Marker) return;

	// 락온 마커는 월드 공간에 독립적으로 둔다. 플레이어 루트에 붙이면 WASD 이동 중
	// 부모 이동을 한 프레임 상속받고 UpdateMarker에서 되돌아와 흔들려 보일 수 있다.
	Marker->SetParent(nullptr);
	Marker->SetAbsoluteScale(true);
	Marker->SetRelativeScale(FVector(MarkerScale, MarkerScale, MarkerScale));

	// 아이콘 머티리얼 로드. 에셋이 없으면 매니저가 기본(핑크) 머티리얼을 돌려주므로 안전.
	if (!MarkerMaterialPath.empty() && MarkerMaterialPath != "None")
	{
		if (UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(MarkerMaterialPath))
		{
			Marker->SetMaterial(Material);
		}
	}

	// 락온 전까지는 숨김. UpdateMarker 가 타깃 획득 시 켠다.
	Marker->SetVisibility(false);

	// 스프링 암 컴포넌트 셋업
	for (UActorComponent* Comp : Owner->GetComponents())
	{
		if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Comp))
		{
			SpringArmComponent = SpringArm;
		}
	}

	MarkerComponent = Marker;
}

void ULockOnComponent::EndPlay()
{
	ClearLockOn();
	UnbindOwnerHealthEvents();
	Super::EndPlay();
}

void ULockOnComponent::BindOwnerHealthEvents()
{
	if (OwnerDeathHandle.IsValid())
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	UHealthComponent* HealthComponent = OwnerActor ? OwnerActor->GetComponentByClass<UHealthComponent>() : nullptr;
	if (!HealthComponent)
	{
		return;
	}

	BoundHealthComponent = HealthComponent;
	OwnerDeathHandle = HealthComponent->OnDeath.AddWeakUObject(this, &ULockOnComponent::HandleOwnerDeath);
}

void ULockOnComponent::UnbindOwnerHealthEvents()
{
	if (UHealthComponent* HealthComponent = BoundHealthComponent.Get())
	{
		if (OwnerDeathHandle.IsValid())
		{
			HealthComponent->OnDeath.Remove(OwnerDeathHandle);
		}
	}

	OwnerDeathHandle.Reset();
	BoundHealthComponent.Reset();
}

void ULockOnComponent::HandleOwnerDeath(
	UHealthComponent* /*Component*/,
	AActor* /*DamageCauser*/,
	AActor* /*InstigatorActor*/)
{
	ClearLockOn();
}

void ULockOnComponent::ToggleLockOn()
{
	if (LockedTarget.Get())
	{
		ClearLockOn();
		return;
	}

	AEnemyCharacter* Target = Cast<AEnemyCharacter>(FindBestTarget());
	if (!Target || !MarkerComponent) return;

	SetLockedTarget(Target);
}

void ULockOnComponent::ClearLockOn()
{
	LockedTarget.Reset();
	if (MarkerComponent) MarkerComponent->SetVisibility(false);
	SwitchInputX = 0.0f;
	SwitchCooldownRemaining = 0.0f;
}

bool ULockOnComponent::AddSwitchInput(float MouseDeltaX)
{
	if (!LockedTarget.Get() || SwitchMouseThreshold <= 0.0f)
	{
		return false;
	}

	if (SwitchCooldownRemaining > 0.0f)
	{
		return false;
	}

	SwitchInputX += MouseDeltaX;

	if (SwitchInputX >= SwitchMouseThreshold)
	{
		SwitchInputX = 0.0f;
		return SwitchLockTarget(1);
	}

	if (SwitchInputX <= -SwitchMouseThreshold)
	{
		SwitchInputX = 0.0f;
		return SwitchLockTarget(-1);
	}

	return false;
}

bool ULockOnComponent::SwitchLockTarget(int Direction)
{
	if (!LockedTarget.Get())
	{
		return false;
	}

	AEnemyCharacter* Target = FindSwitchTarget(Direction);
	if (!Target)
	{
		return false;
	}

	SetLockedTarget(Target);
	SwitchCooldownRemaining = SwitchCooldownSeconds;
	return true;
}

void ULockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	if (SwitchCooldownRemaining > 0.0f)
	{
		SwitchCooldownRemaining -= DeltaTime;
		if (SwitchCooldownRemaining < 0.0f)
		{
			SwitchCooldownRemaining = 0.0f;
		}
	}

	if (AActor* Target = LockedTarget.Get())
	{
		if (IsValidLockTarget(Target))
		{
			UpdateCameraLock(DeltaTime);
			UpdateMarker();
			return;
		}

		ClearLockOn();
	}

	RestoreCamera(DeltaTime);
	UpdateMarker();
}

AActor* ULockOnComponent::FindBestTarget() const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UWorld* World = OwnerPawn ? OwnerPawn->GetWorld() : nullptr;
	if (!OwnerPawn || !World) return nullptr;

	const FVector PlayerLoc = OwnerPawn->GetActorLocation();
	const FVector CameraForward = OwnerPawn->GetControlRotation().GetForwardVector();

	AActor* Best = nullptr;
	float BestScore = FLT_MAX;

	for (AActor* Actor : World->GetActors())
	{
		AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor);
		if (!IsValidLockTarget(Enemy)) continue;

		FVector ToTarget = GetLockOnPoint(Enemy) - PlayerLoc;
		const float Dist = ToTarget.Length();
		if (Dist > MaxLockDistance) continue;

		FVector Dir = ToTarget.Normalized();
		const float Dot = FMath::Clamp(CameraForward.Dot(Dir), -1.0f, 1.0f);
		const float Angle = acosf(Dot) * FMath::RadToDeg;
		if (Angle > AcquireAngleDegrees) continue;

		const float Score = Angle * 2.0f + Dist * 0.25f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Enemy;
		}
	}

	return Best;
}

AEnemyCharacter* ULockOnComponent::FindSwitchTarget(int Direction) const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UWorld* World = OwnerPawn ? OwnerPawn->GetWorld() : nullptr;
	AActor* CurrentTarget = LockedTarget.Get();
	if (!OwnerPawn || !World || !CurrentTarget || Direction == 0) return nullptr;

	FVector CameraLoc = OwnerPawn->GetActorLocation();
	FVector CameraForward = OwnerPawn->GetControlRotation().GetForwardVector();
	FVector CameraRight = OwnerPawn->GetControlRotation().GetRightVector();

	if (UCameraComponent* Camera = OwnerPawn->GetComponentByClass<UCameraComponent>())
	{
		CameraLoc = Camera->GetWorldLocation();
		CameraForward = Camera->GetForwardVector();
		CameraRight = Camera->GetRightVector();
	}

	CameraForward.Z = 0.0f;
	CameraRight.Z = 0.0f;
	if (CameraForward.Length() <= 1.0e-4f || CameraRight.Length() <= 1.0e-4f)
	{
		return nullptr;
	}
	CameraForward = CameraForward.Normalized();
	CameraRight = CameraRight.Normalized();

	const FVector PlayerLoc = OwnerPawn->GetActorLocation();

	// 현재 락온 타깃의 수평 베어링(카메라 forward 기준, +가 오른쪽). 후보는 이 기준에 대한 상대각으로 본다.
	FVector CurDir = GetLockOnPoint(CurrentTarget) - CameraLoc;
	CurDir.Z = 0.0f;
	if (CurDir.Length() <= 1.0e-4f) return nullptr;
	CurDir = CurDir.Normalized();
	const float CurBearing = atan2f(CurDir.Dot(CameraRight), CurDir.Dot(CameraForward)) * FMath::RadToDeg;

	// 최소 각 분리 — 거의 같은 베어링의 타깃으로 스위치되는 지터 방지. SwitchSideDotMin 을 sine 으로 해석.
	const float MinSeparationDeg = asinf(FMath::Clamp(SwitchSideDotMin, 0.0f, 1.0f)) * FMath::RadToDeg;

	AEnemyCharacter* Best = nullptr;
	float BestGap = FLT_MAX; // 요청 방향으로 각 간격이 가장 작은 이웃을 고른다(=바로 옆 타깃).

	for (AActor* Actor : World->GetActors())
	{
		AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor);
		if (!IsValidLockTarget(Enemy) || Enemy == CurrentTarget) continue;

		const FVector LockPoint = GetLockOnPoint(Enemy);
		if (FVector::DistSquared(LockPoint, PlayerLoc) > BreakDistance * BreakDistance) continue;

		FVector Dir = LockPoint - CameraLoc;
		Dir.Z = 0.0f;
		if (Dir.Length() <= 1.0e-4f) continue;
		Dir = Dir.Normalized();

		// 카메라 후방(거의 뒤) 타깃은 스위치 대상에서 제외.
		if (Dir.Dot(CameraForward) < SwitchForwardDotMin) continue;

		// 현재 타깃 기준 상대 베어링: +면 오른쪽, -면 왼쪽. (-180,180] 로 정규화.
		const float CandBearing = atan2f(Dir.Dot(CameraRight), Dir.Dot(CameraForward)) * FMath::RadToDeg;
		const float RelBearing = NormalizeDegrees(CandBearing - CurBearing);

		// 요청 방향(부호) 쪽이고 최소 분리 이상인 후보만. 가장 작은 간격 = 바로 옆 이웃.
		const float DirectionalGap = (Direction > 0) ? RelBearing : -RelBearing;
		if (DirectionalGap < MinSeparationDeg) continue;

		if (DirectionalGap < BestGap)
		{
			BestGap = DirectionalGap;
			Best = Enemy;
		}
	}

	return Best;
}

FVector ULockOnComponent::GetLockOnPoint(AActor* Target) const
{
	if (!Target) return FVector::ZeroVector;

	FVector Point = Target->GetActorLocation();

	if (ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			//Point.Z += Capsule->GetScaledCapsuleHalfHeight() * 0.65f;
			return Point;
		}
	}

	//Point.Z += 1.5f;
	return Point;
}

bool ULockOnComponent::IsValidLockTarget(AActor* Target) const
{
	AEnemyCharacter* EnemyTarget = Cast<AEnemyCharacter>(Target);
	if (!EnemyTarget || EnemyTarget->HasTag("Untargettable") || EnemyTarget->IsDead()) return false;
	return true;
}

void ULockOnComponent::SetLockedTarget(AEnemyCharacter* Target)
{
	if (!Target || !MarkerComponent)
	{
		return;
	}

	LockedTarget = Target;
	MarkerComponent->SetWorldLocation(GetLockOnPoint(Target));

	float AppliedMarkerScale = MarkerScale;
	if (ScaleType == EMarkerScaleType::Relative)
	{
		if (UCapsuleComponent* Capsule = Target->GetCapsuleComponent())
		{
			AppliedMarkerScale = MarkerScale * Capsule->GetScaledCapsuleHalfHeight();
		}
	}
	MarkerComponent->SetRelativeScale(FVector(AppliedMarkerScale, AppliedMarkerScale, AppliedMarkerScale));
	MarkerComponent->SetVisibility(true);
}

void ULockOnComponent::UpdateCameraLock(float DeltaTime)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AActor* Target = LockedTarget.Get();
	if (!OwnerPawn || !Target) return;

	if (!IsValidLockTarget(Target))
	{
		ClearLockOn();
		RestoreCamera(DeltaTime);
		return;
	}

	const FVector LockPoint = GetLockOnPoint(Target);

	if (USpringArmComponent* SpringArm = SpringArmComponent.Get())
	{
		FVector ToTarget = LockPoint - OwnerPawn->GetActorLocation();
		ToTarget.Z = 0.0f;

		const float MinLockedArm = (LockedMinArmLength <= LockedMaxArmLength) ? LockedMinArmLength : LockedMaxArmLength;
		const float MaxLockedArm = (LockedMinArmLength <= LockedMaxArmLength) ? LockedMaxArmLength : LockedMinArmLength;
		const float DesiredArmLength = FMath::Clamp(ToTarget.Length(), MinLockedArm, MaxLockedArm);

		SpringArm->TargetArmLength = FloatInterpTo(SpringArm->TargetArmLength, DesiredArmLength, DeltaTime, ArmInterpSpeed);
		SpringArm->TargetOffset = VectorInterpTo(SpringArm->TargetOffset, LockedTargetOffset, DeltaTime, ArmInterpSpeed);
	}

	// 실제 카메라 위치에서 락온 지점(+offset)을 향하게 한다. 카메라가 없으면 폰 위치로 폴백.
	FVector AimOrigin = OwnerPawn->GetActorLocation();
	if (UCameraComponent* Camera = OwnerPawn->GetComponentByClass<UCameraComponent>())
	{
		AimOrigin = Camera->GetWorldLocation();
	}
	FVector Dir = LockPoint - AimOrigin;
	if (Dir.Length() <= 1.0e-4f) return;
	Dir = Dir.Normalized();

	// FRotator 컨벤션: forward = (cosP*cosY, cosP*sinY, -sinP) → 역산.
	const float DesiredYaw   = atan2f(Dir.Y, Dir.X) * FMath::RadToDeg;
	const float DesiredPitch = asinf(FMath::Clamp(Dir.Z, -1.0f, 1.0f)) * FMath::RadToDeg;

	// ControlRotation 을 목표로 부드럽게 보간 — SpringArm 이 bUsePawnControlRotation 으로 따라온다.
	FRotator Rot = OwnerPawn->GetControlRotation();
	Rot.Yaw   = AngleInterpTo(Rot.Yaw,   DesiredYaw,   DeltaTime, CameraRotationInterpSpeed);
	//Rot.Pitch = AngleInterpTo(Rot.Pitch, DesiredPitch, DeltaTime, CameraRotationInterpSpeed);
	Rot.Roll  = 0.0f;
	OwnerPawn->SetControlRotation(Rot);
}

void ULockOnComponent::UpdateMarker()
{
	if (!Owner) return; 
	if (IsValidLockTarget(LockedTarget) && MarkerComponent)
	{
		FVector LockOnPoint = GetLockOnPoint(LockedTarget);
		if (FVector::DistSquared(LockOnPoint, Owner->GetActorLocation()) >= pow(BreakDistance, 2))
		{
			ClearLockOn();
			return;
		}

		MarkerComponent->SetWorldLocation(LockOnPoint);
	}
}

void ULockOnComponent::RestoreCamera(float DeltaTime)
{
	if (USpringArmComponent* SpringArm = SpringArmComponent.Get())
	{
		SpringArm->TargetArmLength = FloatInterpTo(SpringArm->TargetArmLength, NormalArmLength, DeltaTime, ArmInterpSpeed);
		SpringArm->TargetOffset = VectorInterpTo(SpringArm->TargetOffset, NormalTargetOffset, DeltaTime, ArmInterpSpeed);
	}
}
