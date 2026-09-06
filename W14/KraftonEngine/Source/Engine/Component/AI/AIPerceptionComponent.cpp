#include "Component/AI/AIPerceptionComponent.h"

#include "Component/AI/AIBlackboardComponent.h"
#include "Component/AI/AwarenessComponent.h"
#include "Component/AI/EnemyAIBrainComponent.h"
#include "Component/AI/PhaseComponent.h"
#include "Component/Combat/CombatStateComponent.h"
#include "Component/Combat/CombatMoveComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

#include <cmath>

namespace
{
    const FName Key_HasTarget    = FName("HasTarget");
    const FName Key_CanSee       = FName("CanSee");
    const FName Key_Distance     = FName("Distance");
    const FName Key_Angle        = FName("Angle");
    const FName Key_AbsAngle     = FName("AbsAngle");
    const FName Key_SelfHealth   = FName("SelfHealth");
    const FName Key_SelfPosture  = FName("SelfPosture");
    const FName Key_TargetHealth = FName("TargetHealth");
    const FName Key_TargetPosture= FName("TargetPosture");
    const FName Key_TargetThreat = FName("TargetThreat");
    const FName Key_Phase        = FName("Phase");
    const FName Key_TargetPerilous   = FName("TargetPerilous");
    const FName Key_TargetInRecovery = FName("TargetInRecovery");
    const FName Key_TargetMovePhase  = FName("TargetMovePhase");
    const FName Key_Distance3D        = FName("Distance3D");
    const FName Key_VerticalDelta     = FName("VerticalDelta");
    const FName Key_HasLOS            = FName("HasLOS");
    const FName Key_InProximity       = FName("InProximity");
    const FName Key_LastSeenValid     = FName("LastSeenValid");
}

