#pragma once

#include "Asset/Cooked/ObjCookedData.h"
#include "Asset/Runtime/StaticMeshRenderResource.h"
#include "Core/Containers/Array.h"
#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Math/Vector3.h"
#include "Engine/Asset/Asset.h"
#include "Scene/SAH-BVH8/MeshBLAS.h"
#include <memory>

// BVH용
#include "Scene/SAH-BVH8/MeshBLAS.h"

class UMaterial;

struct FStaticMeshSection
{
    uint32 FirstIndex = 0;
    uint32 IndexCount = 0;
    uint32 MaterialIndex = 0;
};

struct FStaticMeshTrianglePrecompute
{
    FVector3        V0;
    FVector3        E1;
    FVector3        E2;
    Geometry::FAABB LocalBounds;

    uint32 Index0 = 0;
    uint32 Index1 = 0;
    uint32 Index2 = 0;
};

struct alignas(32) FStaticMeshTrianglePacket8
{
    float V0X[8] = {};
    float V0Y[8] = {};
    float V0Z[8] = {};

    float E1X[8] = {};
    float E1Y[8] = {};
    float E1Z[8] = {};

    float E2X[8] = {};
    float E2Y[8] = {};
    float E2Z[8] = {};

    uint32 TriangleIndices[8] = {};
    uint32 TriangleCount = 0;
};

struct FStaticMeshBVHNode
{
    Geometry::FAABB Bounds;

    int32 LeftChild = -1;
    int32 RightChild = -1;

    uint32 FirstTriangle = 0;
    uint32 TriangleCount = 0;

    bool IsLeaf() const { return LeftChild < 0 && RightChild < 0; }
};

struct FStaticMeshTriangleSoA
{
    TArray<float>  V0X, V0Y, V0Z;
    TArray<float>  E1X, E1Y, E1Z;
    TArray<float>  E2X, E2Y, E2Z;
    TArray<uint32> OriginalIndices;
};

struct FStaticMeshRaycastData
{
    TArray<FStaticMeshTrianglePrecompute> Triangles;
    TArray<uint32>                        TriangleIndices;
    TArray<FStaticMeshBVHNode>            BVHNodes;
    TArray<FStaticMeshTrianglePacket8>    LeafPackets;

    TArray<FStaticMeshBVH8Node> BVH8Nodes;
    FStaticMeshTriangleSoA      TriangleSoA;

void Reset()
    {
        Triangles.clear();
        TriangleIndices.clear();
        BVHNodes.clear();
        LeafPackets.clear();
        // ⭐ 초기화 추가
        BVH8Nodes.clear();
        TriangleSoA.V0X.clear();
        TriangleSoA.V0Y.clear();
        TriangleSoA.V0Z.clear();
        TriangleSoA.E1X.clear();
        TriangleSoA.E1Y.clear();
        TriangleSoA.E1Z.clear();
        TriangleSoA.E2X.clear();
        TriangleSoA.E2Y.clear();
        TriangleSoA.E2Z.clear();
        TriangleSoA.OriginalIndices.clear();
    }

    bool IsValid() const { return Triangles.empty() == false; }
};



class UStaticMesh : public UAsset
{
    DECLARE_RTTI(UStaticMesh, UAsset)

  public:
    void BuildFromCookedData(const Asset::FObjCookedData &InCooked);

    const TArray<uint8>              &GetVerticesData() const { return VertexData; }
    const TArray<uint32>             &GetIndicesData() const { return Indices; }
    const TArray<FStaticMeshSection> &GetSections() const { return Sections; }
    const TArray<UMaterial *>        &GetMaterialSlots() const { return MaterialSlots; }
    TArray<UMaterial *>              &GetMaterialSlots() { return MaterialSlots; }

    uint32 GetVertexStride() const { return VertexStride; }
    uint32 GetVerticesCount() const { return VertexCount; }
    uint32 GetIndicesCount() const { return static_cast<uint32>(Indices.size()); }

    const Geometry::FAABB &GetAABB() const { return Bounds; }

    const std::shared_ptr<Asset::FStaticMeshRenderResource> &GetRenderResource() const
    {
        return RenderResource;
    }
    void SetRenderResource(std::shared_ptr<Asset::FStaticMeshRenderResource> InResource)
    {
        RenderResource = std::move(InResource);
    }

    const std::shared_ptr<Asset::FObjCookedData> &GetCookedData() const { return CookedData; }
    void SetCookedData(std::shared_ptr<Asset::FObjCookedData> InCookedData)
    {
        CookedData = std::move(InCookedData);
    }

    const FStaticMeshRaycastData &GetRaycastData() const { return RaycastData; }
    FStaticMeshRaycastData       &GetRaycastData() { return RaycastData; }

    bool HasRaycastData() const { return RaycastData.IsValid(); }

    bool IsValidLowLevel() const override;

    // BVH용
    const FMeshBLAS &GetBLAS() const { return MeshBLAS; }

  private:
    void BuildBounds();
    void BuildRaycastData();
    void BuildTrianglePrecomputeData();
    void BuildBVH();
    void BuildLeafPackets();

    void CompressBVHTo8();
    int  CollapseRecursive(int CurrentNodeIdx);

  private:
    uint32 VertexStride = 0;
    uint32 VertexCount = 0;

    TArray<uint8>              VertexData;
    TArray<uint32>             Indices;
    TArray<FStaticMeshSection> Sections;
    TArray<UMaterial *>        MaterialSlots;

    Geometry::FAABB                                   Bounds;
    std::shared_ptr<Asset::FStaticMeshRenderResource> RenderResource;
    std::shared_ptr<Asset::FObjCookedData>            CookedData;

    FStaticMeshRaycastData RaycastData;

    // BVH용
    FMeshBLAS MeshBLAS;
};