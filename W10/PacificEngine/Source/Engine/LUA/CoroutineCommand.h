#pragma once
#include "Core/CoreTypes.h"
#include "Sol/sol.hpp"

struct FCouroutineContext
{
    float DeltaTime = 0.f;
    float FixedDeltaTime = 0.f;

    FCouroutineContext(float InDeltaTime = 0.f, float InFixedDeltaTime = 1 / 50)
    {
        DeltaTime = InDeltaTime;
        FixedDeltaTime = InFixedDeltaTime;
    }
};

class FExecuteCommand
{
public:
    virtual ~FExecuteCommand() = default;
    virtual void Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context) { GetResult(InCoroutine()); }
    virtual bool IsEnd() { return true; }
    virtual bool IsValid() { return bIsValid; };
    virtual sol::call_status GetStatus() { return Status; };
    virtual void GetResult(const sol::protected_function_result& InResult);
    virtual void Invalidate() { bIsValid = false; }

    template <typename T>
    bool TryGetResultParam(uint32 Index, T& OutValue) const
    {
        if (Index >= Args.size())
        {
            return false;
        }

        const sol::object& Obj = Args[Index];

        if (!Obj.is<T>())
        {
            return false;
        }

        OutValue = Obj.as<T>();
        return true;
    }

protected:
    bool bIsValid = true;
    sol::call_status Status;
    TArray<sol::object> Args;
};


class FWaitRealTime final : public FExecuteCommand
{
public:
    FWaitRealTime(float RealTime) { TargetTime = RealTime; }
    void Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context) override;
    virtual bool IsEnd() override { return CurrentTime >= TargetTime; }


private:
    float TargetTime = 0;
    float CurrentTime = 0;
};

class FWaitNextFrame final : public FExecuteCommand
{
public:
    void Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context) override;
    virtual bool IsEnd() override { return bIsEnd; }

private:
    bool bIsEnd = false;
};

class FWaitUntilPredicate final : public FExecuteCommand
{
public:
    FWaitUntilPredicate(sol::function InPredicate) { Predicate = InPredicate; }
    void Run(sol::coroutine& InCoroutine, const FCouroutineContext& Context) override;
    virtual bool IsEnd() override { return bIsEnd; }

private:
    bool bIsEnd = false;
    sol::function Predicate;
};