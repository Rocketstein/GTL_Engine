#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AActor;

#include "Source/Engine/Component/Character/CharacterStateMachineComponent.generated.h"

// =============================================================================
//  UCharacterStateMachineComponent — 이름 기반 상태 런타임
// =============================================================================
//  이 컴포넌트는 더 이상 Idle/Attack/Hit/Dead 같은 게임 상태 의미를 알지 않는다.
//  외부 정책(Lua Blueprint/Lua Script/Data)이 상태 이름과 실행 정의를 주입하고,
//  C++은 현재 상태, 상태 시간, locomotion 적용, animation StateId push만 담당한다.
// =============================================================================

UENUM()
enum class ELocomotionMode : uint8
{
    Locked        = 0,  // 수평 이동 정지. root motion/dash는 별도 허용.
    InputAllowed  = 1,  // 외부 입력(WASD/path-follow)이 이동을 구동 — 미개입.
    Strafe        = 2,  // MovementTarget 기준 좌우 이동 입력 생성.
    Retreat       = 3,  // MovementTarget 반대 방향 이동 입력 생성.
};

// 상태별 실행 정의. 상태의 의미와 전이 조건은 Lua Blueprint/Data가 소유한다.
USTRUCT()
struct FCharacterStateDef
{
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="State", DisplayName="State Name")
    FName StateName = FName("Idle");

    UPROPERTY(Edit, Save, Category="State", DisplayName="Locomotion", Enum=ELocomotionMode)
    ELocomotionMode Locomotion = ELocomotionMode::InputAllowed;

    UPROPERTY(Edit, Save, Category="State", DisplayName="Anim State Id", Min=0.0f, Max=64.0f, Speed=1.0f)
    int32 AnimStateId = 0;

    UPROPERTY(Edit, Save, Category="State", DisplayName="Allows Policy Exit")
    bool bAllowsPolicyExit = true;

    UPROPERTY(Edit, Save, Category="State", DisplayName="Face Target")
    bool bFaceTarget = false;
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FCharacterStateChangedSignature, class UCharacterStateMachineComponent*, FName, FName);

UCLASS()
class UCharacterStateMachineComponent : public UActorComponent
{
public:
    GENERATED_BODY()
    UCharacterStateMachineComponent()           = default;
    ~UCharacterStateMachineComponent() override = default;

    void BeginPlay() override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

    // 정책이 상태를 요청한다. 현재 상태 정의가 bAllowsPolicyExit=false면 거부한다.
    UFUNCTION(Callable, Category="Character|State")
    bool RequestState(const FName& NewState);

    // 시스템/정책이 강제로 상태를 바꾼다. 이 함수도 상태 의미는 해석하지 않는다.
    UFUNCTION(Callable, Category="Character|State")
    void ForceState(const FName& NewState);

    UFUNCTION(Callable, Category="Character|State")
    void DefineState(const FName& StateName, int32 LocomotionMode, int32 AnimStateId, bool bAllowsPolicyExit, bool bFaceTarget);

    UFUNCTION(Callable, Category="Character|State")
    void ClearStateDefinitions();

    UFUNCTION(Pure, Category="Character|State")
    bool HasStateDefinition(const FName& StateName) const;

    // 갭클로저/루트모션 없는 이동형 액션용 짧은 대시. 상태 의미와 무관한 이동 primitive다.
    UFUNCTION(Callable, Category="Character|State")
    void BeginDash(float Seconds, float Scale);

    UFUNCTION(Pure, Category="Character|State")
    FName GetState() const { return CurrentState; }

    UFUNCTION(Pure, Category="Character|State")
    FName GetPreviousState() const { return PreviousState; }

    UFUNCTION(Pure, Category="Character|State")
    int32 GetStateInt() const { return GetStateDef(CurrentState).AnimStateId; }

    UFUNCTION(Pure, Category="Character|State")
    float GetTimeInState() const { return TimeInState; }

    UFUNCTION(Pure, Category="Character|State")
    int32 GetLocomotionModeInt() const;

    UFUNCTION(Pure, Category="Character|State")
    bool IsMovementLocked() const;

    UFUNCTION(Pure, Category="Character|State")
    bool CanPolicyExitCurrentState() const;

    // Strafe/Retreat/조준이 사용할 이동 타깃. 브레인 비의존.
    UFUNCTION(Callable, Category="Character|State")
    void SetMovementTarget(AActor* InTarget) { MovementTarget = InTarget; }

    UFUNCTION(Pure, Category="Character|State")
    AActor* GetMovementTarget() const { return MovementTarget.Get(); }

    FCharacterStateChangedSignature OnStateChanged;

    UPROPERTY(Edit, Save, Category="Character|State", DisplayName="Initial State")
    FName InitialState = FName("Idle");

    UPROPERTY(Edit, Save, Category="Character|State", DisplayName="State Definitions", Type=Array)
    TArray<FCharacterStateDef> StateDefinitions;

    UPROPERTY(Edit, Save, Category="Character|State", DisplayName="Strafe Scale", Min=0.0f, Max=2.0f, Speed=0.05f)
    float StrafeScale = 0.7f;

    UPROPERTY(Edit, Save, Category="Character|State", DisplayName="Retreat Scale", Min=0.0f, Max=2.0f, Speed=0.05f)
    float RetreatScale = 0.6f;

    UPROPERTY(Edit, Save, Category="Character|State", DisplayName="Strafe Flip Period", Min=0.0f, Max=10.0f, Speed=0.05f)
    float StrafeFlipPeriod = 1.6f;

    UPROPERTY(Edit, Save, Category="Character|State", DisplayName="Face Yaw Rate", Min=0.0f, Max=3600.0f, Speed=5.0f)
    float FaceYawRate = 540.0f;

private:
    void EnsureStateDefinitions();
    FCharacterStateDef GetStateDef(const FName& StateName) const;
    void ChangeState(const FName& NewState);
    void ApplyMovement(const FCharacterStateDef& Def, float DeltaTime);
    void ApplyAnimation(const FCharacterStateDef& Def);
    void ApplyFacing(const FCharacterStateDef& Def, float DeltaTime);
    // 직접 입력 이동(Strafe/Retreat)이 맵 가장자리(navmesh 경계) 밖으로 나가지 않도록, 의도한 방향
    // 앞을 navmesh 로 미리 찍어 경계를 넘으면 navmesh 가 남아있는 가장 가까운 방향으로 틀어준다(요청 #7).
    // navmesh 가 없으면 입력을 그대로 돌려준다. 어느 방향도 안전치 않으면 ZeroVector(바깥 입력 포기).
    FVector SteerAwayFromNavEdge(class ACharacter* Character, const FVector& DesiredDir) const;

    FName CurrentState  = FName("Idle");
    FName PreviousState = FName::None;
    float TimeInState   = 0.0f;
    TWeakObjectPtr<AActor> MovementTarget;
    bool  bStrafeClockwise = true;
    float StrafeFlipTimer  = 0.0f;
    bool  bHasCachedOrient = false;
    bool  bCachedOrientToMovement = true;
    float DashRemaining    = 0.0f;
    float DashScale        = 1.0f;
};
