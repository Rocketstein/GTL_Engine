#include "EnemySpawnerActor.h"
#include "GameFramework/GamejamActor/EnemyBaseActor.h"
#include "GameFramework/World.h"
#include "GameFramework/ActorPoolManager.h"
#include "Component/ScriptComponent.h"
#include "Core/Random.h"

IMPLEMENT_CLASS(AEnemySpawnerActor, AActor)

void AEnemySpawnerActor::BeginPlay()
{
    AActor::BeginPlay();
	PoolManager = GetWorld()->GetPoolManager();
}

void AEnemySpawnerActor::EndPlay()
{
    PoolManager = nullptr;
}

void AEnemySpawnerActor::InitDefaultComponents()
{
    ScriptComponent = AddComponent<UScriptComponent>();
    ScriptComponent->SetScriptPath("EnemySpawner.lua");
}
