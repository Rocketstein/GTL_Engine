#include "GameFramework/StaticMeshActor.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"

IMPLEMENT_CLASS(AStaticMeshActor, AActor)

void AStaticMeshActor::InitDefaultComponents(const FString& UStaticMeshFileName)
{
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	StaticMeshComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(StaticMeshComponent);

	if (!UStaticMeshFileName.empty() && UStaticMeshFileName != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(UStaticMeshFileName, Device);
		StaticMeshComponent->SetStaticMesh(Asset);

		if (Asset && IsBasicShapeAssetPath(UStaticMeshFileName))
		{
			const FString DefaultShapeMaterialPath = FResourceManager::Get().ResolvePath(FName("Default.Material.BasicShape"));
			if (UMaterial* DefaultShapeMaterial = FMaterialManager::Get().GetOrCreateMaterial(DefaultShapeMaterialPath))
			{
				int32 MaterialCount = static_cast<int32>(Asset->GetStaticMaterials().size());
				if (MaterialCount == 0 && Asset->GetStaticMeshAsset() &&
					(!Asset->GetStaticMeshAsset()->Sections.empty() || !Asset->GetStaticMeshAsset()->Indices.empty()))
				{
					MaterialCount = 1;
				}
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
				{
					StaticMeshComponent->SetMaterial(MaterialIndex, DefaultShapeMaterial);
				}
			}
		}
	}
	else
	{
		StaticMeshComponent->SetStaticMesh(nullptr);
	}

	// UUID 텍스트 표시
	//TextRenderComponent = AddComponent<UTextRenderComponent>();
	//TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.3f));
	//TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
	//TextRenderComponent->AttachToComponent(StaticMeshComponent);
	//TextRenderComponent->SetFont(FName("Default"));

	// SubUV 파티클
	//SubUVComponent = AddComponent<USubUVComponent>();
	//SubUVComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	//SubUVComponent->SetParticle(FName("Explosion"));
	//SubUVComponent->AttachToComponent(StaticMeshComponent);
	//SubUVComponent->SetVisibility(true);
}

bool AStaticMeshActor::IsBasicShapeAssetPath(const FString& Path) {
	const char* BasicShapeMeshKeys[] = {
				"Default.Mesh.BasicShape.Cone",
				"Default.Mesh.BasicShape.Cube",
				"Default.Mesh.BasicShape.Cylinder",
				"Default.Mesh.BasicShape.Plane",
				"Default.Mesh.BasicShape.Sphere",
				"Default.Mesh.BasicShape.SphereLowpoly"
	};

	for (const char* MeshKey : BasicShapeMeshKeys)
	{
		if (const FMeshResource* MeshResource = FResourceManager::Get().FindMesh(FName(MeshKey)))
		{
			if (MeshResource->Path == Path)
			{
				return true;
			}
		}
	}

	return false;
}