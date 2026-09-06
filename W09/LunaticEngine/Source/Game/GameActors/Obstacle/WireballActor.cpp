#include "WireballActor.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(AWireballActor, AObstacleActorBase)

void AWireballActor::InitDefaultComponents(const FString& UStaticMeshFileName) {
	FString MeshPath = UStaticMeshFileName;
	if (MeshPath.empty() || MeshPath == "None") {
		if (const FMeshResource* Res = FResourceManager::Get().FindMesh(FName("Game.Mesh.Obstacle.Wireball")))
			MeshPath = Res->Path;
	}
	Super::InitDefaultComponents(MeshPath);
}
