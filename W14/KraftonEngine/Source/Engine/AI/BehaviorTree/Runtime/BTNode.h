#pragma once

#include "Core/Types/CoreTypes.h"
#include "AI/BehaviorTree/Runtime/BTNodeResult.h"

struct FBTContext;
class FReferenceCollector;

// =============================================================================
//  FBTNode_Base — 모든 BT 런타임 노드의 베이스 (plain 클래스, 비-UObject)
// =============================================================================
//  UBehaviorTreeComponent 가 TArray<unique_ptr> 로 단일 소유하고, 부모-자식 참조는
//  raw pointer 다 (= FAnimNode_Base + UAnimInstance::OwnedNodes 패턴). reflection/Outer
//  비용 없이 매 think-step 수십 번 평가해도 안전하다.
//
//  라이프사이클 (부모/컴포넌트가 Execute/ForceAbort 를 통해 구동):
//    Initialize(ctx) — 트리 build 직후 1회(재귀). 파생이 자식 Initialize 를 부른다.
//    OnEnter(ctx)    — 이 노드가 (재)활성화되어 첫 Tick 직전. 상태 리셋.
//    Tick(ctx)       — 매 think-step. InProgress 면 다음 step 에 OnEnter 없이 이어서.
//    OnExit(result)  — InProgress 가 아닌 최종 결과를 낸 직후. 정리.
//    Abort(ctx)      — 외부 선점으로 강제 중단(Phase 4 observer-abort 가 사용).
//
//  Execute() 가 enter/exit/InProgress 부기를 캡슐화하므로, Composite 와 컴포넌트는
//  child->Execute(ctx) 와 child->ForceAbort(ctx) 만 호출하면 된다.
// =============================================================================
class FBTNode_Base
{
public:
    virtual ~FBTNode_Base() = default;

    // 트리 build 직후 1회. 파생 Composite 가 자식 Initialize 를 재귀 호출한다.
    virtual void Initialize(const FBTContext& Context) { (void)Context; }

    // 부모/컴포넌트가 호출하는 진입점. enter→tick→(exit) 부기 포함.
    EBTNodeResult Execute(const FBTContext& Context)
    {
        if (!bEntered)
        {
            bEntered = true;
            OnEnter(Context);
        }
        PreTick(Context); // Composite 가 부착 서비스를 주기 틱하는 훅
        const EBTNodeResult Result = Tick(Context);
        LastResult = Result;
        ++TickCount;
        if (Result != EBTNodeResult::InProgress)
        {
            OnExit(Result);
            bEntered = false;
        }
        return Result;
    }

    // 진행 중인 노드를 강제 중단. (파생 Composite 는 진행 중 자식도 함께 중단.)
    void ForceAbort(const FBTContext& Context)
    {
        if (bEntered)
        {
            Abort(Context);
            OnExit(EBTNodeResult::Aborted);
            LastResult = EBTNodeResult::Aborted;
            bEntered   = false;
        }
    }

    // 비-UObject 노드가 UObject 자산을 강하게 참조할 경우 GC 마킹을 전달.
    virtual void AddReferencedObjects(FReferenceCollector& Collector) { (void)Collector; }

    // ── 디버그/시각화 ──
    virtual const char* GetDebugName() const { return "BTNode"; }
    // Composite 만 non-null 자식 목록을 반환 — 디버거 트리 워크용.
    virtual const TArray<FBTNode_Base*>* GetDebugChildren() const { return nullptr; }

    // ── observer-abort / 게이트 ──
    // 데코레이터가 조건 게이트를 노출한다. Selector 가 매 think-step 상위 우선순위 자식의
    // 게이트를 peek 해 반응형으로 전환한다(공격 중 위협 감지→가드). 부수효과 없어야 한다.
    virtual bool IsGateOpen(const FBTContext& Context) const { (void)Context; return true; }
    // 이 노드(데코레이터)가 상위 우선순위 선점을 원하는가(observer-abort 켜짐).
    virtual bool WantsObserverAbort() const { return false; }
    bool          IsEntered() const   { return bEntered; }
    EBTNodeResult GetLastResult() const { return LastResult; }
    int64         GetTickCount() const  { return TickCount; }

protected:
    virtual void          OnEnter(const FBTContext& Context) { (void)Context; }
    virtual void          PreTick(const FBTContext& Context) { (void)Context; } // 서비스 틱 훅(Composite override)
    virtual EBTNodeResult Tick(const FBTContext& Context) = 0;
    virtual void          OnExit(EBTNodeResult Result)       { (void)Result; }
    virtual void          Abort(const FBTContext& Context)   { (void)Context; }

private:
    bool          bEntered   = false;
    EBTNodeResult LastResult = EBTNodeResult::Failed;
    int64         TickCount  = 0;
};

