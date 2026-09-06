#include "AI/BehaviorTree/Runtime/BTTasks.h"

#include "AI/BehaviorTree/Runtime/BTContext.h"
#include "Component/AI/AIBlackboardComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/Pawn/EnemyCharacter.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

bool EvaluateBrainCondition(const FBTContext& Context, const FName& Name)
{
    AEnemyCharacter* Enemy = Context.Enemy;
    if (!Enemy)
    {
        return false;
    }
    if (Name == FName("CanSeeTarget"))      return Enemy->Brain_CanSeeTarget();
    if (Name == FName("InAttackRange"))     return Enemy->Brain_GetDistance() <= Enemy->Brain_GetAttackRange();
    if (Name == FName("TargetThreatening")) return Enemy->Brain_TargetThreatening();
    if (Name == FName("IsAlerted"))         return Enemy->Brain_IsAlerted();
    if (Name == FName("TargetInRecovery"))  return Enemy->Brain_TargetInRecovery();
    if (Name == FName("HasLineOfSight"))    return Enemy->Brain_HasLineOfSight();
    if (Name == FName("IsInProximity"))     return Enemy->Brain_IsInProximity();
    if (Name == FName("IsUnaware"))         return Enemy->Brain_IsUnaware();
    if (Name == FName("HasCombatPerception")) return (Enemy->Brain_CanSeeTarget() && Enemy->Brain_HasLineOfSight()) || Enemy->Brain_IsInProximity();
    if (Name == FName("LastAttackWasDeflected")) return Enemy->Brain_LastAttackWasDeflected();
    if (Name == FName("LastAttackConnected"))    return Enemy->Brain_LastAttackConnected();
    if (Name == FName("DeathblowReady"))         return Enemy->Brain_IsDeathblowReady();
    if (Name == FName("Phase2"))                 return Enemy->Brain_GetPhase() == 2;
    if (Name == FName("Phase3"))                 return Enemy->Brain_GetPhase() == 3;
    if (Name == FName("PhaseAtLeast2"))          return Enemy->Brain_GetPhase() >= 2;
    if (Name == FName("PhaseAtLeast3"))          return Enemy->Brain_GetPhase() >= 3;
    return false;
}

