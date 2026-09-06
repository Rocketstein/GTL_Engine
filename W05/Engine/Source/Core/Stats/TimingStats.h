#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/Platform/PlatformTypes.h"

struct FTimingStatEntry
{
    FString Name;
    uint64  CallCount = 0;
    double  LastTimeMs = 0.0;
    double  AverageTimeMs = 0.0;
    double  TotalTimeMs = 0.0;
};

struct TStatId
{
    const char *Name = "";

    explicit TStatId(const char *InName = "")
        : Name(InName)
    {
    }
};

class FTimingStats
{
  public:
    static void AddCycles(TStatId StatId, uint64 CycleDiff);
    static void Reset();
    static TArray<FTimingStatEntry> GetEntries();
};

class FScopeCycleCounter
{
  public:
    explicit FScopeCycleCounter(TStatId StatId);
    ~FScopeCycleCounter();

    uint64 Finish();

  private:
    uint64  StartCycles = 0;
    TStatId UsedStatId;
    bool    bFinished = false;
};

#define PREPROCESS_JOIN_INNER(A, B) A##B
#define PREPROCESS_JOIN(A, B) PREPROCESS_JOIN_INNER(A, B)

#define SCOPED_TIMING_STAT(NameLiteral)                                                          \
    static const TStatId PREPROCESS_JOIN(GTimingStatId_, __LINE__)(NameLiteral);                \
    FScopeCycleCounter  PREPROCESS_JOIN(GScopeCycleCounter_, __LINE__)(                          \
        PREPROCESS_JOIN(GTimingStatId_, __LINE__))
