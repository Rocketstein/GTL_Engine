#include "AI/CombatTargetRegistry.h"

#include "Component/Combat/CombatStateComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "GameFramework/AActor.h"
#include "Object/Object.h"

#include <cfloat>
#include <cmath>

FCombatTargetRegistry& FCombatTargetRegistry::Get()
{
    static FCombatTargetRegistry Instance;
    return Instance;
}

void FCombatTargetRegistry::Register(UCombatStateComponent* Combatant)
{
    if (!Combatant)
    {
        return;
    }
    for (const TWeakObjectPtr<UCombatStateComponent>& Existing : Combatants)
    {
        if (Existing.Get() == Combatant)
        {
            return; // 중복 등록 방지
        }
    }
    Combatants.push_back(Combatant);
}

void FCombatTargetRegistry::Unregister(UCombatStateComponent* Combatant)
{
    for (auto It = Combatants.begin(); It != Combatants.end(); )
    {
        UCombatStateComponent* C = It->Get();
        if (C == Combatant || C == nullptr)
        {
            It = Combatants.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

void FCombatTargetRegistry::Prune()
{
    for (auto It = Combatants.begin(); It != Combatants.end(); )
    {
        if (!It->IsValid())
        {
            It = Combatants.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

AActor* FCombatTargetRegistry::FindNearestHostile(const UCombatStateComponent* OwnerCombat, const FVector& From, float Range)
{
    if (!OwnerCombat)
    {
        return nullptr;
    }
    Prune();

    AActor*     Best       = nullptr;
    float       BestDistSq = FLT_MAX;
    const float RangeSq    = (Range > 0.0f) ? Range * Range : FLT_MAX;

    for (const TWeakObjectPtr<UCombatStateComponent>& Weak : Combatants)
    {
        UCombatStateComponent* Candidate = Weak.Get();
        if (!Candidate || Candidate == OwnerCombat)
        {
            continue;
        }
        if (!OwnerCombat->IsHostileTo(Candidate))
        {
            continue;
        }
        AActor* CandidateActor = Candidate->GetOwner();
        if (!IsValid(CandidateActor))
        {
            continue;
        }
        if (UHealthComponent* Health = CandidateActor->GetComponentByClass<UHealthComponent>())
        {
            if (Health->IsDead())
            {
                continue;
            }
        }
        FVector Delta = CandidateActor->GetActorLocation() - From;
        Delta.Z = 0.0f; // 평면 거리 — 시야/추적 정책과 일관
        const float DistSq = Delta.X * Delta.X + Delta.Y * Delta.Y;
        if (DistSq > RangeSq)
        {
            continue;
        }
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            Best       = CandidateActor;
        }
    }
    return Best;
}

void FCombatTargetRegistry::GatherAllies(const UCombatStateComponent* OwnerCombat, const FVector& From, float Radius, TArray<AActor*>& OutAllies)
{
    OutAllies.clear();
    if (!OwnerCombat || Radius <= 0.0f)
    {
        return;
    }
    Prune();

    const float RadiusSq = Radius * Radius;
    for (const TWeakObjectPtr<UCombatStateComponent>& Weak : Combatants)
    {
        UCombatStateComponent* Candidate = Weak.Get();
        if (!Candidate || Candidate == OwnerCombat)
        {
            continue;
        }
        // 같은 편(아군)만 — 적대 관계는 제외.
        if (OwnerCombat->IsHostileTo(Candidate))
        {
            continue;
        }
        AActor* CandidateActor = Candidate->GetOwner();
        if (!IsValid(CandidateActor))
        {
            continue;
        }
        if (UHealthComponent* Health = CandidateActor->GetComponentByClass<UHealthComponent>())
        {
            if (Health->IsDead())
            {
                continue;
            }
        }
        FVector Delta = CandidateActor->GetActorLocation() - From;
        Delta.Z = 0.0f;
        const float DistSq = Delta.X * Delta.X + Delta.Y * Delta.Y;
        if (DistSq > RadiusSq)
        {
            continue;
        }
        OutAllies.push_back(CandidateActor);
    }
}

FVector FCombatTargetRegistry::ComputeSeparation(const UCombatStateComponent* OwnerCombat, const FVector& From, float Radius)
{
    if (!OwnerCombat || Radius <= 0.0f)
    {
        return FVector::ZeroVector;
    }
    Prune();

    FVector     Sum       = FVector::ZeroVector;
    const float RadiusSq  = Radius * Radius;

    for (const TWeakObjectPtr<UCombatStateComponent>& Weak : Combatants)
    {
        UCombatStateComponent* Candidate = Weak.Get();
        if (!Candidate || Candidate == OwnerCombat)
        {
            continue;
        }
        // 같은 편(아군)만 분리한다 — 적/타깃은 접근/공격 대상이므로 제외.
        if (OwnerCombat->IsHostileTo(Candidate))
        {
            continue;
        }
        AActor* CandidateActor = Candidate->GetOwner();
        if (!IsValid(CandidateActor))
        {
            continue;
        }
        if (UHealthComponent* Health = CandidateActor->GetComponentByClass<UHealthComponent>())
        {
            if (Health->IsDead())
            {
                continue;
            }
        }
        FVector Away = From - CandidateActor->GetActorLocation();
        Away.Z = 0.0f;
        const float DistSq = Away.X * Away.X + Away.Y * Away.Y;
        if (DistSq > RadiusSq || DistSq < 1e-6f)
        {
            continue;
        }
        const float Dist   = std::sqrt(DistSq);
        const float Weight = 1.0f - (Dist / Radius); // 가까울수록 1 에 가깝게
        Sum += Away.Normalized() * Weight;
    }
    return Sum;
}
