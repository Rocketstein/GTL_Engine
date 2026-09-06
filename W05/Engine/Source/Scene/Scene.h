#pragma once

#include "Core/Containers/Array.h"
#include "Core/Geometry/Geometry.h"
#include "Core/Platform/PlatformTypes.h"
#include "SAH-BVH8/SceneRenderer.h" // FBVH8Node 등 헤더 포함
#include "SAH-BVH8/SceneSoA.h"      // 새로 만드신 SoA 헤더 포함
#include <memory>
#include <vector>


struct FSceneBoundsEntry
{
    uint32          ComponentIndex = 0;
    Geometry::FAABB WorldBounds;
};

struct FScenePackedBoundsSOA
{
    TArray<float>  CenterX;
    TArray<float>  CenterY;
    TArray<float>  CenterZ;
    TArray<float>  ExtentX;
    TArray<float>  ExtentY;
    TArray<float>  ExtentZ;
    TArray<uint32> ComponentIndices;

    void Clear();
    void Reserve(size_t Count);
    bool IsEmpty() const { return ComponentIndices.empty(); }
};

//struct alignas(32) FBVH8Node
//{
//    float MinX[8];
//    float MaxX[8];
//    float MinY[8];
//    float MaxY[8];
//    float MinZ[8];
//    float MaxZ[8];
//    int32 Children[8];
//};

struct FSceneBVHSoA
{
    int32 TotalCount = 0;
    int32 PaddedCount = 0;

    float  *MinX = nullptr;
    float  *MaxX = nullptr;
    float  *MinY = nullptr;
    float  *MaxY = nullptr;
    float  *MinZ = nullptr;
    float  *MaxZ = nullptr;
    float  *DistanceSq = nullptr;
    uint32 *ComponentIndices = nullptr;

    FSceneBVHSoA() = default;
    ~FSceneBVHSoA() { Free(); }

    FSceneBVHSoA(const FSceneBVHSoA &) = delete;
    FSceneBVHSoA &operator=(const FSceneBVHSoA &) = delete;

    FSceneBVHSoA(FSceneBVHSoA &&Other) noexcept;
    FSceneBVHSoA &operator=(FSceneBVHSoA &&Other) noexcept;

    void Clear() { Free(); }
    void Allocate(int32 InTotalCount, int32 InPaddedCount);
    void Free();
    bool IsEmpty() const
    {
        return TotalCount == 0 || PaddedCount == 0 || ComponentIndices == nullptr;
    }
};

struct FSceneSpatialTree
{
    // 하위 호환용 선형 정렬 인덱스
    TArray<uint32> SortedBoundsIndices;

    // BVH8 + SoA 기반 가속 구조
    TArray<FBVH8Node> BVH8Nodes;
    int32             RootNodeIndex = EMPTY_NODE;

    void Clear()
    {
        SortedBoundsIndices.clear();
        BVH8Nodes.clear();
        RootNodeIndex = EMPTY_NODE;
    }

    bool HasBVH8() const { return RootNodeIndex >= 0 && !BVH8Nodes.empty(); }
};

struct FSceneVisibleItem
{
    uint32 ComponentIndex = 0;
    uint64 SortKey = 0;
};

using FSceneDrawItem = FSceneVisibleItem;

enum class ESceneMemoryPolicy
{
    KeepCapacity,
    ReleaseExcess
};

class FScene
{
  public:
    FScene();
    ~FScene();

    FScene(const FScene &) = delete;
    FScene &operator=(const FScene &) = delete;

    void Clear(ESceneMemoryPolicy MemoryPolicy = ESceneMemoryPolicy::KeepCapacity);
    void MoveFrom(FScene &&Other,
                  ESceneMemoryPolicy ExistingScenePolicy = ESceneMemoryPolicy::ReleaseExcess);

