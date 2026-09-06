#pragma once

#include "Core/CoreTypes.h"
#include <algorithm>
#include <functional>
#include <utility>

class UWorld;
class AActor;

/*
    FActorPool owns a reusable set of actors of one concrete type (defined by class name).
    Actors stay registered in the world; Release only deactivates and hides them.
*/
class FActorPool
{
public:
    using FActorCallback = std::function<void(AActor*)>;

    explicit FActorPool(UWorld* InWorld, const FString& InClassName);
    ~FActorPool();

    FActorPool(const FActorPool&) = delete;
    FActorPool& operator=(const FActorPool&) = delete;

    void Warmup(uint32 Count, const FActorCallback& Initialize = nullptr);
    AActor* Acquire(const FActorCallback& Activate = nullptr);
    void Release(AActor* Actor, const FActorCallback& Deactivate = nullptr);
    void ReleaseAll(const FActorCallback& Deactivate = nullptr);
    void ForgetActor(AActor* Actor);
    void DestroyAll();

    uint32 GetActiveCount() const { return static_cast<uint32>(Active.size()); }
    uint32 GetAvailableCount() const { return static_cast<uint32>(Available.size()); }
    uint32 GetTotalCount() const { return GetActiveCount() + GetAvailableCount(); }

private:
    AActor* CreateActor();
    static void ActivateActor(AActor* Actor);
    static void DeactivateActor(AActor* Actor);

    UWorld* World = nullptr;
    FString ActorClassName;
    TArray<AActor*> Available;
    TArray<AActor*> Active;
};
