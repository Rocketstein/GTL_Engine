#pragma once

#include "Component/ActorComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Core/Delegate.h"

#include "Source/Engine/Component/Combat/CombatMoveComponent.generated.h"

// =============================================================================
//  UCombatMoveComponent — 공격 프레임 데이터 타임라인 (엔진 코어 메커니즘, Phase 2)
// =============================================================================
//  보고서 "Combat Frame Data / Animation Timeline Scheduler". 공격이 시작되면
//  startup→active→recovery 프레임 타임라인을 60Hz 로 진행시키며:
//   - 위험표식(perilous cue)을 선딜 내 특정 프레임에 켠다(텔레그래프).
//   - active 동안 소유자 CombatState 에 위험공격 종류를 표시한다(방어자 대응 근거).
//   - 현재 phase/frame 을 노출해 AI 디버거가 프레임 데이터 타임라인을 그린다.
//
//  실제 타격/회복은 기존 AEnemyCharacter::UpdateAttackExecution 이 담당하고, 이
//  컴포넌트는 그와 병행하는 "프레임 데이터 관측 + 위험표식" 계층이다(비파괴).
// =============================================================================

DECLARE_MULTICAST_DELEGATE_TwoParams(FCombatPerilousCueSignature, class UCombatMoveComponent*, EPerilousType);

UCLASS()
class UCombatMoveComponent : public UActorComponent
{
public:
    GENERATED_BODY()
    UCombatMoveComponent()           = default;
    ~UCombatMoveComponent() override = default;

    // 한 공격의 프레임 타임라인을 시작한다.
    UFUNCTION(Callable, Category="Combat|Move")
    void BeginMove(EPerilousType InPerilousType, int32 Startup, int32 Active, int32 Recovery, int32 CueFrame, bool bCanDeflect);

    UFUNCTION(Callable, Category="Combat|Move")
    void EndMove();

    // 매 프레임 호출(AEnemyCharacter::Tick). 60Hz 프레임으로 타임라인 진행.
    void TickMove(float DeltaSeconds);

    UFUNCTION(Pure, Category="Combat|Move")
    bool IsMoveActive() const { return bActive; }
    UFUNCTION(Pure, Category="Combat|Move")
    int32 GetCurrentFrame() const { return CurrentFrame; }
    UFUNCTION(Pure, Category="Combat|Move")
    int32 GetTotalFrames() const { return StartupFrames + ActiveFrames + RecoveryFrames; }
    UFUNCTION(Pure, Category="Combat|Move")
    int32 GetPhaseInt() const { return static_cast<int32>(GetPhase()); }
    UFUNCTION(Pure, Category="Combat|Move")
    int32 GetPerilousTypeInt() const { return static_cast<int32>(PerilousType); }
    UFUNCTION(Pure, Category="Combat|Move")
    bool IsInRecovery() const { return bActive && GetPhase() == ECombatMovePhase::Recovery; }
    UFUNCTION(Pure, Category="Combat|Move")
    bool CanBeDeflected() const { return bCanBeDeflected; }

    ECombatMovePhase GetPhase() const;
    EPerilousType    GetPerilousType() const { return PerilousType; }
    int32            GetStartupFrames() const { return StartupFrames; }
    int32            GetActiveFrames() const { return ActiveFrames; }
    int32            GetRecoveryFrames() const { return RecoveryFrames; }

    FCombatPerilousCueSignature OnPerilousCue;

private:
    bool          bActive        = false;
    bool          bCanBeDeflected = true;
    bool          bPerilousCued  = false;
    EPerilousType PerilousType   = EPerilousType::None;
    int32         StartupFrames  = 0;
    int32         ActiveFrames   = 0;
    int32         RecoveryFrames = 0;
    int32         CueFrame       = 0;
    int32         CurrentFrame   = 0;
    float         ElapsedSeconds = 0.0f;
};
