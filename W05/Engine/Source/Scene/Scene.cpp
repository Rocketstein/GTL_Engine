#include "Scene/Scene.h"
#include "Scene/SceneDrawSort.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/EngineStatics.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

FSceneBVHSoA::FSceneBVHSoA(FSceneBVHSoA &&Other) noexcept { *this = std::move(Other); }
FSceneBVHSoA &FSceneBVHSoA::operator=(FSceneBVHSoA &&Other) noexcept
{
    if (this != &Other)
    {
        Free();
        TotalCount = Other.TotalCount;
        PaddedCount = Other.PaddedCount;
        MinX = Other.MinX;
        MaxX = Other.MaxX;
        MinY = Other.MinY;
        MaxY = Other.MaxY;
        MinZ = Other.MinZ;
        MaxZ = Other.MaxZ;
        DistanceSq = Other.DistanceSq;
        ComponentIndices = Other.ComponentIndices;
        Other.TotalCount = 0;
        Other.PaddedCount = 0;
        Other.MinX = Other.MaxX = Other.MinY = Other.MaxY = nullptr;
        Other.MinZ = Other.MaxZ = Other.DistanceSq = nullptr;
        Other.ComponentIndices = nullptr;
    }
    return *this;
}

void FSceneBVHSoA::Allocate(int32 InTotalCount, int32 InPaddedCount)
{
    Free();
    TotalCount = InTotalCount;
    PaddedCount = InPaddedCount;
    if (PaddedCount <= 0)
        return;
    MinX = new float[PaddedCount]();
    MaxX = new float[PaddedCount]();
    MinY = new float[PaddedCount]();
    MaxY = new float[PaddedCount]();
    MinZ = new float[PaddedCount]();
    MaxZ = new float[PaddedCount]();
    DistanceSq = new float[PaddedCount]();
    ComponentIndices = new uint32[PaddedCount]();
}

void FSceneBVHSoA::Free()
{
    delete[] MinX;
    delete[] MaxX;
    delete[] MinY;
    delete[] MaxY;
    delete[] MinZ;
    delete[] MaxZ;
    delete[] DistanceSq;
    delete[] ComponentIndices;
    MinX = MaxX = MinY = MaxY = MinZ = MaxZ = DistanceSq = nullptr;
    ComponentIndices = nullptr;
    TotalCount = 0;
    PaddedCount = 0;
}

void FScenePackedBoundsSOA::Clear()
{
    CenterX.clear();
    CenterY.clear();
    CenterZ.clear();
    ExtentX.clear();
    ExtentY.clear();
    ExtentZ.clear();
    ComponentIndices.clear();
}

void FScenePackedBoundsSOA::Reserve(size_t Count)
{
    CenterX.reserve(Count);
    CenterY.reserve(Count);
    CenterZ.reserve(Count);
    ExtentX.reserve(Count);
    ExtentY.reserve(Count);
    ExtentZ.reserve(Count);
    ComponentIndices.reserve(Count);
}


FScene::FScene() = default;
FScene::~FScene() { Clear(ESceneMemoryPolicy::ReleaseExcess); }

//void FScene::Clear()
//{
//    StaticMeshComponents.clear();
//    BoundsArray.clear();
//    CullPackedBounds.Clear();
//    PickPackedBounds.Clear();
//    SpatialQuerySoA.Free();
//    SpatialTree.Clear();
//    VisibleSet.clear();
//    DrawItems.clear();
//}

void FScene::Clear(ESceneMemoryPolicy MemoryPolicy)
{
    StaticMeshComponents.clear();
    BoundsArray.clear();
    CullPackedBounds.Clear();
    PickPackedBounds.Clear();
    SpatialTree.Clear();
    VisibleSet.clear();
    DrawItems.clear();

    // 메모리 누수를 막기 위해 명시적 해제
    SceneData.Free();
    BVH8Nodes.clear();
    SpatialQuerySoA.Free();
    RootNodeIndex = EMPTY_NODE;
    bIsSceneDirty = true;

    if (MemoryPolicy == ESceneMemoryPolicy::ReleaseExcess)
    {
        TArray<UStaticMeshComponent *>().swap(StaticMeshComponents);
        TArray<FSceneBoundsEntry>().swap(BoundsArray);
        TArray<FSceneVisibleItem>().swap(VisibleSet);
        TArray<FSceneDrawItem>().swap(DrawItems);
        TArray<uint32>().swap(SpatialTree.SortedBoundsIndices);
        TArray<FBVH8Node>().swap(SpatialTree.BVH8Nodes);
        std::vector<FBVH8Node>().swap(BVH8Nodes);

        TArray<float>().swap(CullPackedBounds.CenterX);
        TArray<float>().swap(CullPackedBounds.CenterY);
        TArray<float>().swap(CullPackedBounds.CenterZ);
        TArray<float>().swap(CullPackedBounds.ExtentX);
        TArray<float>().swap(CullPackedBounds.ExtentY);
        TArray<float>().swap(CullPackedBounds.ExtentZ);
        TArray<uint32>().swap(CullPackedBounds.ComponentIndices);

        TArray<float>().swap(PickPackedBounds.CenterX);
        TArray<float>().swap(PickPackedBounds.CenterY);
        TArray<float>().swap(PickPackedBounds.CenterZ);
        TArray<float>().swap(PickPackedBounds.ExtentX);
        TArray<float>().swap(PickPackedBounds.ExtentY);
        TArray<float>().swap(PickPackedBounds.ExtentZ);
        TArray<uint32>().swap(PickPackedBounds.ComponentIndices);

        ReleaseComponentPool();
    }
    else
    {
        ResetComponentPoolUsage();
    }
}

