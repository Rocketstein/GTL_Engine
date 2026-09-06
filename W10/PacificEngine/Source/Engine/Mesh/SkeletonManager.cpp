#include "SkeletonManager.h"
#include "Object/Object.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Mesh/SkeletalMesh.h"
#include <filesystem>

USkeleton* FSkeletonManager::Find(const FString& Key)
{
    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(Key, SourcePath, SubResource);
    
    // 접두사 보정 (Skeleton_ 로 변경)
    if (!SubResource.empty() && SubResource.find(SkeletalMeshPrefix::Skeleton) != 0)
    {
        if (SubResource.find(SkeletalMeshPrefix::Mesh) == 0)
            SubResource = SkeletalMeshPrefix::Skeleton + SubResource.substr(SkeletalMeshPrefix::Mesh.size());
        else
            SubResource = SkeletalMeshPrefix::Skeleton + SubResource;
    }

    FString CacheKey = FPaths::BuildSubResourceCachePath(SourcePath, SubResource);

    auto It = SkeletonCache.find(CacheKey);
    return (It != SkeletonCache.end()) ? It->second : nullptr;
}

void FSkeletonManager::Unload(const FString& Key)
{
    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(Key, SourcePath, SubResource);
    
    if (!SubResource.empty() && SubResource.find(SkeletalMeshPrefix::Skeleton) != 0)
    {
        if (SubResource.find(SkeletalMeshPrefix::Mesh) == 0)
            SubResource = SkeletalMeshPrefix::Skeleton + SubResource.substr(SkeletalMeshPrefix::Mesh.size());
        else
            SubResource = SkeletalMeshPrefix::Skeleton + SubResource;
    }

    FString CacheKey = FPaths::BuildSubResourceCachePath(SourcePath, SubResource);

    SkeletonCache.erase(CacheKey);
}

USkeleton* FSkeletonManager::LoadSkeleton(const FString& PathFileName)
{
    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(PathFileName, SourcePath, SubResource);
    
    // 접두사 보정
    if (!SubResource.empty() && SubResource.find(SkeletalMeshPrefix::Skeleton) != 0)
    {
        if (SubResource.find(SkeletalMeshPrefix::Mesh) == 0)
            SubResource = SkeletalMeshPrefix::Skeleton + SubResource.substr(SkeletalMeshPrefix::Mesh.size());
        else
            SubResource = SkeletalMeshPrefix::Skeleton + SubResource;
    }
    
    FString CacheKey = FPaths::BuildSubResourceCachePath(SourcePath, SubResource);
    if (CacheKey.empty()) return nullptr;

    auto It = SkeletonCache.find(CacheKey);
    if (It != SkeletonCache.end()) return It->second;

    USkeleton* Skeleton = UObjectManager::Get().CreateObject<USkeleton>();

    FWindowsBinReader Reader(CacheKey);
    if (Reader.IsValid())
    {
        Skeleton->Serialize(Reader);
        SkeletonCache[CacheKey] = Skeleton;
        return Skeleton;
    }

    return nullptr;
}