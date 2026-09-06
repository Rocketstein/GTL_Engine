#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AActor;
class UCombatStateComponent;

// =============================================================================
//  FCombatTargetRegistry — 전투원 등록소 (엔진 코어 메커니즘)
// =============================================================================
//  타깃 획득이 매 think 마다 World 전체 Actor 를 순회(O(N*M))하던 문제를 없앤다.
//  CombatState 를 가진 액터만 등록되어, 적대 타깃 질의가 "전투원 수"에만 비례한다.
//  team 은 컴포넌트에서 live 로 읽으므로 team 변경 시 재등록이 필요 없다.
//  프로세스 싱글턴, weak ptr 백킹이라 파괴된 전투원은 안전하게 건너뛴다.
// =============================================================================
class FCombatTargetRegistry
{
public:
    static FCombatTargetRegistry& Get();

    void Register(UCombatStateComponent* Combatant);
    void Unregister(UCombatStateComponent* Combatant);

    // From 위치 기준 Range(평면) 안에서 OwnerCombat 과 적대하는 가장 가까운 액터.
    AActor* FindNearestHostile(const UCombatStateComponent* OwnerCombat, const FVector& From, float Range);

    // 같은 편 아군과 겹치지 않도록 밀어내는 분리(separation) 벡터(평면). 가까울수록 강하다.
    // 반환은 정규화하지 않은 합 — 호출측이 방향+세기로 쓴다.
    FVector ComputeSeparation(const UCombatStateComponent* OwnerCombat, const FVector& From, float Radius);

    // From 기준 Radius(평면) 안의 살아 있는 같은 편 아군 액터를 모은다(자신 제외).
    // 경계 전파(보고서 2군: 동료 경계 전파)용 — 한 명이 적을 발견하면 주변 아군에게 알린다.
    void GatherAllies(const UCombatStateComponent* OwnerCombat, const FVector& From, float Radius, TArray<AActor*>& OutAllies);

    int32 GetCombatantCount() const { return static_cast<int32>(Combatants.size()); }
    void  Reset() { Combatants.clear(); }

private:
    FCombatTargetRegistry() = default;
    void Prune();

    TArray<TWeakObjectPtr<UCombatStateComponent>> Combatants;
};
