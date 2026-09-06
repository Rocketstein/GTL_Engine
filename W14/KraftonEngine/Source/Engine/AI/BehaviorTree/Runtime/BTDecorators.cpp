#include "AI/BehaviorTree/Runtime/BTDecorators.h"

#include "AI/BehaviorTree/Runtime/BTContext.h"
#include "AI/BehaviorTree/Runtime/BTTasks.h" // EvaluateBrainCondition
#include "Component/AI/AIBlackboardComponent.h"

bool FBTDecorator_Blackboard::IsGateOpen(const FBTContext& Context) const
{
    if (!Context.Blackboard || !Key.IsValid())
    {
        return false;
    }
    return Context.Blackboard->GetBool(Key) == bExpected;
}

bool FBTDecorator_Condition::IsGateOpen(const FBTContext& Context) const
{
    return EvaluateBrainCondition(Context, ConditionName);
}

EBTNodeResult FBTDecorator_Cooldown::Tick(const FBTContext& Context)
{
    if (Remaining > 0.0f)
    {
        Remaining -= Context.DeltaTime;
        if (Remaining > 0.0f)
        {
            return EBTNodeResult::Failed; // 쿨다운 중 — 차단
        }
    }
    if (!Child)
    {
        return EBTNodeResult::Failed;
    }
    const EBTNodeResult Result = Child->Execute(Context);
    if (Result == EBTNodeResult::Succeeded)
    {
        Remaining = CooldownSeconds; // 성공 후 쿨다운 시작
    }
    return Result;
}
