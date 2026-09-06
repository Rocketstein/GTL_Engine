#include "Scene/SceneAssetBinder.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Asset/AssetObjectManager.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Scene/Scene.h"

void FSceneAssetBinder::BindScene(FScene *InScene, FAssetObjectManager *InAssetObjectManager)
{
    if (InScene == nullptr || InAssetObjectManager == nullptr || !InAssetObjectManager->IsReady())
    {
        UE_LOG(SceneBinder, ELogLevel::Warning,
               "BindScene skipped: scene=%p assetManager=%p ready=%d", InScene,
               InAssetObjectManager,
               (InAssetObjectManager != nullptr && InAssetObjectManager->IsReady()) ? 1 : 0);
        return;
    }

    UE_LOG(SceneBinder, ELogLevel::Info, "Binding scene assets. StaticMeshComponent count=%zu",
           InScene->GetStaticMeshComponents().size());

    for (UStaticMeshComponent *Component : InScene->GetStaticMeshComponents())
    {
        BindStaticMeshComponent(Component, InAssetObjectManager);
    }

    UE_LOG(SceneBinder, ELogLevel::Info, "Scene asset binding completed.");
}

void FSceneAssetBinder::BindStaticMeshComponent(UStaticMeshComponent *InComponent,
                                                FAssetObjectManager  *InAssetObjectManager)
{
    if (InComponent == nullptr || InAssetObjectManager == nullptr ||
        !InAssetObjectManager->IsReady())
    {
        return;
    }

    const FString MeshPath = InComponent->GetStaticMeshPath();
    UE_LOG(SceneBinder, ELogLevel::Debug, "Binding static mesh component: path=%s",
           MeshPath.c_str());
    if (MeshPath.empty())
    {
        InComponent->SetStaticMeshAsset(nullptr);
        UE_LOG(SceneBinder, ELogLevel::Warning, "StaticMeshComponent has empty mesh path");
        return;
    }

    UStaticMesh *StaticMeshAsset = InAssetObjectManager->LoadStaticMeshObject(MeshPath);
    if (StaticMeshAsset == nullptr)
    {
        InComponent->SetStaticMeshAsset(nullptr);
        UE_LOG(SceneBinder, ELogLevel::Error, "Failed to load static mesh object: %s",
               MeshPath.c_str());
        return;
    }

    InComponent->SetStaticMeshAsset(StaticMeshAsset);
}
