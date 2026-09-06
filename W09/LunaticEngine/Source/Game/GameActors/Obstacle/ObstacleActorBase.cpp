#include "ObstacleActorBase.h"  
#include "Component/Shape/BoxComponent.h"  
#include "Component/Shape/SphereComponent.h"  
#include "Component/Shape/CapsuleComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Runtime/Engine.h"

#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"

#include <random>

DEFINE_CLASS(AObstacleActorBase, AStaticMeshActor)

namespace {
	std::mt19937& RandomEngine()
	{
		static std::mt19937 Engine(std::random_device{}());
		return Engine;
	}

	uint8 GetRandomN() {
		std::uniform_int_distribution<int> Distribution(0, 4);
		return Distribution(RandomEngine());
	}

	constexpr const char* DefaultMeshPath[5] = { "Sample.Material.Brick", "Sample.Material.RedGrid", "Sample.Material.Wood", "Sample.Material.Checker", "Sample.Material.Metal" };
}

void AObstacleActorBase::BeginPlay() {
	Super::BeginPlay();
	for (UActorComponent* Comp : OwnedComponents)
	{
		if (UShapeComponent* Shape = Cast<UShapeComponent>(Comp))
		{
			Shape->OnComponentBeginOverlap.AddDynamic(this, &AObstacleActorBase::OnOverlap);
			Shape->OnComponentHit.AddDynamic(this, &AObstacleActorBase::OnHit);
		}
	}
}

void AObstacleActorBase::EndPlay() {
	Super::EndPlay();
}

void AObstacleActorBase::OnHit(const FComponentHitEvent& Other) {
	if (!Other.HitComponent) return;

}

void AObstacleActorBase::OnOverlap(const FComponentOverlapEvent& Other) {
	if (!Other.OtherComponent) return;
}

void AObstacleActorBase::InitDefaultComponents(const FString& UStaticMeshFileName) {
	UBoxComponent* CollisionBox = AddComponent<UBoxComponent>();
	CollisionBox->SetCanDeleteFromDetails(false);
	CollisionBox->AttachToComponent(StaticMeshComponent);
	CollisionBox->SetBoxExtent(CollisionBoxExtent);
	CollisionBox->SetRelativeLocation(CollisionBoxOffset);
	CollisionBox->SetCollisionEnabled(true);
	CollisionBox->SetGenerateOverlapEvents(true);
	SetRootComponent(CollisionBox);
	if (HasActorBegunPlay())
	{
		CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AObstacleActorBase::OnOverlap);
		CollisionBox->OnComponentHit.AddDynamic(this, &AObstacleActorBase::OnHit);
	}
	
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	StaticMeshComponent->SetCanDeleteFromDetails(false);
	StaticMeshComponent->SetCollisionEnabled(false);
	StaticMeshComponent->AttachToComponent(CollisionBox);

	if (!UStaticMeshFileName.empty() && UStaticMeshFileName != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(UStaticMeshFileName, Device);
		StaticMeshComponent->SetStaticMesh(Asset);

		if (Asset)
		{
			const FString MaterialPath = DefaultMeshPath[GetRandomN()];
			const FString RedGridMaterialPath = FResourceManager::Get().ResolvePath(FName(MaterialPath));
			if (UMaterial* RedGridMaterial = FMaterialManager::Get().GetOrCreateMaterial(RedGridMaterialPath))
			{
				int32 MaterialCount = static_cast<int32>(Asset->GetStaticMaterials().size());
				if (MaterialCount == 0 && Asset->GetStaticMeshAsset() &&
					(!Asset->GetStaticMeshAsset()->Sections.empty() || !Asset->GetStaticMeshAsset()->Indices.empty()))
				{
					MaterialCount = 1;
				}
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
				{
					StaticMeshComponent->SetMaterial(MaterialIndex, RedGridMaterial);
				}
			}
		}
	}
	else
	{
		StaticMeshComponent->SetStaticMesh(nullptr);
	}
}
