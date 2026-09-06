#pragma once

class UBehaviorTreeAsset;
class UBehaviorTreeComponent;
class FBTNode_Base;

// =============================================================================
//  FBehaviorTreeCompiler — UBehaviorTreeAsset(정적 그래프) → FBTNode_* 런타임 트리
// =============================================================================
//  FAnimGraphCompiler 미러. Root 노드에서 링크를 따라 내려가며 Owner.MakeNode<T> 로
//  plain 런타임 노드를 생성하고 자식을 PosX 순서로 연결한다. 생성된 노드의 lifetime 은
//  Owner(UBehaviorTreeComponent)의 OwnedNodes 가 단일 소유한다 — 컴파일러는 lifetime
//  을 신경 쓰지 않는다. 반환값은 트리의 실행 루트(보통 Root 의 단일 자식)다.
//
//  nullptr 반환 — Root 노드 없음 / Root 에 자식 없음 등.
// =============================================================================
class FBehaviorTreeCompiler
{
public:
    static FBTNode_Base* Compile(const UBehaviorTreeAsset& Asset, UBehaviorTreeComponent& Owner);
};
