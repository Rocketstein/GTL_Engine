#include "Component/Combat/CombatMoveComponent.h"

#include "Component/Combat/CombatStateComponent.h"
#include "GameFramework/AActor.h"

namespace
{
    constexpr float kFrameSeconds = 1.0f / 60.0f;
}

void UCombatMoveComponent::BeginMove(EPerilousType InPerilousType, int32 Startup, int32 Active, int32 Recovery, int32 CueFrameIn, bool bCanDeflect)
{
    PerilousType    = InPerilousType;
    StartupFrames   = Startup   > 0 ? Startup  : 0;
    ActiveFrames    = Active    > 0 ? Active   : 1;
    RecoveryFrames  = Recovery  > 0 ? Recovery : 0;
    CueFrame        = CueFrameIn >= 0 ? CueFrameIn : 0;
    bCanBeDeflected = bCanDeflect;
    CurrentFrame    = 0;
    ElapsedSeconds  = 0.0f;
    bActive         = true;
    bPerilousCued   = false;
}

void UCombatMoveComponent::EndMove()
{
    bActive       = false;
    bPerilousCued = false;
    CurrentFrame  = 0;
    ElapsedSeconds = 0.0f;
    PerilousType  = EPerilousType::None;
}

ECombatMovePhase UCombatMoveComponent::GetPhase() const
{
    if (!bActive)
    {
        return ECombatMovePhase::Inactive;
    }
    if (CurrentFrame < StartupFrames)
    {
        return ECombatMovePhase::Startup;
    }
    if (CurrentFrame < StartupFrames + ActiveFrames)
    {
        return ECombatMovePhase::Active;
    }
    return ECombatMovePhase::Recovery;
}

void UCombatMoveComponent::TickMove(float DeltaSeconds)
{
    if (!bActive || DeltaSeconds <= 0.0f)
    {
        return;
    }

    ElapsedSeconds += DeltaSeconds;
    CurrentFrame = static_cast<int32>(ElapsedSeconds / kFrameSeconds);

    // 위험표식: 선딜 내 CueFrame 도달 시 1회. 위험공격이면 소유자 CombatState 에
    // active 종료까지 위험 종류를 표시해 방어자가 종류별 대응을 잡게 한다.
    if (!bPerilousCued && PerilousType != EPerilousType::None && CurrentFrame >= CueFrame)
    {
        bPerilousCued = true;
        OnPerilousCue.Broadcast(this, PerilousType);
        if (AActor* Owner = GetOwner())
        {
            if (UCombatStateComponent* Combat = Owner->GetComponentByClass<UCombatStateComponent>())
            {
                const int32 FramesUntilActiveEnd = (StartupFrames + ActiveFrames) - CurrentFrame;
                const float Duration = (FramesUntilActiveEnd > 0 ? FramesUntilActiveEnd : ActiveFrames) * kFrameSeconds;
                Combat->MarkPerilous(PerilousType, Duration);
            }
        }
    }

    if (CurrentFrame >= GetTotalFrames())
    {
        EndMove();
    }
}
