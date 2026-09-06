#pragma once

#include "Core/Types/CoreTypes.h"

// =============================================================================
//  EBTNodeResult — Behavior Tree 런타임 노드의 한 틱 실행 결과
// =============================================================================
//  UE 의 EBTNodeResult 대응. Composite(Selector/Sequence)는 자식이 낸 이 값으로
//  진행/성공/실패/중단을 결정한다. InProgress 는 "여러 think-step 에 걸쳐 진행 중"을
//  뜻하며, 이게 BT 가 FSM 보다 지속 행동(이동/공격 애니메이션/대기)을 자연스럽게
//  표현하는 핵심이다.
// =============================================================================
enum class EBTNodeResult : uint8
{
    Succeeded,  // 이 노드의 행동/판정이 성공
    Failed,     // 실패
    InProgress, // 아직 진행 중 — 다음 think-step 에 이어서 Tick
    Aborted     // 상위 우선순위 선점 등으로 강제 중단됨
};
