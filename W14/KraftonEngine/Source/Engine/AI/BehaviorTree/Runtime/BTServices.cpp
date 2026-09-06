#include "AI/BehaviorTree/Runtime/BTServices.h"

#include "AI/BehaviorTree/Runtime/BTContext.h"
#include "GameFramework/Pawn/EnemyCharacter.h"

void FBTService_BrainSense::Tick(const FBTContext& Context)
{
    if (!Context.Enemy)
    {
        return;
    }
    Accum += Context.DeltaTime;
    if (Accum < Interval)
    {
        return;
    }
    Accum = 0.0f;
    Context.Enemy->Brain_Sense();
    Context.Enemy->Brain_AcquireTarget();
}
