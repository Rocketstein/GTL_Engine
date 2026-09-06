#pragma once

class AActor;
class AEnemyCharacter;
class UAIBlackboardComponent;

// =============================================================================
//  FBTContext — 매 think-step 에 노드 트리로 전달되는 실행 컨텍스트 (plain struct)
// =============================================================================
//  보고서의 "결정=BT, 실행=C++ 파사드" 원칙을 그대로 따른다. Task 노드는 새 로직을
//  짜지 않고 Enemy(AEnemyCharacter)의 Brain_* 동사를 호출만 하며, Condition/Decorator
//  는 Blackboard 와 Brain_* 질의를 읽는다. 노드 자신은 상태 데이터를 들지 않는다.
//
//  Enemy 가 nullptr 이면(= AEnemyCharacter 가 아닌 소유자) BT 는 의미 있는 행동을 못
//  하므로 컴포넌트가 틱을 건너뛴다.
// =============================================================================
struct FBTContext
{
    AActor*                 OwnerActor = nullptr;
    AEnemyCharacter*        Enemy      = nullptr; // 파사드 캐시 (Brain_* 위임 대상)
    UAIBlackboardComponent* Blackboard = nullptr;
    float                   DeltaTime  = 0.0f;    // think-step 길이(초)
};
