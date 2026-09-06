#include "AI/BehaviorTree/BTNodeRegistry.h"

#include "AI/BehaviorTree/BehaviorTreeTypes.h"
#include "AI/BehaviorTree/Runtime/BTDecorators.h"
#include "AI/BehaviorTree/Runtime/BTServices.h"
#include "AI/BehaviorTree/Runtime/BTTasks.h"
#include "Component/AI/BehaviorTreeComponent.h"

// ── Leaf 카탈로그 ──────────────────────────────────────────────────────────
const TArray<FBTLeafType>& GetBTLeafCatalog()
{
    static const TArray<FBTLeafType> Catalog = []
    {
        TArray<FBTLeafType> C;
        auto Add = [&C](const char* Id, bool bCond, const char* Cat, const char* Disp,
                        FBTNode_Base* (*Make)(UBehaviorTreeComponent&, const FBTGraphNode&),
                        int ParamFlags = 0)
        {
            FBTLeafType T;
            T.Id = FName(Id); T.bIsCondition = bCond; T.Category = Cat; T.Display = Disp; T.Make = Make;
            T.ParamFlags = ParamFlags;
            C.push_back(T);
        };

        // ── Tasks ──
        Add("Chase", false, "Movement", "Chase",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_Chase>(); });
        Add("Attack", false, "Combat", "Attack",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_Attack>(); });
        Add("SelectAttack", false, "Combat", "Select Attack",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_SelectAttack>(); });
        Add("ReactiveDeflect", false, "Combat", "Reactive Deflect",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_ReactiveDeflect>(); });
        Add("TacticalMove", false, "Movement", "Tactical Move",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_TacticalMove>(); });
        Add("Strafe", false, "Movement", "Strafe",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_Strafe>(); });
        Add("Reposition", false, "Movement", "Reposition",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_Reposition>(); });
        Add("Investigate", false, "Movement", "Investigate",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_Investigate>(); });
        Add("Idle", false, "Movement", "Idle",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_Idle>(); });

        // ── Conditions ──
        Add("CanSeeTarget", true, "Perception", "CanSeeTarget",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTCondition_CanSeeTarget>(); });
        Add("InAttackRange", true, "Combat", "InAttackRange",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTCondition_InAttackRange>(); });
        Add("TargetThreatening", true, "Combat", "TargetThreatening",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTCondition_TargetThreatening>(); });
        Add("IsAlerted", true, "Perception", "IsAlerted",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTCondition_IsAlerted>(); });

        // ── 범용 Flow / Blackboard / Debug (게임플레이 비의존) ──
        Add("Wait", false, "Flow", "Wait",
            [](UBehaviorTreeComponent& O, const FBTGraphNode& N) -> FBTNode_Base*
            {
                FBTTask_Wait* T = O.MakeNode<FBTTask_Wait>();
                T->Duration = N.FloatValue > 0.0f ? N.FloatValue : 1.0f;
                return T;
            }, BTParam_Float);
        Add("Succeed", false, "Flow", "Succeed",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_Succeed>(); });
        Add("Fail", false, "Flow", "Fail",
            [](UBehaviorTreeComponent& O, const FBTGraphNode&) -> FBTNode_Base* { return O.MakeNode<FBTTask_Fail>(); });
        Add("SetBlackboardBool", false, "Blackboard", "Set BB Bool",
            [](UBehaviorTreeComponent& O, const FBTGraphNode& N) -> FBTNode_Base*
            {
                FBTTask_SetBlackboardBool* T = O.MakeNode<FBTTask_SetBlackboardBool>();
                T->Key   = N.KeyValue;
                T->Value = N.BoolValue;
                return T;
            }, BTParam_Key | BTParam_Bool);
        Add("SetBlackboardFloat", false, "Blackboard", "Set BB Float",
            [](UBehaviorTreeComponent& O, const FBTGraphNode& N) -> FBTNode_Base*
            {
                FBTTask_SetBlackboardFloat* T = O.MakeNode<FBTTask_SetBlackboardFloat>();
                T->Key   = N.KeyValue;
                T->Value = N.FloatValue;
                return T;
            }, BTParam_Key | BTParam_Float);
        Add("Log", false, "Debug", "Log",
            [](UBehaviorTreeComponent& O, const FBTGraphNode& N) -> FBTNode_Base*
            {
                FBTTask_Log* T = O.MakeNode<FBTTask_Log>();
                T->Message = N.StringValue;
                return T;
            }, BTParam_String);

        // ── Generic (데이터 구동) ──
        Add("BrainVerb", false, "Generic", "Brain Verb",
            [](UBehaviorTreeComponent& O, const FBTGraphNode& N) -> FBTNode_Base*
            {
                FBTTask_BrainVerb* T = O.MakeNode<FBTTask_BrainVerb>();
                T->VerbId = N.KeyValue;
                return T;
            }, BTParam_Key);
        Add("Blackboard", true, "Generic", "Blackboard?",
            [](UBehaviorTreeComponent& O, const FBTGraphNode& N) -> FBTNode_Base*
            {
                FBTCondition_Blackboard* Cnd = O.MakeNode<FBTCondition_Blackboard>();
                Cnd->Key       = N.KeyValue;
                Cnd->bExpected = N.BoolValue;
                return Cnd;
            }, BTParam_Key | BTParam_Bool);
        return C;
    }();
    return Catalog;
}

