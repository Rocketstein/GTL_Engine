#pragma once
#include "Engine/SpatialContainer/DynamicSpatialGrid.h"
#include "Engine/Component/StaticMeshComponent.h"

struct FSpatialContainer
{
    FDynamicSpatialGrid Grid;
    //
    //
    //
    //
    //

    void Initailize(TArray<UStaticMeshComponent* >& Components)
    {
        TArray<FGridObject> GridObjects;

        for (int i = 0;  i < Components.size(); ++i)
        {
            UStaticMeshComponent* &Component = Components[i];
            GridObjects.push_back(FGridObject{Component->GetWorldAABB(), &Component});
        }

        Grid.InitializeDynamic(GridObjects);
    }
};