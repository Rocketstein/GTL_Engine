#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Resource/IAssetLoader.h"
#include "Mesh/Skeleton.h"
#include <map>
#include <string>

// USkeleton 관련 처리를 담당
// NOTE: 서로 다른 SkeletalMesh가 동일한 Skeleton을 공유하는 케이스를 위해
//       USkeleton을 별도로 관리 (e.g. 캐릭터 스킨 변경)
class FSkeletonManager : public TSingleton<FSkeletonManager>, public IAssetLoader<USkeleton>
{
    friend class TSingleton<FSkeletonManager>;

    // path -> USkeleton* 캐시
    TMap<FString, USkeleton*> SkeletonCache;

public:
    // IAssetLoader Implementation
    virtual void SetDevice(ID3D11Device* /*InDevice*/) override {}
    virtual USkeleton* Load(const FString& Path) override { return LoadSkeleton(Path); }
    virtual USkeleton* Find(const FString& Key) override;
    virtual void Unload(const FString& Key) override;
    virtual void UnloadAll() override { SkeletonCache.clear(); }

    USkeleton* LoadSkeleton(const FString& PathFileName);

private:
    FSkeletonManager() = default;
};