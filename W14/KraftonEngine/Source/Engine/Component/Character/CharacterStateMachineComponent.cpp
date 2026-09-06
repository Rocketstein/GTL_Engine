#include "Component/Character/CharacterStateMachineComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/Graph/AnimGraphInstance.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Rotator.h"
#include "Navigation/NavigationSystem.h"

#include <algorithm>
#include <cmath>

namespace
{
    float NormalizeDeltaYaw(float Delta)
    {
        while (Delta > 180.0f) Delta -= 360.0f;
        while (Delta < -180.0f) Delta += 360.0f;
        return Delta;
    }

    FVector FlatDirTo(AActor* From, AActor* To)
    {
        if (!From || !To)
        {
            return FVector::ZeroVector;
        }
        FVector Delta = To->GetActorLocation() - From->GetActorLocation();
        Delta.Z = 0.0f;
        if (Delta.IsNearlyZero())
        {
            return FVector::ZeroVector;
        }
        return Delta.Normalized();
    }

    ELocomotionMode SanitizeLocomotionMode(int32 Value)
    {
        switch (Value)
        {
        case static_cast<int32>(ELocomotionMode::Locked):
            return ELocomotionMode::Locked;
        case static_cast<int32>(ELocomotionMode::InputAllowed):
            return ELocomotionMode::InputAllowed;
        case static_cast<int32>(ELocomotionMode::Strafe):
            return ELocomotionMode::Strafe;
        case static_cast<int32>(ELocomotionMode::Retreat):
            return ELocomotionMode::Retreat;
        default:
            return ELocomotionMode::InputAllowed;
        }
    }
}

void UCharacterStateMachineComponent::EnsureStateDefinitions()
{
    const FName SafeInitial = InitialState.IsValid() ? InitialState : FName("Idle");
    if (!CurrentState.IsValid())
    {
        CurrentState = SafeInitial;
    }
    if (!HasStateDefinition(SafeInitial))
    {
        FCharacterStateDef Def;
        Def.StateName = SafeInitial;
        Def.Locomotion = ELocomotionMode::InputAllowed;
        Def.AnimStateId = 0;
        Def.bAllowsPolicyExit = true;
        Def.bFaceTarget = false;
        StateDefinitions.push_back(Def);
    }
}

FCharacterStateDef UCharacterStateMachineComponent::GetStateDef(const FName& StateName) const
{
    for (const FCharacterStateDef& Def : StateDefinitions)
    {
        if (Def.StateName == StateName)
        {
            return Def;
        }
    }

    FCharacterStateDef NeutralDef;
    NeutralDef.StateName = StateName.IsValid() ? StateName : InitialState;
    NeutralDef.Locomotion = ELocomotionMode::InputAllowed;
    NeutralDef.AnimStateId = 0;
    NeutralDef.bAllowsPolicyExit = true;
    NeutralDef.bFaceTarget = false;
    return NeutralDef;
}

int32 UCharacterStateMachineComponent::GetLocomotionModeInt() const
{
    return static_cast<int32>(GetStateDef(CurrentState).Locomotion);
}

bool UCharacterStateMachineComponent::IsMovementLocked() const
{
    return GetStateDef(CurrentState).Locomotion == ELocomotionMode::Locked;
}

bool UCharacterStateMachineComponent::CanPolicyExitCurrentState() const
{
    return GetStateDef(CurrentState).bAllowsPolicyExit;
}

bool UCharacterStateMachineComponent::HasStateDefinition(const FName& StateName) const
{
    for (const FCharacterStateDef& Def : StateDefinitions)
    {
        if (Def.StateName == StateName)
        {
            return true;
        }
    }
    return false;
}

void UCharacterStateMachineComponent::DefineState(const FName& StateName, int32 LocomotionMode, int32 AnimStateId, bool bAllowsPolicyExit, bool bFaceTarget)
{
    if (!StateName.IsValid())
    {
        return;
    }
    FCharacterStateDef NewDef;
    NewDef.StateName = StateName;
    NewDef.Locomotion = SanitizeLocomotionMode(LocomotionMode);
    NewDef.AnimStateId = AnimStateId;
    NewDef.bAllowsPolicyExit = bAllowsPolicyExit;
    NewDef.bFaceTarget = bFaceTarget;

    for (FCharacterStateDef& Def : StateDefinitions)
    {
        if (Def.StateName == StateName)
        {
            Def = NewDef;
            return;
        }
    }
    StateDefinitions.push_back(NewDef);
}

void UCharacterStateMachineComponent::ClearStateDefinitions()
{
    StateDefinitions.clear();
}

