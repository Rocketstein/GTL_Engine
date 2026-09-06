#pragma once

class FScene;
class FAssetObjectManager;

class UStaticMeshComponent;

class FSceneAssetBinder
{
  public:
    static void BindScene(FScene *InScene, FAssetObjectManager *InAssetObjectManager);
    static void BindStaticMeshComponent(UStaticMeshComponent *InComponent,
                                        FAssetObjectManager  *InAssetObjectManager);
};
