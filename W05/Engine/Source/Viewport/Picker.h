#pragma once
#include "Core/CoreMinimal.h"
#include "Scene/Scene.h"
#include "Engine/Component/StaticMeshComponent.h"

struct FRayResult
{
    int   ComponentIndex;
    float Distance;
    UStaticMeshComponent *HitComponent;

    // Picking 경로 진단용 카운터
    uint32 LeafObjectsVisited = 0;
    uint32 LeafAABBRejected = 0;
    uint32 BLASTests = 0;
    uint32 BLASHits = 0;
};

class FPicker
{
    FScene *Scene;

  public:
    void SetScene(FScene *InScene) { Scene = InScene; }
    bool RayCastDefault(Geometry::FRay InRay, FRayResult &OutResult);


    bool RayCastWithBVH(Geometry::FRay InRay, FRayResult &OutResult);
    void CheckBLAS(const Geometry::FRay &WorldRay, UStaticMeshComponent *Component,
                   float &InOutClosestDistance, UStaticMeshComponent *&OutClosestComponent);
    bool RayCastAABB(Geometry::FRay InRay, FRayResult &OutResult);
};
