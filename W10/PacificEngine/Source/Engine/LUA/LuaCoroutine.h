#pragma once
#include "CoroutineCommand.h"

struct FLuaCoroutineTask
{
    sol::thread Thread;
    sol::coroutine Coroutine;
};

class CoroutineExecutor
{
public:
    CoroutineExecutor(FLuaCoroutineTask&& InTask)
        : Task(std::move(InTask)) { SetCommand(new FExecuteCommand()); }
    ~CoroutineExecutor() { delete Command; }
    void Tick(const FCouroutineContext& Context);
    bool IsValid();

private:
    void SetCommand(FExecuteCommand* NewCommand);

private:
    FExecuteCommand* Command = nullptr;
    FLuaCoroutineTask Task;
};

struct FPendingCoroutineStart
{
    uint32 CoroutineId = 0;
    sol::function Func;

    FPendingCoroutineStart(uint32 InCoroutineID, sol::function InFunc)
    {
        CoroutineId = InCoroutineID;
        Func = InFunc;
    }
};

class FCoroutineExecutorSet
{
public:
    ~FCoroutineExecutorSet() { Clear(); }
    uint32 Start(const sol::function& LuaFunc);
    void StartInternal(uint32 FuncKey, const sol::function& LuaFunc);
    bool Stop(uint32 FuncKey);
    void Tick(const FCouroutineContext& Context);
    void Clear();

private:
    TMap<uint32, CoroutineExecutor*> Executors;
    TArray<FPendingCoroutineStart> PendingStarts;
    TSet<uint32> PendingStopKeys;
    uint32 NextExecutorId = 0;
    bool bIsTicking = false;
};
