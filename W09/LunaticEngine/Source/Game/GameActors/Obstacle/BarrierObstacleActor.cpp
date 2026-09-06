#include "BarrierObstacleActor.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"

#include <algorithm>
#include <random>

IMPLEMENT_CLASS(ABarrierObstacleActor, AObstacleActorBase)

namespace {
	std::mt19937& RandomEngine()
	{
		static std::mt19937 Engine(std::random_device{}());
		return Engine;
	}

	uint8 GetRandomN() {
		std::uniform_int_distribution<int> Distribution(0, 2);
		return Distribution(RandomEngine());
	}

	constexpr const char* DefaultMeshPath[3] = { "Default.Mesh.BasicShape.Cube", "Default.Mesh.BasicShape.Sphere", "Default.Mesh.BasicShape.Cylinder" };
}

void ABarrierObstacleActor::InitDefaultComponents(const FString& UStaticMeshFileName) {
	FString MeshPath = UStaticMeshFileName;
	FString Path = DefaultMeshPath[GetRandomN()];
	if (MeshPath.empty() || MeshPath == "None") {
		if (const FMeshResource* Res = FResourceManager::Get().FindMesh(Path)) {
			MeshPath = Res->Path;
		}
	}
	Super::InitDefaultComponents(MeshPath);
}
