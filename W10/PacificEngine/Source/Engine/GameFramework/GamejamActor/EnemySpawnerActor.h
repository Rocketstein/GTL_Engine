#pragma once
#include "GameFramework/AActor.h"

class FActorPoolManager;
class UScriptComponent;

struct FEnemySpawnPhase
{
    float TimeThreshold;
    uint32 DesiredCount;
    float SpawnRate;
};

class AEnemySpawnerActor : public AActor
{
public:
    DECLARE_CLASS(AEnemySpawnerActor, AActor)
    AEnemySpawnerActor() = default;

    void BeginPlay() override;
    void EndPlay() override;
    void InitDefaultComponents() override;

private:
    UScriptComponent* ScriptComponent = nullptr;

    FActorPoolManager* PoolManager = nullptr;
};
