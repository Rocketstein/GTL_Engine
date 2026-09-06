#pragma once

#include "AI/BehaviorTree/Runtime/BTNode.h"
#include "Object/FName.h"

// =============================================================================
//  Behavior Tree 데코레이터 — 단일 자식을 감싸 실행을 조건부로 막거나 결과를 변형
// =============================================================================
//  컴파일러가 그래프 노드에 부착된 FBTAuxNode(Decorator) 마다 자식을 한 겹씩 감싼다
//  (첫 번째로 나열된 데코레이터가 가장 바깥). observer-abort 가능한 게이트 데코레이터
//  (Blackboard / Condition)는 Selector 의 반응형 선점에 쓰인다.
// =============================================================================
class FBTDecorator_Base : public FBTNode_Base
{
public:
    void SetChild(FBTNode_Base* InChild)
    {
        Child = InChild;
        ChildList.clear();
        if (InChild)
        {
            ChildList.push_back(InChild);
        }
    }

    void Initialize(const FBTContext& Context) override
    {
        if (Child) Child->Initialize(Context);
    }
    void AddReferencedObjects(FReferenceCollector& Collector) override
    {
        if (Child) Child->AddReferencedObjects(Collector);
    }
    const TArray<FBTNode_Base*>* GetDebugChildren() const override { return &ChildList; }

protected:
    // 게이트 닫힘 → 자식 실행 안 함(진행 중이던 자식은 self-abort) → Failed.
    EBTNodeResult Tick(const FBTContext& Context) override
    {
        if (!IsGateOpen(Context))
        {
            if (Child && Child->IsEntered())
            {
                Child->ForceAbort(Context);
            }
            return EBTNodeResult::Failed;
        }
        if (!Child)
        {
            return EBTNodeResult::Failed;
        }
        return DecorateResult(Child->Execute(Context));
    }

    void Abort(const FBTContext& Context) override
    {
        if (Child && Child->IsEntered())
        {
            Child->ForceAbort(Context);
        }
    }

    // 게이트가 열린 상태에서 자식 결과를 변형(기본: 그대로).
    virtual EBTNodeResult DecorateResult(EBTNodeResult Result) const { return Result; }

    FBTNode_Base*         Child = nullptr;
    TArray<FBTNode_Base*> ChildList; // 디버그 트리 워크용(요소 0 또는 1개)
};

// 자식 결과 반전(Succeeded ↔ Failed).
class FBTDecorator_Inverter : public FBTDecorator_Base
{
public:
    const char* GetDebugName() const override { return "Inverter"; }

protected:
    EBTNodeResult DecorateResult(EBTNodeResult Result) const override
    {
        if (Result == EBTNodeResult::Succeeded) return EBTNodeResult::Failed;
        if (Result == EBTNodeResult::Failed)    return EBTNodeResult::Succeeded;
        return Result;
    }
};

// 자식이 끝나면 항상 Succeeded(InProgress 는 유지).
class FBTDecorator_ForceSuccess : public FBTDecorator_Base
{
public:
    const char* GetDebugName() const override { return "ForceSuccess"; }

protected:
    EBTNodeResult DecorateResult(EBTNodeResult Result) const override
    {
        return (Result == EBTNodeResult::InProgress) ? Result : EBTNodeResult::Succeeded;
    }
};

// 자식이 끝나면 항상 Failed(InProgress 는 유지).
class FBTDecorator_ForceFailure : public FBTDecorator_Base
{
public:
    const char* GetDebugName() const override { return "ForceFailure"; }

protected:
    EBTNodeResult DecorateResult(EBTNodeResult Result) const override
    {
        return (Result == EBTNodeResult::InProgress) ? Result : EBTNodeResult::Failed;
    }
};

// 블랙보드 bool 키 게이트. observer-abort 가능.
class FBTDecorator_Blackboard : public FBTDecorator_Base
{
public:
    FName Key;
    bool  bExpected      = true;
    bool  bObserverAbort = false;

    const char* GetDebugName() const override { return "Blackboard?"; }
    bool        IsGateOpen(const FBTContext& Context) const override;
    bool        WantsObserverAbort() const override { return bObserverAbort; }
};

// Brain_* 명명 조건 게이트(예: TargetThreatening). observer-abort 가능 — 공격 중 위협 감지→가드 전환.
class FBTDecorator_Condition : public FBTDecorator_Base
{
public:
    FName ConditionName;
    bool  bObserverAbort = false;

    const char* GetDebugName() const override { return "Condition?"; }
    bool        IsGateOpen(const FBTContext& Context) const override;
    bool        WantsObserverAbort() const override { return bObserverAbort; }
};

// 쿨다운: 자식 Success 후 N초 동안 차단(Failed).
class FBTDecorator_Cooldown : public FBTDecorator_Base
{
public:
    float CooldownSeconds = 1.0f;

    const char* GetDebugName() const override { return "Cooldown"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;

private:
    float Remaining = 0.0f;
};
