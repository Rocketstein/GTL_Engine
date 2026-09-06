#include "SkeletalMeshActor.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshManager.h"

IMPLEMENT_CLASS(ASkeletalMeshActor, AActor)

void ASkeletalMeshActor::InitDefaultComponents()
{
    SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
    SetRootComponent(SkeletalMeshComponent);
}

void ASkeletalMeshActor::InitDefaultComponents(const FString& SkeletalMeshPath)
{
    InitDefaultComponents();

    USkeletalMesh* Asset = FSkeletalMeshManager::Get().Load(SkeletalMeshPath);
    SkeletalMeshComponent->SetSkeletalMesh(Asset);
}