#pragma once

#include "Core/Containers/Array.h"
#include "Core/CoreMinimal.h"
#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Geometry/Primitives/Ray.h"
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <utility>

using namespace Geometry;

struct FVectorInt
{
    int X = 0;
    int Y = 0;
    int Z = 0;

    FVectorInt() = default;
    FVectorInt(int InX, int InY, int InZ) : X(InX), Y(InY), Z(InZ) {}

    bool operator==(const FVectorInt &Other) const
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z;
    }

    bool operator!=(const FVectorInt &Other) const { return !(*this == Other); }
};

struct FGridObject
{
    FAABB Bounds;
    void *UserData = nullptr;
};

class FDynamicSpatialGrid
{
  private:
    struct FVoxelTraversalState
    {
        FVectorInt Current;

        int StepX = 0;
        int StepY = 0;
        int StepZ = 0;

        float tMaxX = FLT_MAX;
        float tMaxY = FLT_MAX;
        float tMaxZ = FLT_MAX;

        float tDeltaX = FLT_MAX;
        float tDeltaY = FLT_MAX;
        float tDeltaZ = FLT_MAX;

        float EntryT = 0.0f;
        float ExitT = 0.0f;
    };

    struct FBuildStats
    {
        int OccupiedCellCount = 0;
        int TotalElementRefs = 0;
        int MaxElementRefsPerCell = 0;
    };

  private:
    FVector3 Min = FVector3(0, 0, 0);
    FVector3 Max = FVector3(0, 0, 0);
    FVector3 IdealRatio = FVector3(1, 1, 1);

    FVectorInt MinInt = FVectorInt(0, 0, 0);
    FVectorInt MaxInt = FVectorInt(0, 0, 0);

    // Dense contiguous cell storage. Each flat cell owns a candidate list.
    TArray<TArray<int>> Cells;
    TArray<FGridObject> Objects;
    TArray<uint32_t>    VisitStamps;

    int      DimX = 0;
    int      DimY = 0;
    int      DimZ = 0;
    uint32_t CurrentVisitStamp = 1;

  private:
    static constexpr float Epsilon = 1e-8f;

  private:
    int         FloorToInt(float Value) const { return static_cast<int>(std::floor(Value)); }
    int         CeilToInt(float Value) const { return static_cast<int>(std::ceil(Value)); }
    float       ClampPositiveCellSize(float Value) const;
    FVector3    SanitizeCellSize(const FVector3 &InCellSize) const;
    int         GetCellIndex(int X, int Y, int Z) const;
    int         GetCellIndex(const FVectorInt &Cell) const;
    FVectorInt  WorldToCell(const FVector3 &InPosition) const;
    FVector3    CellMinToWorld(const FVectorInt &InCell) const;
    bool        IsValidCell(const FVectorInt &InCell) const;
    FBuildStats GetBuildStats() const;
    float       GetBuildScore(float XCount, float YCount, float ZCount) const;
    uint32_t    BeginVisitQuery();
    bool        IntersectRayAABB(const FVector3 &InMin, const FVector3 &InMax, const FRay &InRay,
                                 float &OutEntryT, float &OutExitT) const;
    bool        IntersectRayAABB(const FVector3 &InMin, const FVector3 &InMax, const FRay &InRay,
                                 float &OutHitT) const;
    bool        InitializeVoxelTraversal(const FRay &InRay, FVoxelTraversalState &OutState) const;
    bool        GetNextRayPoint(FVoxelTraversalState &InOutState) const;

  public:
    void Clear();
    void Initialize(const FVector3 &InMin, const FVector3 &InMax, const FVector3 &InIdealRatio);
    void InitializeDynamic(const TArray<FGridObject> &InArray);
    void Insert(const FGridObject &Item);
    FGridObject *CastRay(const FRay &InRay);
    void        *CastRayUserData(const FRay &InRay);
    TArray<void *> CastRayAll(const FRay &InRay);
    bool IsCollideRay(const FVector3 &InMin, const FVector3 &InMax, const FRay &InRay) const;
};