EBTNodeResult FBTCondition_CanSeeTarget::Tick(const FBTContext& Context)
{
    if (!Context.Enemy)
    {
        return EBTNodeResult::Failed;
    }
    return Context.Enemy->Brain_CanSeeTarget() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

EBTNodeResult FBTTask_Chase::Tick(const FBTContext& Context)
{
    AEnemyCharacter* Enemy = Context.Enemy;
    if (!Enemy)
    {
        return EBTNodeResult::Failed;
    }
    Enemy->Brain_Chase();
    const float Distance = Enemy->Brain_GetDistance();
    const float Range    = Enemy->Brain_GetAttackRange();
    // 사정권 안이면 추격 완료(Succeeded) → 상위 Sequence 가 Attack 단계로 진행.
    return (Distance <= Range) ? EBTNodeResult::Succeeded : EBTNodeResult::InProgress;
}

EBTNodeResult FBTTask_Attack::Tick(const FBTContext& Context)
{
    AEnemyCharacter* Enemy = Context.Enemy;
    if (!Enemy)
    {
        return EBTNodeResult::Failed;
    }
    if (Enemy->Brain_IsBusy())
    {
        // 공격 애니메이션/회복 진행 중 — 끝날 때까지 이 노드가 점유.
        return EBTNodeResult::InProgress;
    }
    Enemy->Brain_FaceTarget();
    // 선택된 공격이 없거나 재생 불가하면 Failed → 상위 Selector 가 다음 후보로 폴백.
    return Enemy->Brain_PlaySelectedAttack() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

EBTNodeResult FBTTask_Idle::Tick(const FBTContext& Context)
{
    if (Context.Enemy)
    {
        Context.Enemy->Brain_Idle();
    }
    // 폴백 행동 — 항상 점유(다른 후보가 없을 때 계속 대기).
    return EBTNodeResult::InProgress;
}

EBTNodeResult FBTTask_Strafe::Tick(const FBTContext& Context)
{
    if (!Context.Enemy)
    {
        return EBTNodeResult::Failed;
    }
    Context.Enemy->Brain_Strafe();
    return EBTNodeResult::InProgress;
}

EBTNodeResult FBTTask_Reposition::Tick(const FBTContext& Context)
{
    if (!Context.Enemy)
    {
        return EBTNodeResult::Failed;
    }
    Context.Enemy->Brain_Reposition();
    return EBTNodeResult::InProgress;
}

EBTNodeResult FBTTask_Investigate::Tick(const FBTContext& Context)
{
    if (!Context.Enemy)
    {
        return EBTNodeResult::Failed;
    }
    Context.Enemy->Brain_Investigate();
    return EBTNodeResult::InProgress;
}

EBTNodeResult FBTCondition_InAttackRange::Tick(const FBTContext& Context)
{
    if (!Context.Enemy)
    {
        return EBTNodeResult::Failed;
    }
    return (Context.Enemy->Brain_GetDistance() <= Context.Enemy->Brain_GetAttackRange())
        ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

EBTNodeResult FBTCondition_TargetThreatening::Tick(const FBTContext& Context)
{
    if (!Context.Enemy)
    {
        return EBTNodeResult::Failed;
    }
    return Context.Enemy->Brain_TargetThreatening() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

EBTNodeResult FBTCondition_IsAlerted::Tick(const FBTContext& Context)
{
    if (!Context.Enemy)
    {
        return EBTNodeResult::Failed;
    }
    return Context.Enemy->Brain_IsAlerted() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}


namespace
{
    float BTRand01()
    {
        return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    }

    float BTRangeCurve(float Distance, float MinRange, float MaxRange)
    {
        const float Span = (std::max)(1.0f, MaxRange - MinRange);
        const float T = FMath::Clamp((Distance - MinRange) / Span, 0.0f, 1.0f);
        const float OffCenter = fabsf(T - 0.5f) * 2.0f;
        return 1.0f - 0.6f * OffCenter;
    }

    float BTAngleCurve(float AbsAngle, float MaxAbsAngle)
    {
        const float Limit = (std::max)(1.0f, MaxAbsAngle);
        const float T = FMath::Clamp(AbsAngle / Limit, 0.0f, 1.0f);
        return 1.0f - 0.7f * T;
    }

    FString NormalizeTacticTag(const FName& Name)
    {
        FString Src = Name.ToString();
        FString Out;
        Out.reserve(Src.size());
        for (char C : Src)
        {
            if (C == '_' || std::isspace(static_cast<unsigned char>(C)))
            {
                continue;
            }
            Out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(C))));
        }
        return Out;
    }

    bool IsValidBTName(const FName& Name)
    {
        return Name.IsValid() && Name != FName::None && Name.ToString() != "None";
    }

    bool BTComboGate(AEnemyCharacter* Enemy, int32 AttackIndex)
    {
        if (!Enemy)
        {
            return false;
        }
        if (!Enemy->Brain_AttackRequiresPreviousAttack(AttackIndex))
        {
            return true;
        }
        const FName Required = Enemy->Brain_GetAttackRequiredPreviousAttack(AttackIndex);
        return IsValidBTName(Required) && Required == Enemy->Brain_GetLastAttackName();
    }

    float BTTacticScale(
        AEnemyCharacter* Enemy,
        const FString& Tactic,
        bool bIsGapCloser,
        bool bTargetThreatening,
        bool bTargetRecovery,
        float TargetPostureRatio,
        int32 Phase)
    {
        float Scale = 1.0f;
        const bool bBoss = Enemy && Enemy->Brain_IsBoss();
        const bool bLastConnected = Enemy && Enemy->Brain_LastAttackConnected();
        const bool bLastDeflected = Enemy && Enemy->Brain_LastAttackWasDeflected();

        if (Tactic == "opener")
        {
            const float StateTime = Enemy ? Enemy->Brain_GetStateTime() : 999.0f;
            Scale *= StateTime <= 1.5f ? 1.35f : 0.70f;
        }
        else if (Tactic == "pressure")
        {
            Scale *= bBoss ? 1.35f : 1.15f;
        }
        else if (Tactic == "combo")
        {
            Scale *= (bLastConnected || IsValidBTName(Enemy ? Enemy->Brain_GetLastAttackName() : FName::None)) ? 1.55f : 0.35f;
        }
        else if (Tactic == "gapcloser" || bIsGapCloser)
        {
            Scale *= 1.25f + 0.10f * static_cast<float>((std::max)(0, Phase - 1));
        }
        else if (Tactic == "punish")
        {
            Scale *= bTargetRecovery ? 1.80f : 0.75f;
        }
        else if (Tactic == "retreat")
        {
            Scale *= bTargetThreatening ? 1.35f : 0.70f;
        }
        else if (Tactic == "phasechange")
        {
            Scale *= bBoss && Phase >= 3 ? 1.65f : 0.60f;
        }

        if (bTargetThreatening && (Tactic == "punish" || Tactic == "retreat"))
        {
            Scale *= 1.25f;
        }
        if (TargetPostureRatio > 0.0f && TargetPostureRatio < 0.40f && (Tactic == "pressure" || Tactic == "combo"))
        {
            Scale *= 1.35f;
        }
        if (Phase >= 2 && (Tactic == "pressure" || Tactic == "gapcloser" || Tactic == "phasechange" || bIsGapCloser))
        {
            Scale *= 1.0f + 0.14f * static_cast<float>(Phase - 1);
        }
        if (Phase >= 3 && Tactic == "combo")
        {
            Scale *= 1.35f;
        }

        // 플레이어에게 탄기당한 직후에는 같은 압박을 고집하지 않고 방어/재배치 분기로 빠질 여지를 준다.
        if (bLastDeflected && (Tactic == "pressure" || Tactic == "combo" || Tactic == "opener"))
        {
            Scale *= 0.42f;
        }
        return Scale;
    }
}

EBTNodeResult FBTTask_SelectAttack::Tick(const FBTContext& Context)
{
    AEnemyCharacter* Enemy = Context.Enemy;
    if (!Enemy)
    {
        return EBTNodeResult::Failed;
    }
    if (Enemy->Brain_IsBusy())
    {
        return EBTNodeResult::InProgress;
    }

    const int32 AttackCount = Enemy->Brain_GetAttackCount();
    if (AttackCount <= 0)
    {
        return EBTNodeResult::Failed;
    }

    const float Distance = Enemy->Brain_GetDistance();
    const float AbsAngle = Enemy->Brain_GetAbsAngle();
    const int32 Phase = Enemy->Brain_GetPhase();
    const float TargetPosture = Enemy->Brain_GetTargetPostureRatio();
    const bool bTargetRecovery = Enemy->Brain_TargetInRecovery();
    const bool bTargetThreatening = Enemy->Brain_TargetThreatening();
    const bool bPerceptionReady = (Enemy->Brain_CanSeeTarget() && Enemy->Brain_HasLineOfSight()) || Enemy->Brain_IsInProximity();
    if (!bPerceptionReady)
    {
        return EBTNodeResult::Failed;
    }

    Enemy->Brain_BeginDecisionTrace();

    FName BestName = FName::None;
    float BestScore = -1.0f;

    for (int32 Index = 0; Index < AttackCount; ++Index)
    {
        if (!Enemy->Brain_CanUseAttackIndex(Index) || !BTComboGate(Enemy, Index))
        {
            continue;
        }

        const FName AttackName = Enemy->Brain_GetAttackName(Index);
        if (!IsValidBTName(AttackName))
        {
            continue;
        }

        const float BaseWeight = Enemy->Brain_GetAttackBaseWeight(Index);
        const float Priority = Enemy->Brain_GetAttackPriority(Index);
        const float MinRange = Enemy->Brain_GetAttackMinRange(Index);
        const float MaxRange = Enemy->Brain_GetAttackMaxRange(Index);
        const float MaxAbsAngle = Enemy->Brain_GetAttackMaxAbsAngle(Index);
        const float RepeatScale = FMath::Clamp(Enemy->Brain_GetAttackRepeatWeightScale(Index), 0.0f, 1.0f);
        const int32 RepeatCount = Enemy->Brain_GetRecentAttackRepeat(AttackName);
        const bool bGapCloser = Enemy->Brain_IsAttackGapCloser(Index);
        const FString Tactic = NormalizeTacticTag(Enemy->Brain_GetAttackTacticTag(Index));

        float Score = (std::max)(0.0001f, BaseWeight)
            * (std::max)(0.1f, Priority)
            * BTRangeCurve(Distance, MinRange, MaxRange)
            * BTAngleCurve(AbsAngle, MaxAbsAngle)
            * std::pow(RepeatScale, static_cast<float>((std::max)(0, RepeatCount)))
            * BTTacticScale(Enemy, Tactic, bGapCloser, bTargetThreatening, bTargetRecovery, TargetPosture, Phase);

        // 위험공격은 2페이즈부터 문법을 보여주고, 3페이즈에서는 더 과감하게 섞는다.
        const int32 PerilousType = Enemy->Brain_GetAttackPerilousType(Index);
        if (PerilousType > 0)
        {
            Score *= Phase >= 3 ? 1.20f : (Phase >= 2 ? 1.05f : 0.45f);
        }

        // 너무 멀면 일반 근접기는 자연스럽게 밀리고, 갭클로저가 선택된다.
        const float AttackRange = Enemy->Brain_GetAttackRange();
        if (!bGapCloser && Distance > AttackRange * 1.25f)
        {
            Score *= 0.45f;
        }

        Score *= 0.92f + BTRand01() * 0.16f;
        Enemy->Brain_AddDecisionCandidate(AttackName, Score);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestName = AttackName;
        }
    }

    if (IsValidBTName(BestName) && BestScore > 0.05f && Enemy->Brain_SetSelectedAttack(BestName))
    {
        Enemy->Brain_CommitDecisionTrace(BestName);
        return EBTNodeResult::Succeeded;
    }

    Enemy->Brain_CommitDecisionTrace(FName("None"));
    return EBTNodeResult::Failed;
}