void UCharacterStateMachineComponent::BeginPlay()
{
    UActorComponent::BeginPlay();
    EnsureStateDefinitions();
    CurrentState = InitialState.IsValid() ? InitialState : FName("Idle");
    PreviousState = FName::None;
    TimeInState = 0.0f;

    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            bCachedOrientToMovement = Movement->bOrientRotationToMovement;
            bHasCachedOrient        = true;
        }
    }
}

void UCharacterStateMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    TimeInState     += DeltaTime;
    StrafeFlipTimer += DeltaTime;
    if (StrafeFlipPeriod > 0.0f && StrafeFlipTimer >= StrafeFlipPeriod)
    {
        bStrafeClockwise = !bStrafeClockwise;
        StrafeFlipTimer  = 0.0f;
    }
    if (DashRemaining > 0.0f)
    {
        DashRemaining = (std::max)(0.0f, DashRemaining - DeltaTime);
    }

    const FCharacterStateDef Def = GetStateDef(CurrentState);
    ApplyMovement(Def, DeltaTime);
    ApplyFacing(Def, DeltaTime);
    ApplyAnimation(Def);
}

bool UCharacterStateMachineComponent::RequestState(const FName& NewState)
{
    if (!NewState.IsValid())
    {
        return false;
    }
    const FCharacterStateDef Def = GetStateDef(CurrentState);
    if (!Def.bAllowsPolicyExit && CurrentState != NewState)
    {
        return false;
    }
    if (CurrentState == NewState)
    {
        return true;
    }
    ChangeState(NewState);
    return true;
}

void UCharacterStateMachineComponent::ForceState(const FName& NewState)
{
    if (NewState.IsValid() && CurrentState != NewState)
    {
        ChangeState(NewState);
    }
}

void UCharacterStateMachineComponent::ChangeState(const FName& NewState)
{
    PreviousState = CurrentState;
    CurrentState  = NewState;
    TimeInState   = 0.0f;
    OnStateChanged.Broadcast(this, PreviousState, NewState);
}

void UCharacterStateMachineComponent::BeginDash(float Seconds, float Scale)
{
    DashRemaining = (std::max)(0.0f, Seconds);
    DashScale     = Scale;
}

void UCharacterStateMachineComponent::ApplyMovement(const FCharacterStateDef& Def, float /*DeltaTime*/)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        return;
    }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Movement)
    {
        return;
    }
    AActor* Target = MovementTarget.Get();

    switch (Def.Locomotion)
    {
    case ELocomotionMode::Locked:
        if (DashRemaining > 0.0f && Target)
        {
            const FVector Dir = FlatDirTo(Character, Target);
            if (!Dir.IsNearlyZero())
            {
                Character->AddMovementInput(Dir, DashScale);
            }
        }
        else if (!Movement->HasPendingRootMotion() && !Movement->IsFalling() && !Movement->HasPendingExternalImpulse())
        {
            // 지상 Locked(공격 등)에서만 수평 드리프트를 멈춘다.
            // 공중(백점프/넉백 임펄스 비행) 중에 수평 속도를 0으로 만들면 임펄스가 즉시 지워져
            // "제자리 점프"가 난다 → Falling 중엔 보존한다.
            Movement->ClearInputVector();
            Movement->StopHorizontalMovementImmediately();
        }
        break;
    case ELocomotionMode::InputAllowed:
        break;
    case ELocomotionMode::Strafe:
        if (Target)
        {
            const FVector Dir = FlatDirTo(Character, Target);
            if (!Dir.IsNearlyZero())
            {
                FVector Perp(-Dir.Y, Dir.X, 0.0f);
                if (!bStrafeClockwise)
                {
                    Perp = Perp * -1.0f;
                }
                // 가장자리로 도는 입력이면 navmesh 안쪽으로 틀어준다(요청 #7).
                const FVector Safe = SteerAwayFromNavEdge(Character, Perp.Normalized());
                if (!Safe.IsNearlyZero())
                {
                    Character->AddMovementInput(Safe, StrafeScale);
                }
            }
        }
        break;
    case ELocomotionMode::Retreat:
        if (Target)
        {
            const FVector Dir = FlatDirTo(Character, Target);
            if (!Dir.IsNearlyZero())
            {
                // 타깃 반대로 물러서되 맵 밖/낭떠러지로 후퇴하지 않게 navmesh 경계를 회피한다(요청 #7).
                const FVector Safe = SteerAwayFromNavEdge(Character, Dir * -1.0f);
                if (!Safe.IsNearlyZero())
                {
                    Character->AddMovementInput(Safe, RetreatScale);
                }
            }
        }
        break;
    }
}

