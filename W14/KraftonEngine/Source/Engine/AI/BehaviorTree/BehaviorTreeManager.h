#pragma once

#include "Asset/AssetRegistry.h"
#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Object/GarbageCollection.h"

class UBehaviorTreeAsset;

// UBehaviorTreeAsset 디스크 로드/캐시/저장 매니저. FLuaBlueprintManager / FAnimGraphManager 미러.
// 캐시된 자산을 FGCObject 로 강참조해 GC sweep 에서 살아있게 한다.
class FBehaviorTreeManager : public TSingleton<FBehaviorTreeManager>, public FGCObject
{
    friend class TSingleton<FBehaviorTreeManager>;

public:
    UBehaviorTreeAsset* Load(const FString& Path);
    UBehaviorTreeAsset* Find(const FString& Path) const;
    bool                Save(UBehaviorTreeAsset* Asset);

    void                          RefreshAvailableBehaviorTrees();
    const TArray<FAssetListItem>& GetAvailableBehaviorTreeFiles() const { return AvailableBehaviorTreeFiles; }

    const char* GetReferencerName() const override { return "FBehaviorTreeManager"; }
    void        AddReferencedObjects(FReferenceCollector& Collector) override;
    void        ClearCache();

private:
    TMap<FString, UBehaviorTreeAsset*> LoadedTrees;
    TArray<FAssetListItem>             AvailableBehaviorTreeFiles;
};
