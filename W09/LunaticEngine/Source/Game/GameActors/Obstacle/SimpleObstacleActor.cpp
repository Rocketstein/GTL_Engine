#include "SimpleObstacleActor.h"
#include "Engine/Component/Shape/BoxComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Resource/ResourceManager.h"

IMPLEMENT_CLASS(ASimpleObstacleActor, AObstacleActorBase)

void ASimpleObstacleActor::InitDefaultComponents(const FString& UStaticMeshFileName) {
	FString MeshPath = UStaticMeshFileName;
	if (MeshPath.empty() || MeshPath == "None") {
		if (const FMeshResource* Res = FResourceManager::Get().FindMesh(FName("Default.Mesh.BasicShape.Cube")))
			MeshPath = Res->Path;
	}
	Super::InitDefaultComponents(MeshPath);
}
