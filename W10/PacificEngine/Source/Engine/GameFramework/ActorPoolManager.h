#pragma once

#include "Core/CoreTypes.h"
#include <functional>
#include <memory>
#include <utility>
#include "GameFramework/ActorPool.h"

class UWorld;
class AActor;

/*
    FActorPoolManager keeps one actor pool per concrete actor class name.
    Pooled actors can call RequestReturnToPool(), and the manager will return
    them to the pool that acquired them.
*/
class FActorPoolManager
{
public:
    explicit FActorPoolManager(UWorld* InWorld);
	FActorPoolManager() = default;
    ~FActorPoolManager();

    FActorPoolManager(const FActorPoolManager&) = delete;
    FActorPoolManager& operator=(const FActorPoolManager&) = delete;

    void SetWorld(UWorld* InWorld);

    // Helper for C++ usage with types
    template <typename TActor>
    void Warmup(uint32 Count, const std::function<void(TActor*)>& Initialize = nullptr)
    {
        FActorPool::FActorCallback WrappedInit = nullptr;
        if (Initialize)
        {
            WrappedInit = [Initialize](AActor* A) { Initialize(static_cast<TActor*>(A)); };
        }
        Warmup(TActor::StaticClass()->GetName(), Count, WrappedInit);
    }

    void Warmup(const FString& ClassName, uint32 Count, const FActorPool::FActorCallback& Initialize = nullptr);

    // Helper for C++ usage with types
    template <typename TActor>
    TActor* Acquire(const std::function<void(TActor*)>& Activate = nullptr)
    {
        FActorPool::FActorCallback WrappedActivate = nullptr;
        if (Activate)
        {
            WrappedActivate = [Activate](AActor* A) { Activate(static_cast<TActor*>(A)); };
        }
        return static_cast<TActor*>(Acquire(TActor::StaticClass()->GetName(), WrappedActivate));
    }

    AActor* Acquire(const FString& ClassName, const FActorPool::FActorCallback& Activate = nullptr);

    void Release(AActor* Actor);
    void ReleaseActiveByClass(const FString& ClassName);
    void ForgetActor(AActor* Actor);
    void DestroyAll();

    // Helper for C++ usage with types
    template <typename TActor>
    uint32 GetActiveCount() const
    {
        return GetActiveCount(TActor::StaticClass()->GetName());
    }

    uint32 GetActiveCount(const FString& ClassName) const;

    // Helper for C++ usage with types
    template <typename TActor>
    uint32 GetAvailableCount() const
    {
        return GetAvailableCount(TActor::StaticClass()->GetName());
    }

    uint32 GetAvailableCount(const FString& ClassName) const;

private:
    FActorPool& GetOrCreatePool(const FString& ClassName);
    const FActorPool* FindPool(const FString& ClassName) const;

    void RegisterActiveActor(AActor* Actor, FActorPool* Pool);
    void HandleActorPoolReturnRequested(AActor* Actor);
    bool IsBoundActorAlive(AActor* Actor) const;

    UWorld* World = nullptr;
    TMap<FString, std::unique_ptr<FActorPool>> Pools;
    TMap<AActor*, FActorPool*> ActiveActorToPool;
    TMap<AActor*, uint32> BoundActorUUIDs;
    TSet<AActor*> DelegateBoundActors;
};
