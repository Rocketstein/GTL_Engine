#include "MustSlideObstacleActor.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(AMustSlideObstacleActor, AObstacleActorBase)

void AMustSlideObstacleActor::InitDefaultComponents(const FString& UStaticMeshFileName) {
	FString MeshPath = UStaticMeshFileName;
	if (MeshPath.empty() || MeshPath == "None") {
		if (const FMeshResource* Res = FResourceManager::Get().FindMesh("Game.Mesh.Obstacle.MustJumpOrSlide0")) {
			MeshPath = Res->Path;
		}
	}

	SetCollisionBoxExtent(FVector(1.5f, 6.f, 0.45f));
	SetCollisionBoxOffset(FVector(-0.3f, 0, 0.3f));
	Super::InitDefaultComponents(MeshPath);
}
