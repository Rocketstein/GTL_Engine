#include "EngineStatics.h"
#include <utility>


uint32 UEngineStatics::GenUUID() { return NextUUID++; }

uint32 UEngineStatics::NextUUID = 1;
uint32 UEngineStatics::TotalAllocatedBytes = 0;
uint32 UEngineStatics::TotalAllocationCount = 0;

float                             UEngineStatics::GridSpacing = 100.f;
TArray<FResourceLoadHistoryEntry> UEngineStatics::ResourceLoadHistory;

void UEngineStatics::RecordResourceLoad(const char *AssetTypeLabel, const FString &AssetPath,
                                        double LoadMilliseconds, bool bCacheHit, bool bSuccess)
{
    FResourceLoadHistoryEntry Entry;
    Entry.AssetType = AssetTypeLabel != nullptr ? AssetTypeLabel : "Unknown";
    Entry.AssetPath = AssetPath;
    Entry.LoadMilliseconds = LoadMilliseconds;
    Entry.bCacheHit = bCacheHit;
    Entry.bSuccess = bSuccess;

    if (ResourceLoadHistory.size() >= MaxResourceLoadHistoryCount)
    {
        ResourceLoadHistory.erase(ResourceLoadHistory.begin());
    }

    ResourceLoadHistory.push_back(std::move(Entry));
}

const TArray<FResourceLoadHistoryEntry> &UEngineStatics::GetResourceLoadHistory()
{
    return ResourceLoadHistory;
}
