#include "GroundEnemyActor.h"

#include "Component/StaticMeshComponent.h"
#include "Component/Collision/CircleCollider2DComponent.h"
#include "Mesh/ObjManager.h"
#include "Runtime/Engine.h"

IMPLEMENT_CLASS(AGroundEnemyActor, AEnemyBaseActor)

UStaticMeshComponent* AGroundEnemyActor::GetDefaultMeshComponent()
{
    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* Asset = FObjManager::Get().Load(FPaths::ContentRelativePath("Models/ArmyMan/ArmyMan-posed.obj"));
    
    UStaticMeshComponent* MeshComp = AddComponent<UStaticMeshComponent>();
    MeshComp->SetStaticMesh(Asset);
    MeshComp->SetRelativeScale(FVector(2.0f));
    return MeshComp;
}

UCollider2DComponent* AGroundEnemyActor::GetDefaultColliderComponent()
{
    UCircleCollider2DComponent* Collider = AddComponent<UCircleCollider2DComponent>();
    Collider->SetCollisionChannel(ECollisionChannel::Enemy);
    Collider->SetGenerateOverlapEvents(true);
    Collider->SetRadius(0.5f);
    return Collider;
}