void UAIPerceptionComponent::AgeStimuli()
{
    // 노화/만료는 RecordStimulus 가 아니라 여기서 — 아무것도 감지하지 않는 시간에도
    // stimulus 가 정상적으로 늙어 만료된다(stale stimulus 버그 수정).
    for (auto It = Stimuli.begin(); It != Stimuli.end(); )
    {
        ++It->AgeTicks;
        if (It->AgeTicks > StimulusMemoryTicks)
        {
            It = Stimuli.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

bool UAIPerceptionComponent::ComputeLineOfSight(AActor* Target)
{
    AActor* Owner = GetOwner();
    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    if (!Owner || !World || !Target)
    {
        return false;
    }
    const FVector Eye      = Owner->GetActorLocation() + FVector(0.0f, 0.0f, EyeHeight);
    const FVector TargetEye = Target->GetActorLocation() + FVector(0.0f, 0.0f, EyeHeight * 0.5f);
    FVector Delta = TargetEye - Eye;
    const float Dist = Delta.Length();
    if (Dist < 0.01f)
    {
        return true;
    }
    const FVector Dir = Delta.Normalized();
    FHitResult Hit;
    // static 지오메트리(벽)만 검사 — 타깃 사이에 벽이 있으면 가시선 차단. owner 무시.
    const bool bBlocked = World->PhysicsRaycast(Eye, Dir, Dist, Hit, ECollisionChannel::WorldStatic, Owner);
    return !bBlocked;
}

void UAIPerceptionComponent::RecordStimulus(EAISenseType Type, AActor* Source, const FVector& Location, float Strength)
{
    FAISenseStimulus Stim;
    Stim.Type     = Type;
    Stim.Source   = Source;
    Stim.Location = Location;
    Stim.Strength = Strength;
    Stim.AgeTicks = 0;
    Stimuli.insert(Stimuli.begin(), Stim);

    // 디버거 가독성을 위해 최근 16개만 유지.
    while (static_cast<int32>(Stimuli.size()) > 16)
    {
        Stimuli.pop_back();
    }
}

void UAIPerceptionComponent::RecordHearing(AActor* Source, const FVector& Location, float Strength)
{
    RecordHearingClassified(Source, Location, Strength, static_cast<int32>(EAISoundClass::Normal));
}

void UAIPerceptionComponent::RecordHearingClassified(AActor* Source, const FVector& Location, float Strength, int32 SoundClass)
{
    if (Strength <= 0.0f)
    {
        return;
    }
    RecordStimulus(EAISenseType::Hearing, Source, Location, Strength);
    if (AActor* OwnerActor = GetOwner())
    {
        if (UAwarenessComponent* Awareness = OwnerActor->GetComponentByClass<UAwarenessComponent>())
        {
            Awareness->ReportNoiseClassified(Location, Strength, SoundClass);
        }
    }
}

void UAIPerceptionComponent::UpdateSenses()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    AgeStimuli(); // 감지 여부와 무관하게 매 호출 노화 — stale stimulus 방지
    UAIBlackboardComponent* BB    = Owner->GetComponentByClass<UAIBlackboardComponent>();
    UEnemyAIBrainComponent* Brain = Owner->GetComponentByClass<UEnemyAIBrainComponent>();
    if (!BB || !Brain)
    {
        return;
    }

    // ── 자기 상태 ──
    float SelfHealth  = 1.0f;
    float SelfPosture = 1.0f;
    if (UHealthComponent* H = Owner->GetComponentByClass<UHealthComponent>())
    {
        SelfHealth = H->GetHealthRatio();
    }
    if (UCombatStateComponent* C = Owner->GetComponentByClass<UCombatStateComponent>())
    {
        SelfPosture = C->GetPoiseRatio();
    }
    BB->SetFloat(Key_SelfHealth, SelfHealth);
    BB->SetFloat(Key_SelfPosture, SelfPosture);

    int32 Phase = 1;
    if (UPhaseComponent* P = Owner->GetComponentByClass<UPhaseComponent>())
    {
        Phase = P->GetCurrentPhase();
    }
    BB->SetFloat(Key_Phase, static_cast<float>(Phase));

    // ── 타깃 ──
    AActor*    TargetActor = Brain->GetTarget();
    const bool bHasTarget  = Brain->HasValidTarget();
    BB->SetBool(Key_HasTarget, bHasTarget);
    BB->SetTarget(TargetActor);

    if (!bHasTarget || !TargetActor)
    {
        bCanSeeTarget   = false;
        bHasLineOfSight = false;
        BB->SetBool(Key_CanSee, false);
        BB->SetBool(Key_HasLOS, false);
        BB->SetBool(Key_InProximity, false);
        BB->SetBool(Key_LastSeenValid, false);
        BB->SetFloat(Key_Distance, 9999.0f);
        BB->SetFloat(Key_Distance3D, 9999.0f);
        return;
    }

    // 추적/공격 range 판단은 flat(XY) 거리, 타깃 상실/높이 판단은 3D/수직 거리로 분리.
    const float FlatDist  = Brain->GetFlatDistanceToTarget();
    const float Dist3D    = Brain->GetDistanceToTarget();
    const float VertDelta = Brain->GetVerticalDeltaToTarget();
    const float Angle     = Brain->GetAngleToTarget();
    const float AbsAngle  = std::fabs(Angle);
    BB->SetFloat(Key_Distance, FlatDist);
    BB->SetFloat(Key_Distance3D, Dist3D);
    BB->SetFloat(Key_VerticalDelta, VertDelta);
    BB->SetFloat(Key_Angle, Angle);
    BB->SetFloat(Key_AbsAngle, AbsAngle);

    float TargetHealth  = 1.0f;
    float TargetPosture = 1.0f;
    float TargetThreat  = 0.0f;
    if (UHealthComponent* TH = TargetActor->GetComponentByClass<UHealthComponent>())
    {
        TargetHealth = TH->GetHealthRatio();
    }
    float TargetPerilous = 0.0f;
    if (UCombatStateComponent* TC = TargetActor->GetComponentByClass<UCombatStateComponent>())
    {
        TargetPosture = TC->GetPoiseRatio();
        TargetThreat  = TC->GetAttackThreatRemaining();
        TargetPerilous = static_cast<float>(static_cast<int32>(TC->GetActivePerilousType()));
    }
    BB->SetFloat(Key_TargetHealth, TargetHealth);
    BB->SetFloat(Key_TargetPosture, TargetPosture);
    BB->SetFloat(Key_TargetThreat, TargetThreat);

    // ── 프레임 데이터: 타깃이 후딜 중이면 punish 기회 (Phase 2) ──
    bool  TargetInRecovery = false;
    float TargetMovePhase  = 0.0f;
    if (UCombatMoveComponent* TM = TargetActor->GetComponentByClass<UCombatMoveComponent>())
    {
        TargetInRecovery = TM->IsInRecovery();
        TargetMovePhase  = static_cast<float>(TM->GetPhaseInt());
    }
    BB->SetFloat(Key_TargetPerilous, TargetPerilous);
    BB->SetBool(Key_TargetInRecovery, TargetInRecovery);
    BB->SetFloat(Key_TargetMovePhase, TargetMovePhase);

    // ── 시야 콘: 거리(flat) + FOV + 가시선(LOS) ──
    const bool bInRange = FlatDist <= SightRange;
    const bool bInFov   = AbsAngle <= (FieldOfViewDegrees * 0.5f);
    bHasLineOfSight     = (!bRequireLineOfSight) || ComputeLineOfSight(TargetActor);
    const bool bInProximity = FlatDist <= ProximityRange;
    // FOV 콘은 "최초 발견"용이다. 이미 교전(Alert) 중인 타깃은 측면/뒤로 돌아 콘을 벗어나도 FOV 로 잃지
    // 않는다 — 사거리 안이고 벽에 가리지 않으면(LOS) 계속 추적한다. FOV 로만 잃게 두면, 리치 긴 큰 보스가
    // ProximityRange 밖에서 교전할 때 플레이어가 살짝 돌기만 해도 CanSee 가 꺼져 Searching 으로 빠진다.
    // ProximityRange 안(근접)이면 고저차 LOS 미세 차단도 무시한다(교전 중 타깃은 잃지 않는다).
    UAwarenessComponent* AwarenessForSight = Owner->GetComponentByClass<UAwarenessComponent>();
    const bool bEngaged = AwarenessForSight && AwarenessForSight->IsInCombat();
    const bool bVisible = bInRange && (bInProximity || ((bEngaged || bInFov) && bHasLineOfSight));
    bCanSeeTarget       = bVisible;
    BB->SetBool(Key_CanSee, bVisible);
    BB->SetBool(Key_HasLOS, bHasLineOfSight);
    BB->SetBool(Key_InProximity, bInProximity);

    if (bVisible)
    {
        const FVector TargetLoc = TargetActor->GetActorLocation();
        BB->SetLastSeenLocation(TargetLoc);
        BB->SetBool(Key_LastSeenValid, true);
        RecordStimulus(EAISenseType::Sight, TargetActor, TargetLoc, 1.0f);
    }
    if (bInProximity)
    {
        RecordStimulus(EAISenseType::Proximity, TargetActor, TargetActor->GetActorLocation(), 1.0f);
    }
}
