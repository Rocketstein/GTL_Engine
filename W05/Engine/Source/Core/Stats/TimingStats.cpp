#include "Core/Stats/TimingStats.h"
#include "Core/Platform/PlatformTime.h"
#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace
{
    struct FTimingAccumulator
    {
        uint64 TotalCycles = 0;
        uint64 CallCount = 0;
        uint64 LastCycles = 0;
    };

    std::mutex &GetTimingMutex()
    {
        static std::mutex Mutex;
        return Mutex;
    }

    std::unordered_map<FString, FTimingAccumulator> &GetTimingMap()
    {
        static std::unordered_map<FString, FTimingAccumulator> TimingMap;
        return TimingMap;
    }
} // namespace

void FTimingStats::AddCycles(TStatId StatId, uint64 CycleDiff)
{
    const FString StatName =
        (StatId.Name != nullptr && StatId.Name[0] != '\0') ? FString(StatId.Name) : FString("Unnamed");

    std::lock_guard<std::mutex> Lock(GetTimingMutex());
    FTimingAccumulator         &Accumulator = GetTimingMap()[StatName];
    Accumulator.TotalCycles += CycleDiff;
    ++Accumulator.CallCount;
    Accumulator.LastCycles = CycleDiff;
}

void FTimingStats::Reset()
{
    std::lock_guard<std::mutex> Lock(GetTimingMutex());
    GetTimingMap().clear();
}

TArray<FTimingStatEntry> FTimingStats::GetEntries()
{
    TArray<FTimingStatEntry> Result;

    std::lock_guard<std::mutex> Lock(GetTimingMutex());
    Result.reserve(GetTimingMap().size());
    for (const auto &Pair : GetTimingMap())
    {
        const FTimingAccumulator &Accumulator = Pair.second;
        if (Accumulator.CallCount == 0)
        {
            continue;
        }

        FTimingStatEntry Entry;
        Entry.Name = Pair.first;
        Entry.CallCount = Accumulator.CallCount;
        Entry.LastTimeMs = FPlatformTime::ToMilliseconds(Accumulator.LastCycles);
        Entry.TotalTimeMs = FPlatformTime::ToMilliseconds(Accumulator.TotalCycles);
        Entry.AverageTimeMs = Entry.TotalTimeMs / static_cast<double>(Accumulator.CallCount);
        Result.push_back(std::move(Entry));
    }

    std::sort(Result.begin(), Result.end(),
              [](const FTimingStatEntry &A, const FTimingStatEntry &B)
              { return A.TotalTimeMs > B.TotalTimeMs; });

    return Result;
}

FScopeCycleCounter::FScopeCycleCounter(TStatId StatId)
    : StartCycles(FPlatformTime::Cycles64()), UsedStatId(StatId)
{
}

FScopeCycleCounter::~FScopeCycleCounter() { Finish(); }

uint64 FScopeCycleCounter::Finish()
{
    if (bFinished)
    {
        return 0;
    }

    const uint64 EndCycles = FPlatformTime::Cycles64();
    const uint64 CycleDiff = EndCycles - StartCycles;
    FTimingStats::AddCycles(UsedStatId, CycleDiff);
    bFinished = true;
    return CycleDiff;
}