// =============================================================================
//  FBTService_Base — 부착된 Composite 가 활성인 동안 주기적으로 실행(센서→블랙보드 펌프 등)
// =============================================================================
//  plain 클래스. 컴포넌트(OwnedNodes)가 소유. Composite::PreTick 이 매 think-step 호출한다.
class FBTService_Base
{
public:
    virtual ~FBTService_Base() = default;
    virtual void        Tick(const FBTContext& Context) = 0;
    virtual const char* GetDebugName() const { return "Service"; }
};

// =============================================================================
//  FBTCompositeNode — 자식 여러 개를 보유하는 노드의 베이스
// =============================================================================
class FBTCompositeNode : public FBTNode_Base
{
public:
    // 컴파일러/빌더가 자식을 순서대로 추가. 노드 소유권은 컴포넌트(OwnedNodes)에 있다.
    void AddChild(FBTNode_Base* Child)
    {
        if (Child)
        {
            Children.push_back(Child);
        }
    }

    // 컴파일러가 그래프 노드의 Services 를 부착. PreTick 이 매 think-step 틱한다.
    void AddService(FBTService_Base* Service)
    {
        if (Service)
        {
            Services.push_back(Service);
        }
    }

    const TArray<FBTNode_Base*>&    GetChildren() const { return Children; }
    const TArray<FBTService_Base*>& GetServices() const { return Services; }
    const TArray<FBTNode_Base*>* GetDebugChildren() const override { return &Children; }

    void Initialize(const FBTContext& Context) override
    {
        for (FBTNode_Base* Child : Children)
        {
            if (Child)
            {
                Child->Initialize(Context);
            }
        }
    }

    void AddReferencedObjects(FReferenceCollector& Collector) override
    {
        for (FBTNode_Base* Child : Children)
        {
            if (Child)
            {
                Child->AddReferencedObjects(Collector);
            }
        }
    }

protected:
    // 진행 중이던 자식을 강제 중단하고 인덱스 리셋. OnExit/Abort 경로에서 사용.
    void AbortRunningChild(const FBTContext& Context)
    {
        if (CurrentChild >= 0 && CurrentChild < static_cast<int32>(Children.size()) && Children[CurrentChild])
        {
            Children[CurrentChild]->ForceAbort(Context);
        }
        CurrentChild = -1;
    }

    void Abort(const FBTContext& Context) override { AbortRunningChild(Context); }

    // 부착 서비스 주기 틱(FBTNode_Base::Execute 가 Tick 직전 호출).
    void PreTick(const FBTContext& Context) override
    {
        for (FBTService_Base* Service : Services)
        {
            if (Service)
            {
                Service->Tick(Context);
            }
        }
    }

    TArray<FBTNode_Base*>    Children;
    TArray<FBTService_Base*> Services;
    int32                    CurrentChild = -1; // 현재 실행 중(또는 평가 중) 자식 인덱스
};

// =============================================================================
//  FBTComposite_Selector — "OR / Fallback": 첫 성공까지 자식을 순서대로 시도
// =============================================================================
//  하나라도 Succeeded → 즉시 Succeeded. 모두 Failed/Aborted → Failed.
//  자식이 InProgress 면 그 자식에 머무른다(다음 step 에 이어서).
class FBTComposite_Selector : public FBTCompositeNode
{
public:
    const char* GetDebugName() const override { return "Selector"; }

protected:
    void OnEnter(const FBTContext& /*Context*/) override { CurrentChild = 0; }

