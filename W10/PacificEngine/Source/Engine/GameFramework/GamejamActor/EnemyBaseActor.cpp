#include "EnemyBaseActor.h"
#include "Component/Collision/CircleCollider2DComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(AEnemyBaseActor, AActor)

void AEnemyBaseActor::BeginPlay()
{
    AActor::BeginPlay();
    SetActorTag(FName("Enemy"));
}

void AEnemyBaseActor::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
}

UStaticMeshComponent* AEnemyBaseActor::GetDefaultMeshComponent()
{
    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* Asset = FObjManager::Get().Load(FPaths::ContentRelativePath("Models/_Basic/Cube.OBJ"));

    UStaticMeshComponent* MeshComp = AddComponent<UStaticMeshComponent>();
    MeshComp->SetStaticMesh(Asset);
    MeshComp->SetReceivesDecals(false);
    return MeshComp;
}

UCollider2DComponent* AEnemyBaseActor::GetDefaultColliderComponent()
{
    UCircleCollider2DComponent* Collider = AddComponent<UCircleCollider2DComponent>();
    Collider->SetCollisionChannel(ECollisionChannel::Enemy);
    Collider->SetGenerateOverlapEvents(true);
    Collider->SetGenerateHitEvents(true);
    return Collider;
}

void AEnemyBaseActor::InitScriptComponent()
{
    UScriptComponent* EnemyScriptComponent = AddComponent<UScriptComponent>();
    EnemyScriptComponent->SetScriptPath("EnemyScript.lua");

    UScriptComponent* StateScriptComponent = AddComponent<UScriptComponent>();
    StateScriptComponent->SetScriptPath("Components/EnemyState.lua");
}

void AEnemyBaseActor::InitDefaultComponents()
{
    ColliderComponent = GetDefaultColliderComponent();
    SetRootComponent(ColliderComponent);

	MeshComponent = GetDefaultMeshComponent();
    ColliderComponent->AddChild(MeshComponent);

	InitScriptComponent();
}
