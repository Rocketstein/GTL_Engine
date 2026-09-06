#pragma once
#include "GameFramework/AActor.h"
#include "Mesh/StaticMesh.h"

class UCollider2DComponent;
class UStaticMeshComponent;
class UScriptComponent;

class AEnemyBaseActor : public AActor
{
public:
    DECLARE_CLASS(AEnemyBaseActor, AActor)
    AEnemyBaseActor() = default;
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void InitDefaultComponents() override;

protected:
    virtual UStaticMeshComponent* GetDefaultMeshComponent();
    virtual UCollider2DComponent* GetDefaultColliderComponent();
    void InitScriptComponent();

    UCollider2DComponent* ColliderComponent = nullptr;
    UStaticMeshComponent* MeshComponent = nullptr;
};
