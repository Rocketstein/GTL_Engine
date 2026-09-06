#include "PendulumObstacleActor.h"
#include "Component/Movement/PendulumMovementComponent.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(APendulumObstacleActor, AObstacleActorBase)

void APendulumObstacleActor::InitDefaultComponents(const FString& UStaticMeshFileName) {
	FString MeshPath = UStaticMeshFileName;
	if (MeshPath.empty() || MeshPath == "None") {
		if (const FMeshResource* Res = FResourceManager::Get().FindMesh("Default.Mesh.BasicShape.Sphere")) {
			MeshPath = Res->Path;
		}
	}

	Super::InitDefaultComponents(MeshPath);

	UPendulumMovementComponent* Pendulum = AddComponent<UPendulumMovementComponent>();
	if (!Pendulum) return;

	Pendulum->SetUpdatedComponent(RootComponent);
}