    void   ReserveStaticMeshComponents(size_t Count);
    UStaticMeshComponent *AllocateStaticMeshComponent();
    uint32 AddStaticMeshComponent(UStaticMeshComponent *InComponent);
    bool   RemoveStaticMeshComponent(uint32 ComponentIndex);

    const TArray<UStaticMeshComponent *> &GetStaticMeshComponents() const
    {
        return StaticMeshComponents;
    }

    TArray<UStaticMeshComponent *> &GetStaticMeshComponents() { return StaticMeshComponents; }

    const TArray<FSceneBoundsEntry> &GetBoundsArray() const { return BoundsArray; }
    TArray<FSceneBoundsEntry>       &GetBoundsArray() { return BoundsArray; }

    const FScenePackedBoundsSOA &GetCullPackedBounds() const { return CullPackedBounds; }
    FScenePackedBoundsSOA       &GetCullPackedBounds() { return CullPackedBounds; }

    const FScenePackedBoundsSOA &GetPickPackedBounds() const { return PickPackedBounds; }
    FScenePackedBoundsSOA       &GetPickPackedBounds() { return PickPackedBounds; }

    const FSceneBVHSoA &GetSpatialQuerySoA() const { return SpatialQuerySoA; }
    FSceneBVHSoA       &GetSpatialQuerySoA() { return SpatialQuerySoA; }

    const FSceneSpatialTree &GetSpatialTree() const { return SpatialTree; }
    FSceneSpatialTree       &GetSpatialTree() { return SpatialTree; }

    const TArray<FSceneVisibleItem> &GetVisibleSet() const { return VisibleSet; }
    TArray<FSceneVisibleItem>       &GetVisibleSet() { return VisibleSet; }

    const TArray<FSceneDrawItem> &GetDrawItems() const { return DrawItems; }
    TArray<FSceneDrawItem>       &GetDrawItems() { return DrawItems; }

    void RebuildBoundsArray();
    void RebuildPackedBounds();
    void RebuildSpatialTree();
    void ResetFrameData();
    void BuildDrawItemsFromVisibleSet();

    // --- 새로 추가된 핵심 데이터 접근자 ---
    FSceneDataSoA          &GetSceneDataSoA() { return SceneData; }
    std::vector<FBVH8Node> &GetBVH8Nodes() { return BVH8Nodes; }
    int                     GetRootNodeIndex() const { return RootNodeIndex; }
    void                    SetRootNodeIndex(int InIndex) { RootNodeIndex = InIndex; }
    void                    MarkSceneDirty() { bIsSceneDirty = true; }
    bool                    IsSceneDirty() const { return bIsSceneDirty; }
    void                    ClearSceneDirty() { bIsSceneDirty = false; }

    void Tick(float DeltaTime);

  private:
    struct FComponentChunk
    {
        std::unique_ptr<UStaticMeshComponent[]> Storage;
        size_t                                  Capacity = 0;
        size_t                                  Used = 0;
        size_t                                  AllocatedBytes = 0;
    };

    void ReserveComponentPool(size_t Count);
    void ResetComponentPoolUsage();
    void ReleaseComponentPool();

    TArray<UStaticMeshComponent *> StaticMeshComponents;
    std::vector<FComponentChunk>   ComponentChunks;
    size_t                         DefaultComponentChunkSize = 256;
    TArray<FSceneBoundsEntry>      BoundsArray;
    FScenePackedBoundsSOA          CullPackedBounds;
    FScenePackedBoundsSOA          PickPackedBounds;
    FSceneBVHSoA                   SpatialQuerySoA;
    FSceneSpatialTree              SpatialTree;
    TArray<FSceneVisibleItem>      VisibleSet;
    TArray<FSceneDrawItem>         DrawItems;


    // --- 이전 임시 구조체들을 완벽히 대체하는 3총사 ---
    FSceneDataSoA          SceneData;
    std::vector<FBVH8Node> BVH8Nodes;
    int                    RootNodeIndex = EMPTY_NODE;
    bool                   bIsSceneDirty = true;
};
