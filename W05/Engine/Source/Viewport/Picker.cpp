#include "Picker.h"
#include "Core/Geometry/Intersection.h"
#include "Core/Geometry/Primitives/Ray.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Component/StaticMeshComponent.h"

bool FPicker::RayCastDefault(Geometry::FRay InRay, FRayResult &OutResult)
{
    const TArray<UStaticMeshComponent *> &Targets = Scene->GetStaticMeshComponents();

    float                 ClosestDistance = (std::numeric_limits<float>::max)();
    UStaticMeshComponent *ClosestComponent = nullptr;

    for (UStaticMeshComponent *Component : Targets)
    {

        const FStaticMeshRaycastData                  &RayData = Component->GetStaticMesh()->GetRaycastData();
        const TArray<FStaticMeshTrianglePrecompute> &Triangles = RayData.Triangles;

        Geometry::FRay LocalRay =
            Geometry::FRay(Component->GetRelativeTransform().InverseTransformPosition(InRay.Origin),
                           Component->GetRelativeTransform()
                               .InverseTransformVector(InRay.Direction)
                               .GetSafeNormal());

        for (int i = 0; i < Triangles.size(); ++i)
        {
            float Distance = 0.0f;
            if (Geometry::IntersectRayTriangle(LocalRay.Origin, LocalRay.Direction, Triangles[i].V0,
                                               Triangles[i].E1, Triangles[i].E2, Distance) &&
                Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                ClosestComponent = Component;
            }
        }
    }

    if (!ClosestComponent)
        return false;

    OutResult.HitComponent = ClosestComponent;
    OutResult.Distance = ClosestDistance;
    return true;
}

bool FPicker::RayCastWithBVH(Geometry::FRay InRay, FRayResult &OutResult)
{
    // 占쏙옙占쏙옙 TLAS 占쏙옙占쏙옙체占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙
    FSceneDataSoA          *SoA = &Scene->GetSceneDataSoA();
    std::vector<FBVH8Node> &BVH8Nodes = Scene->GetBVH8Nodes();
    int                     RootNodeIndex = Scene->GetRootNodeIndex();

    const TArray<UStaticMeshComponent *> &Components = Scene->GetStaticMeshComponents();

    if (Components.empty())
        return false;

    if (RootNodeIndex == EMPTY_NODE)
        return false;

    if (RootNodeIndex >= 0 && BVH8Nodes.empty())
        return false;

    // 占싹쇽옙占쌔두쏙옙 RaycastScene 호占쏙옙!
    FRaycastHit Hit =
        RaycastScene(SoA, BVH8Nodes.data(), RootNodeIndex, InRay.Origin, InRay.Direction);

    OutResult.LeafObjectsVisited = Hit.LeafObjectsVisited;
    OutResult.LeafAABBRejected = Hit.LeafAABBRejected;
    OutResult.BLASTests = Hit.BLASTests;
    OutResult.BLASHits = Hit.BLASHits;

    if (Hit.bHit && Hit.HitObjectID >= 0)
    {
        OutResult.HitComponent = Components[Hit.HitObjectID];
        OutResult.Distance = Hit.Distance;
        // 占십울옙占싹다몌옙 OutResult.ComponentIndex 占쏙옙 Hit.HitTriangleIndex占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙 占쌍쏙옙占싹댐옙.
        return true;
    }

    return false;
}

