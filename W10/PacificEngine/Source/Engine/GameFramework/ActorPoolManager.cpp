#include "GameFramework/ActorPoolManager.h"
#include "GameFramework/ActorPool.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/Object.h"

FActorPoolManager::FActorPoolManager(UWorld* InWorld)
    : World(InWorld)
{
}

FActorPoolManager::~FActorPoolManager()
{
    DestroyAll();
}

void FActorPoolManager::SetWorld(UWorld* InWorld)
{
    if (World == InWorld)
    {
        return;
    }

    DestroyAll();
    World = InWorld;
}

void FActorPoolManager::Warmup(const FString& ClassName, uint32 Count, const FActorPool::FActorCallback& Initialize)
{
    GetOrCreatePool(ClassName).Warmup(Count, Initialize);
}

AActor* FActorPoolManager::Acquire(const FString& ClassName, const FActorPool::FActorCallback& Activate)
{
    FActorPool& Pool = GetOrCreatePool(ClassName);
    AActor* Actor = Pool.Acquire(Activate);
    RegisterActiveActor(Actor, &Pool);
    return Actor;
}

void FActorPoolManager::Release(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    auto It = ActiveActorToPool.find(Actor);
    if (It == ActiveActorToPool.end())
    {
        return;
    }

    // 풀에 반납될 때는 델리게이트를 해제하여, 풀에 있는 동안 
    // 불필요한 호출이 발생하지 않도록 합니다.
    Actor->OnPoolReturnRequested.RemoveDynamic(this);
    DelegateBoundActors.erase(Actor);

    FActorPool* Pool = It->second;
    ActiveActorToPool.erase(It);
    Pool->Release(Actor);
}

void FActorPoolManager::ReleaseActiveByClass(const FString& ClassName)
{
    const FActorPool* Pool = FindPool(ClassName);
    if (!Pool)
    {
        return;
    }

    TArray<AActor*> ActorsToRelease;
    for (const auto& Pair : ActiveActorToPool)
    {
        if (Pair.second == Pool)
        {
            ActorsToRelease.push_back(Pair.first);
        }
    }

    for (AActor* Actor : ActorsToRelease)
    {
        Release(Actor);
    }
}

void FActorPoolManager::ForgetActor(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    if (DelegateBoundActors.erase(Actor) > 0)
    {
        Actor->OnPoolReturnRequested.RemoveDynamic(this);
    }

    BoundActorUUIDs.erase(Actor);
    ActiveActorToPool.erase(Actor);

    for (auto& Pair : Pools)
    {
        if (Pair.second)
        {
            Pair.second->ForgetActor(Actor);
        }
    }
}

void FActorPoolManager::DestroyAll()
{
    DelegateBoundActors.clear();
    BoundActorUUIDs.clear();
    ActiveActorToPool.clear();

    for (auto& Pair : Pools)
    {
        if (Pair.second)
        {
            Pair.second->DestroyAll();
        }
    }

    Pools.clear();
}

uint32 FActorPoolManager::GetActiveCount(const FString& ClassName) const
{
    const FActorPool* Pool = FindPool(ClassName);
    return Pool ? Pool->GetActiveCount() : 0;
}

uint32 FActorPoolManager::GetAvailableCount(const FString& ClassName) const
{
    const FActorPool* Pool = FindPool(ClassName);
    return Pool ? Pool->GetAvailableCount() : 0;
}

FActorPool& FActorPoolManager::GetOrCreatePool(const FString& ClassName)
{
    auto It = Pools.find(ClassName);
    if (It == Pools.end())
    {
        auto Pool = std::make_unique<FActorPool>(World, ClassName);
        FActorPool* RawPool = Pool.get();
        Pools.emplace(ClassName, std::move(Pool));
        return *RawPool;
    }

    return *It->second;
}

const FActorPool* FActorPoolManager::FindPool(const FString& ClassName) const
{
    auto It = Pools.find(ClassName);
    if (It == Pools.end())
    {
        return nullptr;
    }

    return It->second.get();
}

void FActorPoolManager::RegisterActiveActor(AActor* Actor, FActorPool* Pool)
{
    if (!Actor || !Pool)
    {
        return;
    }

    ActiveActorToPool[Actor] = Pool;
    if (DelegateBoundActors.insert(Actor).second)
    {
        BoundActorUUIDs[Actor] = Actor->GetUUID();
        Actor->OnPoolReturnRequested.AddDynamic(this, &FActorPoolManager::HandleActorPoolReturnRequested);
    }
}

void FActorPoolManager::HandleActorPoolReturnRequested(AActor* Actor)
{
    Release(Actor);
}

bool FActorPoolManager::IsBoundActorAlive(AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    auto UUIDIt = BoundActorUUIDs.find(Actor);
    if (UUIDIt == BoundActorUUIDs.end())
    {
        return false;
    }

    for (UObject* Object : GUObjectArray)
    {
        if (Object == Actor)
        {
            return Actor->GetUUID() == UUIDIt->second;
        }
    }

    return false;
}
