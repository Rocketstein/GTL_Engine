#include "CoroutineCommand.h"

#include "Core/Logging/LogMacros.h"

void FExecuteCommand::GetResult(const sol::protected_function_result& InResult)
{
    Args.clear();

    bIsValid = InResult.valid();
    Status = InResult.status();

    if (!bIsValid)
    {
        sol::error Err = InResult;
        UE_LOG([Coroutine], Error, "Lua coroutine failed: %s", Err.what());
        return;
    }

    for (int i = 0; i < InResult.return_count(); ++i)
    {
        Args.push_back(InResult.get<sol::object>(i));
    }
}

void FWaitRealTime::Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context)
{
    CurrentTime += Context.DeltaTime;

    if (CurrentTime >= TargetTime)
        GetResult(InCoroutine());
}

void FWaitNextFrame::Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context)
{
    GetResult(InCoroutine(Context.DeltaTime));

	bIsEnd = GetStatus() != sol::call_status::yielded;
}

void FWaitUntilPredicate::Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context)
{
    sol::protected_function_result PredicateResult = Predicate();

    if (!PredicateResult.valid())
    {
        GetResult(PredicateResult);
        bIsEnd = true;
        return;
    }

    bool bDone = PredicateResult.get<bool>(0);

    if (!bDone)
    {
        bIsEnd = false;
        bIsValid = true;
        return;
    }

    bIsEnd = true;

    // 조건이 true가 된 순간에만 Lua 코루틴 재개
    GetResult(InCoroutine());
}