// ── Decorator 카탈로그 ─────────────────────────────────────────────────────
const TArray<FBTDecoratorType>& GetBTDecoratorCatalog()
{
    static const TArray<FBTDecoratorType> Catalog = []
    {
        TArray<FBTDecoratorType> C;
        auto Add = [&C](const char* Id, const char* Disp,
                        FBTDecorator_Base* (*Make)(UBehaviorTreeComponent&, const FBTAuxNode&))
        {
            FBTDecoratorType T;
            T.Id = FName(Id); T.Display = Disp; T.Make = Make;
            C.push_back(T);
        };

        Add("Inverter", "Inverter",
            [](UBehaviorTreeComponent& O, const FBTAuxNode&) -> FBTDecorator_Base* { return O.MakeNode<FBTDecorator_Inverter>(); });
        Add("ForceSuccess", "Force Success",
            [](UBehaviorTreeComponent& O, const FBTAuxNode&) -> FBTDecorator_Base* { return O.MakeNode<FBTDecorator_ForceSuccess>(); });
        Add("ForceFailure", "Force Failure",
            [](UBehaviorTreeComponent& O, const FBTAuxNode&) -> FBTDecorator_Base* { return O.MakeNode<FBTDecorator_ForceFailure>(); });
        Add("Cooldown", "Cooldown",
            [](UBehaviorTreeComponent& O, const FBTAuxNode& A) -> FBTDecorator_Base*
            {
                FBTDecorator_Cooldown* D = O.MakeNode<FBTDecorator_Cooldown>();
                D->CooldownSeconds = A.FloatValue > 0.0f ? A.FloatValue : 1.0f;
                return D;
            });
        Add("Blackboard", "Blackboard Gate",
            [](UBehaviorTreeComponent& O, const FBTAuxNode& A) -> FBTDecorator_Base*
            {
                FBTDecorator_Blackboard* D = O.MakeNode<FBTDecorator_Blackboard>();
                D->Key            = A.KeyValue;
                D->bExpected      = A.BoolValue;
                D->bObserverAbort = A.IntValue != 0;
                return D;
            });
        Add("Condition", "Condition Gate",
            [](UBehaviorTreeComponent& O, const FBTAuxNode& A) -> FBTDecorator_Base*
            {
                FBTDecorator_Condition* D = O.MakeNode<FBTDecorator_Condition>();
                D->ConditionName  = A.KeyValue;
                D->bObserverAbort = A.IntValue != 0;
                return D;
            });
        return C;
    }();
    return Catalog;
}

// ── Service 카탈로그 ───────────────────────────────────────────────────────
const TArray<FBTServiceType>& GetBTServiceCatalog()
{
    static const TArray<FBTServiceType> Catalog = []
    {
        TArray<FBTServiceType> C;
        auto Add = [&C](const char* Id, const char* Disp,
                        FBTService_Base* (*Make)(UBehaviorTreeComponent&, const FBTAuxNode&))
        {
            FBTServiceType T;
            T.Id = FName(Id); T.Display = Disp; T.Make = Make;
            C.push_back(T);
        };

        Add("BrainSense", "Brain Sense",
            [](UBehaviorTreeComponent& O, const FBTAuxNode& A) -> FBTService_Base*
            {
                FBTService_BrainSense* S = O.MakeService<FBTService_BrainSense>();
                S->Interval = A.FloatValue > 0.0f ? A.FloatValue : 0.25f;
                return S;
            });
        return C;
    }();
    return Catalog;
}

const FBTLeafType* FindBTLeafType(const FName& Id)
{
    for (const FBTLeafType& T : GetBTLeafCatalog())
    {
        if (T.Id == Id) return &T;
    }
    return nullptr;
}

const FBTDecoratorType* FindBTDecoratorType(const FName& Id)
{
    for (const FBTDecoratorType& T : GetBTDecoratorCatalog())
    {
        if (T.Id == Id) return &T;
    }
    return nullptr;
}

const FBTServiceType* FindBTServiceType(const FName& Id)
{
    for (const FBTServiceType& T : GetBTServiceCatalog())
    {
        if (T.Id == Id) return &T;
    }
    return nullptr;
}