void FScene::MoveFrom(FScene &&Other, ESceneMemoryPolicy ExistingScenePolicy)
{
    if (this == &Other)
    {
        return;
    }

    Clear(ExistingScenePolicy);

    StaticMeshComponents = std::move(Other.StaticMeshComponents);
    ComponentChunks = std::move(Other.ComponentChunks);
    BoundsArray = std::move(Other.BoundsArray);
    CullPackedBounds = std::move(Other.CullPackedBounds);
    PickPackedBounds = std::move(Other.PickPackedBounds);
    SpatialQuerySoA = std::move(Other.SpatialQuerySoA);
    SpatialTree = std::move(Other.SpatialTree);
    VisibleSet = std::move(Other.VisibleSet);
    DrawItems = std::move(Other.DrawItems);
    SceneData = std::move(Other.SceneData);
    BVH8Nodes = std::move(Other.BVH8Nodes);
    RootNodeIndex = Other.RootNodeIndex;
    bIsSceneDirty = Other.bIsSceneDirty;
}

void FScene::ReserveStaticMeshComponents(size_t Count)
{
    StaticMeshComponents.reserve(Count);
    ReserveComponentPool(Count);
}

UStaticMeshComponent *FScene::AllocateStaticMeshComponent()
{
    for (FComponentChunk &Chunk : ComponentChunks)
    {
        if (Chunk.Used < Chunk.Capacity)
        {
            return &Chunk.Storage[Chunk.Used++];
        }
    }

    const size_t NewChunkSize =
        std::max(DefaultComponentChunkSize,
                 ComponentChunks.empty() ? size_t(1) : ComponentChunks.back().Capacity * 2);

    FComponentChunk NewChunk;
    NewChunk.Storage = std::make_unique<UStaticMeshComponent[]>(NewChunkSize);
    NewChunk.Capacity = NewChunkSize;
    NewChunk.Used = 1;
    NewChunk.AllocatedBytes = NewChunkSize * sizeof(UStaticMeshComponent);
    UEngineStatics::TotalAllocationCount++;
    UEngineStatics::TotalAllocatedBytes += static_cast<uint32>(NewChunk.AllocatedBytes);
    UStaticMeshComponent *Component = &NewChunk.Storage[0];
    ComponentChunks.push_back(std::move(NewChunk));
    return Component;
}

uint32 FScene::AddStaticMeshComponent(UStaticMeshComponent *InComponent)
{
    if (InComponent == nullptr)
    {
        return static_cast<uint32>(StaticMeshComponents.size());
    }
    StaticMeshComponents.push_back(InComponent);
    return static_cast<uint32>(StaticMeshComponents.size() - 1);
}

bool FScene::RemoveStaticMeshComponent(uint32 ComponentIndex)
{
    if (ComponentIndex >= StaticMeshComponents.size())
        return false;
    StaticMeshComponents.erase(StaticMeshComponents.begin() + ComponentIndex);
    return true;
}

void FScene::ReserveComponentPool(size_t Count)
{
    size_t AvailableSlots = 0;
    for (const FComponentChunk &Chunk : ComponentChunks)
    {
        AvailableSlots += (Chunk.Capacity - Chunk.Used);
    }

    if (AvailableSlots >= Count)
    {
        return;
    }

    const size_t Required = Count - AvailableSlots;
    FComponentChunk NewChunk;
    NewChunk.Storage = std::make_unique<UStaticMeshComponent[]>(Required);
    NewChunk.Capacity = Required;
    NewChunk.Used = 0;
    NewChunk.AllocatedBytes = Required * sizeof(UStaticMeshComponent);
    UEngineStatics::TotalAllocationCount++;
    UEngineStatics::TotalAllocatedBytes += static_cast<uint32>(NewChunk.AllocatedBytes);
    ComponentChunks.push_back(std::move(NewChunk));
}

void FScene::ResetComponentPoolUsage()
{
    for (FComponentChunk &Chunk : ComponentChunks)
    {
        Chunk.Used = 0;
    }
}

void FScene::ReleaseComponentPool()
{
    for (const FComponentChunk &Chunk : ComponentChunks)
    {
        if (Chunk.AllocatedBytes > 0)
        {
            UEngineStatics::TotalAllocationCount--;
            UEngineStatics::TotalAllocatedBytes -= static_cast<uint32>(Chunk.AllocatedBytes);
        }
    }

    std::vector<FComponentChunk>().swap(ComponentChunks);
}