EBTNodeResult FBTTask_ReactiveDeflect::Tick(const FBTContext& Context)
{
    AEnemyCharacter* Enemy = Context.Enemy;
    if (!Enemy || Enemy->Brain_IsBusy())
    {
        return EBTNodeResult::Failed;
    }
    if (!Enemy->Brain_TargetThreatening())
    {
        return EBTNodeResult::Failed;
    }
    const float Distance = Enemy->Brain_GetDistance();
    const float Range = Enemy->Brain_GetAttackRange();
    const float AbsAngle = Enemy->Brain_GetAbsAngle();
    const int32 Phase = Enemy->Brain_GetPhase();
    if (Distance > Range * 1.85f || AbsAngle > 85.0f)
    {
        return EBTNodeResult::Failed;
    }
    if (!((Enemy->Brain_CanSeeTarget() && Enemy->Brain_HasLineOfSight()) || Enemy->Brain_IsInProximity()))
    {
        return EBTNodeResult::Failed;
    }

    float Chance = Enemy->Brain_IsBoss() ? 0.64f : 0.42f;
    Chance += 0.08f * static_cast<float>((std::max)(0, Phase - 1));
    if (Enemy->Brain_LastAttackWasDeflected())
    {
        Chance += 0.18f;
    }
    if (Enemy->Brain_GetSelfHealthRatio() < 0.35f)
    {
        Chance += 0.10f;
    }
    Chance = FMath::Clamp(Chance, 0.0f, 0.92f);

    if (BTRand01() > Chance)
    {
        return EBTNodeResult::Failed;
    }

    Enemy->Brain_OpenDeflect();
    return EBTNodeResult::Succeeded;
}