FVector UCharacterStateMachineComponent::SteerAwayFromNavEdge(ACharacter* Character, const FVector& DesiredDir) const
{
    if (!Character || DesiredDir.IsNearlyZero())
    {
        return DesiredDir;
    }
    UWorld* World = Character->GetWorld();
    UNavigationSystem* Nav = World ? World->GetNavigationSystem() : nullptr;
    if (!Nav || !Nav->HasBuiltNavigationData())
    {
        return DesiredDir;   // navmesh 가 없으면 회피하지 않는다(원래 동작 유지).
    }

    FNavAgentProperties Agent;
    if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
    {
        Agent.Radius = (std::max)(0.1f, Capsule->GetScaledCapsuleRadius());
        Agent.Height = (std::max)(Agent.Radius * 2.0f, Capsule->GetScaledCapsuleHalfHeight() * 2.0f);
    }

    const FVector Origin = Character->GetActorLocation();
    // 한 틱 이동량보다 넉넉히 앞을 본다(거대 보스일수록 캡슐 반경↑ → 더 멀리 본다). 너무 길면 작은
    // 투기장에서도 과민 반응하니 캡슐 크기에 비례시킨다.
    const float LookAhead = (std::max)(1.5f, Agent.Radius * 2.0f + 1.0f);
    const float SnapTol   = 1.20f;   // 가장자리 너머 허용오차(FindStableLeapDirection 과 동일한 양자화 흡수).

    auto OnNav = [&](const FVector& Dir) -> bool
    {
        const FVector Sample(Origin.X + Dir.X * LookAhead, Origin.Y + Dir.Y * LookAhead, Origin.Z);
        FVector Projected;
        if (!Nav->ProjectPointToNavigation(Sample, Projected, Agent))
        {
            return false;
        }
        const float DX = Projected.X - Sample.X;
        const float DY = Projected.Y - Sample.Y;
        return (DX * DX + DY * DY) <= SnapTol * SnapTol;   // 가장 가까운 walkable 셀이 코앞 = 바닥 있음.
    };

    if (OnNav(DesiredDir))
    {
        return DesiredDir;   // 앞이 navmesh 위 → 의도대로 이동.
    }

    // 가장자리로 향함 → 의도에 가장 가까우면서 navmesh 가 남아있는 방향을 부채꼴로 탐색(작은 각도 우선).
    const float AnglesDeg[] = { 25.0f, -25.0f, 45.0f, -45.0f, 70.0f, -70.0f, 90.0f, -90.0f };
    for (float AngleDeg : AnglesDeg)
    {
        const float Rad = AngleDeg * FMath::DegToRad;
        const float CosA = cosf(Rad);
        const float SinA = sinf(Rad);
        FVector Rotated(DesiredDir.X * CosA - DesiredDir.Y * SinA,
                        DesiredDir.X * SinA + DesiredDir.Y * CosA, 0.0f);
        if (Rotated.IsNearlyZero())
        {
            continue;
        }
        Rotated = Rotated.Normalized();
        if (OnNav(Rotated))
        {
            return Rotated;
        }
    }

    // 어느 방향도 안전하지 않음(코너에 깊이 몰림) → 바깥으로 미는 입력을 포기(제자리 유지, 강제 추락 방지).
    return FVector::ZeroVector;
}

void UCharacterStateMachineComponent::ApplyFacing(const FCharacterStateDef& Def, float DeltaTime)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        return;
    }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    AActor* Target = MovementTarget.Get();

    if (Def.bFaceTarget && Target)
    {
        if (Movement)
        {
            Movement->bOrientRotationToMovement = false;
        }
        const FVector Dir = FlatDirTo(Character, Target);
        if (!Dir.IsNearlyZero())
        {
            FRotator Rotation = Character->GetActorRotation();
            const float TargetYaw = atan2f(Dir.Y, Dir.X) * FMath::RadToDeg;
            float DeltaYaw = NormalizeDeltaYaw(TargetYaw - Rotation.Yaw);
            const float MaxStep = FaceYawRate * DeltaTime;
            DeltaYaw = FMath::Clamp(DeltaYaw, -MaxStep, MaxStep);
            Rotation.Yaw += DeltaYaw;
            Character->SetActorRotation(Rotation);
        }
    }
    else if (Movement && bHasCachedOrient)
    {
        Movement->bOrientRotationToMovement = bCachedOrientToMovement;
    }
}

void UCharacterStateMachineComponent::ApplyAnimation(const FCharacterStateDef& Def)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        return;
    }
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    if (UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(Anim))
    {
        Graph->SetGraphVariableInt(FName("StateId"), Def.AnimStateId);
    }
}