void FScene::RebuildBoundsArray()
{
    BoundsArray.clear();
    BoundsArray.reserve(StaticMeshComponents.size());
    for (uint32 i = 0; i < StaticMeshComponents.size(); ++i)
    {
        FSceneBoundsEntry Entry{};
        Entry.ComponentIndex = i;
        if (StaticMeshComponents[i] != nullptr)
        {
            Entry.WorldBounds = StaticMeshComponents[i]->GetWorldAABB();
        }
        BoundsArray.push_back(Entry);
    }
}

void FScene::RebuildPackedBounds()
{
    CullPackedBounds.Clear();
    PickPackedBounds.Clear();
    CullPackedBounds.Reserve(BoundsArray.size());
    PickPackedBounds.Reserve(BoundsArray.size());
    for (const FSceneBoundsEntry &Entry : BoundsArray)
    {
        const FVector3 Center((Entry.WorldBounds.Min.X + Entry.WorldBounds.Max.X) * 0.5f,
                              (Entry.WorldBounds.Min.Y + Entry.WorldBounds.Max.Y) * 0.5f,
                              (Entry.WorldBounds.Min.Z + Entry.WorldBounds.Max.Z) * 0.5f);
        const FVector3 Extent((Entry.WorldBounds.Max.X - Entry.WorldBounds.Min.X) * 0.5f,
                              (Entry.WorldBounds.Max.Y - Entry.WorldBounds.Min.Y) * 0.5f,
                              (Entry.WorldBounds.Max.Z - Entry.WorldBounds.Min.Z) * 0.5f);
        CullPackedBounds.CenterX.push_back(Center.X);
        CullPackedBounds.CenterY.push_back(Center.Y);
        CullPackedBounds.CenterZ.push_back(Center.Z);
        CullPackedBounds.ExtentX.push_back(Extent.X);
        CullPackedBounds.ExtentY.push_back(Extent.Y);
        CullPackedBounds.ExtentZ.push_back(Extent.Z);
        CullPackedBounds.ComponentIndices.push_back(Entry.ComponentIndex);
        PickPackedBounds = CullPackedBounds;
    }
}

void FScene::RebuildSpatialTree()
{
    SpatialTree.Clear();
    for (uint32 i = 0; i < BoundsArray.size(); ++i)
    {
        SpatialTree.SortedBoundsIndices.push_back(i);
    }
    SpatialQuerySoA.Allocate((int32)BoundsArray.size(), (int32)BoundsArray.size());
}

void FScene::ResetFrameData()
{
    VisibleSet.clear();
    DrawItems.clear();
}

void FScene::BuildDrawItemsFromVisibleSet()
{
    DrawItems.clear();
    DrawItems.reserve(VisibleSet.size());
    for (uint32 VisibleRank = 0; VisibleRank < VisibleSet.size(); ++VisibleRank)
    {
        const FSceneVisibleItem &Item = VisibleSet[VisibleRank];
        FSceneDrawItem DrawItem{};
        DrawItem.ComponentIndex = Item.ComponentIndex;
        if (Item.ComponentIndex < StaticMeshComponents.size())
        {
            DrawItem.SortKey =
                SceneSort::MakeOpaqueSortKey(StaticMeshComponents[Item.ComponentIndex], VisibleRank);
        }
        else
        {
            DrawItem.SortKey = SceneSort::MakeOpaqueSortKey(nullptr, VisibleRank);
        }
        DrawItems.push_back(DrawItem);
    }

    SceneSort::SortOpaqueDrawItems(DrawItems);
}

//void FScene::Tick(float)
//{
//    ResetFrameData();
//    // 임시: 현재는 무조건 다 그리도록 세팅 (프러스텀 컬링 구현 시 지워질 코드)
//    for (uint32 i = 0; i < StaticMeshComponents.size(); ++i)
//    {
//        FSceneVisibleItem Item{};
//        Item.ComponentIndex = i;
//        Item.SortKey = i;
//        VisibleSet.push_back(Item);
//    }
//    BuildDrawItemsFromVisibleSet();
//}

void FScene::Tick(float DeltaTime)
{
    // 1. 씬에 변화가 있었다면 (물체 이동, 추가, 삭제 등) TLAS(SoA) 재빌드!
    if (bIsSceneDirty)
    {
        // 기존 메모리 날리기
        SceneData.Free();
        BVH8Nodes.clear();

        // 현재 StaticMeshComponents들의 위치를 바탕으로 새로 굽기
        SceneData.BuildSoA(StaticMeshComponents, BVH8Nodes);
        if (!BVH8Nodes.empty())
        {
            RootNodeIndex = 0;
        }
        else if (!StaticMeshComponents.empty())
        {
            // 단일 오브젝트 리프 루트(~0 = -1)도 RaycastScene의 정상 입력으로 사용한다.
            RootNodeIndex = ~0;
        }
        else
        {
            RootNodeIndex = EMPTY_NODE;
        }

        // 다 구웠으니 상태 초기화
        bIsSceneDirty = false;

        UE_LOG(Scene, ELogLevel::Info, "Scene BVH8(TLAS) Rebuilt due to changes.");
    }
}