EBTNodeResult FBTTask_TacticalMove::Tick(const FBTContext& Context)
{
    AEnemyCharacter* Enemy = Context.Enemy;
    if (!Enemy)
    {
        return EBTNodeResult::Failed;
    }

    if (!Enemy->Brain_AcquireTarget())
    {
        Enemy->Brain_Idle();
        return EBTNodeResult::InProgress;
    }

    if (!Enemy->Brain_IsAlerted())
    {
        const int32 AwarenessState = Enemy->Brain_GetAwarenessState();
        if (AwarenessState == 2 || AwarenessState == 4)
        {
            Enemy->Brain_Investigate();
        }
        else
        {
            Enemy->Brain_Idle();
        }
        return EBTNodeResult::InProgress;
    }

    Enemy->Brain_FaceTarget();

    const float Distance = Enemy->Brain_GetDistance();
    const float Range = (std::max)(0.1f, Enemy->Brain_GetAttackRange());
    const float HealthRatio = Enemy->Brain_GetSelfHealthRatio();
    const int32 Phase = Enemy->Brain_GetPhase();

    if (Distance > Range * 1.35f)
    {
        Enemy->Brain_Chase();
        return EBTNodeResult::InProgress;
    }

    float RetreatScore = 0.18f;
    float StrafeScore = 0.92f + 0.10f * static_cast<float>((std::max)(0, Phase - 1));

    if (Distance < Range * 0.55f)
    {
        RetreatScore += 0.55f;
    }
    if (Enemy->Brain_TargetThreatening())
    {
        RetreatScore += Phase >= 2 ? 0.55f : 0.30f;
    }
    if (Enemy->Brain_LastAttackWasDeflected())
    {
        RetreatScore += 0.60f;
    }
    if (HealthRatio < 0.35f && Phase < 3)
    {
        RetreatScore += 0.40f;
    }
    if (Enemy->Brain_LastAttackConnected() || Phase >= 3)
    {
        StrafeScore += 0.35f;
    }

    RetreatScore *= 0.90f + BTRand01() * 0.20f;
    StrafeScore *= 0.90f + BTRand01() * 0.20f;

    if (RetreatScore > StrafeScore)
    {
        Enemy->Brain_Reposition();
    }
    else
    {
        Enemy->Brain_Strafe();
    }
    return EBTNodeResult::InProgress;
}