void FPicker::CheckBLAS(const Geometry::FRay &WorldRay, UStaticMeshComponent *Component,
                        float &InOutClosestDistance, UStaticMeshComponent *&OutClosestComponent)
{
    UStaticMesh *Mesh = Component->GetStaticMesh();
    if (!Mesh || !Mesh->HasRaycastData())
        return;

    const FStaticMeshRaycastData &RayData = Mesh->GetRaycastData();
    if (RayData.BVHNodes.empty())
        return;

    // 1. Ray占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙환
    Geometry::FRay LocalRay =
        Geometry::FRay(Component->GetRelativeTransform().InverseTransformPosition(WorldRay.Origin),
                       Component->GetRelativeTransform()
                           .InverseTransformVector(WorldRay.Direction)
                           .GetSafeNormal());

    // BLAS 占쏙옙회占쏙옙 占쏙옙占쏙옙
    std::vector<int> BLASStack;
    BLASStack.reserve(64);
    BLASStack.push_back(0); // BLAS 占쏙옙트 占쏙옙占쏙옙 占쏙옙占쏙옙 0占쏙옙 占싸듸옙占쏙옙

    while (!BLASStack.empty())
    {
        int NodeIdx = BLASStack.back();
        BLASStack.pop_back();

        const FStaticMeshBVHNode &Node = RayData.BVHNodes[NodeIdx];

        // 2. 占쏙옙占쏙옙 Ray占쏙옙 占쏙옙占쏙옙占?AABB 占쏙옙占쏙옙 占싯삼옙 (占쏙옙占쏙옙화占쏙옙
        // 占쏙옙占쏙옙 Distance占쏙옙 InOutClosestDistance占쏙옙占쏙옙 占쌍몌옙 占쏙옙킵) float
        // BoxHitDistance; if (!Geometry::IntersectRayAABB(LocalRay, Node.Bounds, BoxHitDistance) ||
        // BoxHitDistance > InOutClosestDistance)
        //     continue;

        if (Node.IsLeaf())
        {
            // 占쏙옙占쏙옙 占쏙옙占? 占쏙옙占쏙옙 占쏙각占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙
            // 占싯삼옙
            for (uint32 i = 0; i < Node.TriangleCount; ++i)
            {
                uint32      TriIndex = RayData.TriangleIndices[Node.FirstTriangle + i];
                const auto &Tri = RayData.Triangles[TriIndex];

                float HitDistance = 0.0f;
                if (Geometry::IntersectRayTriangle(LocalRay.Origin, LocalRay.Direction, Tri.V0,
                                                   Tri.E1, Tri.E2, HitDistance))
                {
                    // 占싸몌옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占싹울옙 占쏙옙占쏙옙 占신몌옙
                    // 占쏙옙占쏙옙占쏙옙 占십울옙占쏙옙 占쏙옙 占쌍쏙옙占싹댐옙.
                    // (占싹뱄옙占쏙옙占쏙옙占쏙옙 Uniform Scale占싱몌옙 占신몌옙 占쏙옙환
                    // 占쏙옙占쏙옙)
                    if (HitDistance < InOutClosestDistance)
                    {
                        InOutClosestDistance = HitDistance;
                        OutClosestComponent = Component;
                    }
                }
            }
        }
        else
        {
            // 占쌘쏙옙 占쏙옙弱?占쏙옙占쏙옙占쏙옙 占쏙옙占시울옙 푸占쏙옙
            if (Node.LeftChild >= 0)
                BLASStack.push_back(Node.LeftChild);
            if (Node.RightChild >= 0)
                BLASStack.push_back(Node.RightChild);
        }
    }
}

bool FPicker::RayCastAABB(Geometry::FRay InRay, FRayResult &OutResult)
{
    const TArray<UStaticMeshComponent *> &Targets = Scene->GetStaticMeshComponents();

    float                 ClosestDistance = (std::numeric_limits<float>::max)();
    UStaticMeshComponent *ClosestComponent = nullptr;

    for (UStaticMeshComponent *Component : Targets)
    {
        if (!Geometry::IntersectRayAABB(InRay, Component->GetWorldAABB(), ClosestDistance))
            continue;

        const FStaticMeshRaycastData                  &RayData = Component->GetStaticMesh()->GetRaycastData();
        const TArray<FStaticMeshTrianglePrecompute> &Triangles = RayData.Triangles;

        Geometry::FRay LocalRay =
            Geometry::FRay(Component->GetRelativeTransform().InverseTransformPosition(InRay.Origin),
                           Component->GetRelativeTransform()
                               .InverseTransformVector(InRay.Direction)
                               .GetSafeNormal());

        for (int i = 0; i < Triangles.size(); ++i)
        {
            float Distance = 0.0f;
            if (Geometry::IntersectRayTriangle(LocalRay.Origin, LocalRay.Direction, Triangles[i].V0,
                                               Triangles[i].E1, Triangles[i].E2, Distance) &&
                Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                ClosestComponent = Component;
            }
        }
    }

    if (!ClosestComponent)
        return false;

    OutResult.HitComponent = ClosestComponent;
    OutResult.Distance = ClosestDistance;
    return true;

}