    EBTNodeResult Tick(const FBTContext& Context) override
    {
        // observer-abort: 진행 중 자식보다 높은 우선순위 자식의 게이트가 열리면 선점 전환.
        // (기본 비활성 — WantsObserverAbort 가 true 인 데코레이터를 단 가지만 대상.)
        if (CurrentChild > 0)
        {
            for (int32 Higher = 0; Higher < CurrentChild; ++Higher)
            {
                FBTNode_Base* Candidate = Children[Higher];
                if (Candidate && Candidate->WantsObserverAbort() && Candidate->IsGateOpen(Context))
                {
                    if (Children[CurrentChild])
                    {
                        Children[CurrentChild]->ForceAbort(Context);
                    }
                    CurrentChild = Higher;
                    break;
                }
            }
        }

        while (CurrentChild >= 0 && CurrentChild < static_cast<int32>(Children.size()))
        {
            FBTNode_Base*       Child  = Children[CurrentChild];
            const EBTNodeResult Result = Child ? Child->Execute(Context) : EBTNodeResult::Failed;

            if (Result == EBTNodeResult::InProgress)
            {
                return EBTNodeResult::InProgress;
            }
            if (Result == EBTNodeResult::Succeeded)
            {
                CurrentChild = -1;
                return EBTNodeResult::Succeeded;
            }
            // Failed / Aborted → 다음 후보로.
            ++CurrentChild;
        }
        CurrentChild = -1;
        return EBTNodeResult::Failed;
    }
};

// =============================================================================
//  FBTComposite_Sequence — "AND": 모든 자식이 차례로 성공해야 성공
// =============================================================================
//  하나라도 Failed/Aborted → 즉시 Failed. 모두 Succeeded → Succeeded.
//  자식이 InProgress 면 그 자식에 머무른다(다음 step 에 이어서).
class FBTComposite_Sequence : public FBTCompositeNode
{
public:
    const char* GetDebugName() const override { return "Sequence"; }

protected:
    void OnEnter(const FBTContext& /*Context*/) override { CurrentChild = 0; }

    EBTNodeResult Tick(const FBTContext& Context) override
    {
        while (CurrentChild >= 0 && CurrentChild < static_cast<int32>(Children.size()))
        {
            FBTNode_Base*       Child  = Children[CurrentChild];
            const EBTNodeResult Result = Child ? Child->Execute(Context) : EBTNodeResult::Failed;

            if (Result == EBTNodeResult::InProgress)
            {
                return EBTNodeResult::InProgress;
            }
            if (Result != EBTNodeResult::Succeeded)
            {
                CurrentChild = -1;
                return EBTNodeResult::Failed;
            }
            // Succeeded → 다음 단계로.
            ++CurrentChild;
        }
        CurrentChild = -1;
        return EBTNodeResult::Succeeded;
    }
};

// =============================================================================
//  FBTComposite_Parallel — 모든 자식을 매 틱 동시 평가 (RequireAll, fail-fast)
// =============================================================================
//  자식 하나라도 Failed/Aborted → 나머지 abort 후 Failed.
//  모든 자식이 (한 번이라도) Succeeded → Succeeded. 그 외 InProgress.
//  이미 Succeeded 한 자식은 다시 틱하지 않는다(ChildDone 추적). "이동하면서 동시에 경계"
//  같은 동시 행동에 쓴다.
class FBTComposite_Parallel : public FBTCompositeNode
{
public:
    const char* GetDebugName() const override { return "Parallel"; }

protected:
    void OnEnter(const FBTContext& /*Context*/) override
    {
        ChildDone.assign(Children.size(), false);
    }

    EBTNodeResult Tick(const FBTContext& Context) override
    {
        bool bAllDone = true;
        for (int32 Index = 0; Index < static_cast<int32>(Children.size()); ++Index)
        {
            if (Index < static_cast<int32>(ChildDone.size()) && ChildDone[Index])
            {
                continue; // 이미 완료된 자식은 스킵
            }
            FBTNode_Base*       Child  = Children[Index];
            const EBTNodeResult Result = Child ? Child->Execute(Context) : EBTNodeResult::Failed;

            if (Result == EBTNodeResult::Failed || Result == EBTNodeResult::Aborted)
            {
                AbortAll(Context);
                return EBTNodeResult::Failed;
            }
            if (Result == EBTNodeResult::Succeeded)
            {
                if (Index < static_cast<int32>(ChildDone.size()))
                {
                    ChildDone[Index] = true;
                }
            }
            else
            {
                bAllDone = false;
            }
        }
        return bAllDone ? EBTNodeResult::Succeeded : EBTNodeResult::InProgress;
    }

    void Abort(const FBTContext& Context) override { AbortAll(Context); }

private:
    void AbortAll(const FBTContext& Context)
    {
        for (FBTNode_Base* Child : Children)
        {
            if (Child)
            {
                Child->ForceAbort(Context);
            }
        }
    }

    TArray<bool> ChildDone;
};