EBTNodeResult FBTTask_BrainVerb::Tick(const FBTContext& Context)
{
    AEnemyCharacter* Enemy = Context.Enemy;
    if (!Enemy)
    {
        return EBTNodeResult::Failed;
    }
    const FName& V = VerbId;
    // 지속 이동류 — 점유(InProgress).
    if (V == FName("Chase"))       { Enemy->Brain_Chase();       return EBTNodeResult::InProgress; }
    if (V == FName("Strafe"))      { Enemy->Brain_Strafe();      return EBTNodeResult::InProgress; }
    if (V == FName("Reposition"))  { Enemy->Brain_Reposition();  return EBTNodeResult::InProgress; }
    if (V == FName("Investigate")) { Enemy->Brain_Investigate(); return EBTNodeResult::InProgress; }
    if (V == FName("Idle"))        { Enemy->Brain_Idle();        return EBTNodeResult::InProgress; }
    // 단발 — 즉시 결과.
    if (V == FName("FaceTarget"))         { Enemy->Brain_FaceTarget();  return EBTNodeResult::Succeeded; }
    if (V == FName("OpenDeflect"))        { Enemy->Brain_OpenDeflect(); return EBTNodeResult::Succeeded; }
    if (V == FName("AcquireTarget"))      return Enemy->Brain_AcquireTarget()      ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
    if (V == FName("PlaySelectedAttack")) return Enemy->Brain_PlaySelectedAttack() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
    return EBTNodeResult::Failed;
}

EBTNodeResult FBTCondition_Blackboard::Tick(const FBTContext& Context)
{
    if (!Context.Blackboard || !Key.IsValid())
    {
        return EBTNodeResult::Failed;
    }
    return (Context.Blackboard->GetBool(Key) == bExpected) ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

// ── 범용 Task ──
void FBTTask_Wait::OnEnter(const FBTContext& /*Context*/)
{
    Elapsed = 0.0f;
}

EBTNodeResult FBTTask_Wait::Tick(const FBTContext& Context)
{
    Elapsed += Context.DeltaTime;
    return (Elapsed >= Duration) ? EBTNodeResult::Succeeded : EBTNodeResult::InProgress;
}

EBTNodeResult FBTTask_SetBlackboardBool::Tick(const FBTContext& Context)
{
    if (!Context.Blackboard || !Key.IsValid())
    {
        return EBTNodeResult::Failed;
    }
    Context.Blackboard->SetBool(Key, Value);
    return EBTNodeResult::Succeeded;
}

EBTNodeResult FBTTask_SetBlackboardFloat::Tick(const FBTContext& Context)
{
    if (!Context.Blackboard || !Key.IsValid())
    {
        return EBTNodeResult::Failed;
    }
    Context.Blackboard->SetFloat(Key, Value);
    return EBTNodeResult::Succeeded;
}

EBTNodeResult FBTTask_Log::Tick(const FBTContext& /*Context*/)
{
    UE_LOG("[BT] %s", Message.c_str());
    return EBTNodeResult::Succeeded;
}
