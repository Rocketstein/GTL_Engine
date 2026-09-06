#include "DynamicSpatialGrid.h"
#include "Core/Geometry/Intersection.h"
#include <algorithm>

using namespace Geometry;

float FDynamicSpatialGrid::ClampPositiveCellSize(float Value) const
{
    return (std::abs(Value) < Epsilon) ? 1.0f : Value;
}

FVector3 FDynamicSpatialGrid::SanitizeCellSize(const FVector3 &InCellSize) const
{
    return FVector3(ClampPositiveCellSize(InCellSize.X), ClampPositiveCellSize(InCellSize.Y),
                    ClampPositiveCellSize(InCellSize.Z));
}

int FDynamicSpatialGrid::GetCellIndex(int X, int Y, int Z) const
{
    return X + Y * DimX + Z * DimX * DimY;
}

int FDynamicSpatialGrid::GetCellIndex(const FVectorInt &Cell) const
{
    return GetCellIndex(Cell.X, Cell.Y, Cell.Z);
}

FVectorInt FDynamicSpatialGrid::WorldToCell(const FVector3 &InPosition) const
{
    const FVector3 Local = InPosition - Min;

    return FVectorInt(FloorToInt(Local.X / IdealRatio.X), FloorToInt(Local.Y / IdealRatio.Y),
                      FloorToInt(Local.Z / IdealRatio.Z));
}

FVector3 FDynamicSpatialGrid::CellMinToWorld(const FVectorInt &InCell) const
{
    return FVector3(Min.X + InCell.X * IdealRatio.X, Min.Y + InCell.Y * IdealRatio.Y,
                    Min.Z + InCell.Z * IdealRatio.Z);
}

bool FDynamicSpatialGrid::IsValidCell(const FVectorInt &InCell) const
{
    return InCell.X >= 0 && InCell.X < DimX && InCell.Y >= 0 && InCell.Y < DimY && InCell.Z >= 0 &&
           InCell.Z < DimZ;
}

FDynamicSpatialGrid::FBuildStats FDynamicSpatialGrid::GetBuildStats() const
{
    FBuildStats Stats;

    for (const TArray<int> &CellObjects : Cells)
    {
        if (CellObjects.empty())
            continue;

        const int CellElementCount = static_cast<int>(CellObjects.size());
        ++Stats.OccupiedCellCount;
        Stats.TotalElementRefs += CellElementCount;
        Stats.MaxElementRefsPerCell = std::max(Stats.MaxElementRefsPerCell, CellElementCount);
    }

    return Stats;
}

float FDynamicSpatialGrid::GetBuildScore(float XCount, float YCount, float ZCount) const
{
    const FBuildStats Stats = GetBuildStats();
    const float       TraversalCost = XCount + YCount + ZCount;

    return TraversalCost + static_cast<float>(Stats.TotalElementRefs) +
           static_cast<float>(Stats.MaxElementRefsPerCell);
}

uint32_t FDynamicSpatialGrid::BeginVisitQuery()
{
    // Rare overflow path. Clearing the stamp array is cheaper than rebuilding
    // a hash set every ray cast.
    if (CurrentVisitStamp == UINT32_MAX)
    {
        std::fill(VisitStamps.begin(), VisitStamps.end(), 0u);
        CurrentVisitStamp = 1;
    }

    return CurrentVisitStamp++;
}

bool FDynamicSpatialGrid::IntersectRayAABB(const FVector3 &InMin, const FVector3 &InMax,
                                           const FRay &InRay, float &OutEntryT,
                                           float &OutExitT) const
{
    float tMin = 0.0f;
    float tMax = FLT_MAX;

    const FVector3 &Origin = InRay.Origin;
    const FVector3 &Dir = InRay.Direction;

    for (int Axis = 0; Axis < 3; ++Axis)
    {
        const float OriginValue = (&Origin.X)[Axis];
        const float DirValue = (&Dir.X)[Axis];
        const float MinValue = (&InMin.X)[Axis];
        const float MaxValue = (&InMax.X)[Axis];

        if (std::abs(DirValue) < Epsilon)
        {
            if (OriginValue < MinValue || OriginValue > MaxValue)
                return false;
        }
        else
        {
            const float InvD = 1.0f / DirValue;
            float       t0 = (MinValue - OriginValue) * InvD;
            float       t1 = (MaxValue - OriginValue) * InvD;

            if (t0 > t1)
                std::swap(t0, t1);

            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);

            if (tMax < tMin)
                return false;
        }
    }

    OutEntryT = tMin;
    OutExitT = tMax;
    return true;
}

bool FDynamicSpatialGrid::IntersectRayAABB(const FVector3 &InMin, const FVector3 &InMax,
                                           const FRay &InRay, float &OutHitT) const
{
    float EntryT = 0.0f;
    float ExitT = 0.0f;

    if (!IntersectRayAABB(InMin, InMax, InRay, EntryT, ExitT))
        return false;

    OutHitT = EntryT;
    return true;
}

bool FDynamicSpatialGrid::InitializeVoxelTraversal(const FRay           &InRay,
                                                   FVoxelTraversalState &OutState) const
{
    float EntryT = 0.0f;
    float ExitT = 0.0f;

    if (!IntersectRayAABB(Min, Max, InRay, EntryT, ExitT))
        return false;

    OutState.EntryT = EntryT;
    OutState.ExitT = ExitT;

    const FVector3 StartPoint = InRay.Origin + InRay.Direction * EntryT;
    OutState.Current = WorldToCell(StartPoint);

    if (!IsValidCell(OutState.Current))
    {
        FVectorInt Fixed = OutState.Current;
        Fixed.X = std::max(0, std::min(Fixed.X, MaxInt.X));
        Fixed.Y = std::max(0, std::min(Fixed.Y, MaxInt.Y));
        Fixed.Z = std::max(0, std::min(Fixed.Z, MaxInt.Z));
        OutState.Current = Fixed;
    }

    const FVector3 CellOrigin = CellMinToWorld(OutState.Current);

    if (InRay.Direction.X > 0.0f)
    {
        OutState.StepX = 1;
        const float NextBoundary = CellOrigin.X + IdealRatio.X;
        OutState.tMaxX = EntryT + (NextBoundary - StartPoint.X) / InRay.Direction.X;
        OutState.tDeltaX = IdealRatio.X / InRay.Direction.X;
    }
    else if (InRay.Direction.X < 0.0f)
    {
        OutState.StepX = -1;
        const float NextBoundary = CellOrigin.X;
        OutState.tMaxX = EntryT + (NextBoundary - StartPoint.X) / InRay.Direction.X;
        OutState.tDeltaX = -IdealRatio.X / InRay.Direction.X;
    }
    else
    {
        OutState.StepX = 0;
        OutState.tMaxX = FLT_MAX;
        OutState.tDeltaX = FLT_MAX;
    }

    if (InRay.Direction.Y > 0.0f)
    {
        OutState.StepY = 1;
        const float NextBoundary = CellOrigin.Y + IdealRatio.Y;
        OutState.tMaxY = EntryT + (NextBoundary - StartPoint.Y) / InRay.Direction.Y;
        OutState.tDeltaY = IdealRatio.Y / InRay.Direction.Y;
    }
    else if (InRay.Direction.Y < 0.0f)
    {
        OutState.StepY = -1;
        const float NextBoundary = CellOrigin.Y;
        OutState.tMaxY = EntryT + (NextBoundary - StartPoint.Y) / InRay.Direction.Y;
        OutState.tDeltaY = -IdealRatio.Y / InRay.Direction.Y;
    }
    else
    {
        OutState.StepY = 0;
        OutState.tMaxY = FLT_MAX;
        OutState.tDeltaY = FLT_MAX;
    }

    if (InRay.Direction.Z > 0.0f)
    {
        OutState.StepZ = 1;
        const float NextBoundary = CellOrigin.Z + IdealRatio.Z;
        OutState.tMaxZ = EntryT + (NextBoundary - StartPoint.Z) / InRay.Direction.Z;
        OutState.tDeltaZ = IdealRatio.Z / InRay.Direction.Z;
    }
    else if (InRay.Direction.Z < 0.0f)
    {
        OutState.StepZ = -1;
        const float NextBoundary = CellOrigin.Z;
        OutState.tMaxZ = EntryT + (NextBoundary - StartPoint.Z) / InRay.Direction.Z;
        OutState.tDeltaZ = -IdealRatio.Z / InRay.Direction.Z;
    }
    else
    {
        OutState.StepZ = 0;
        OutState.tMaxZ = FLT_MAX;
        OutState.tDeltaZ = FLT_MAX;
    }

    return IsValidCell(OutState.Current);
}

bool FDynamicSpatialGrid::GetNextRayPoint(FVoxelTraversalState &InOutState) const
{
    const float MinNext = std::min(InOutState.tMaxX, std::min(InOutState.tMaxY, InOutState.tMaxZ));

    if (MinNext == FLT_MAX)
        return false;

    if (std::abs(InOutState.tMaxX - MinNext) < Epsilon)
    {
        InOutState.Current.X += InOutState.StepX;
        InOutState.tMaxX += InOutState.tDeltaX;
    }

    if (std::abs(InOutState.tMaxY - MinNext) < Epsilon)
    {
        InOutState.Current.Y += InOutState.StepY;
        InOutState.tMaxY += InOutState.tDeltaY;
    }

    if (std::abs(InOutState.tMaxZ - MinNext) < Epsilon)
    {
        InOutState.Current.Z += InOutState.StepZ;
        InOutState.tMaxZ += InOutState.tDeltaZ;
    }

    return IsValidCell(InOutState.Current);
}

void FDynamicSpatialGrid::Clear()
{
    Cells.clear();
    Objects.clear();
    VisitStamps.clear();
    Min = FVector3(0, 0, 0);
    Max = FVector3(0, 0, 0);
    IdealRatio = FVector3(1, 1, 1);
    MinInt = FVectorInt(0, 0, 0);
    MaxInt = FVectorInt(0, 0, 0);
    DimX = 0;
    DimY = 0;
    DimZ = 0;
    CurrentVisitStamp = 1;
}

void FDynamicSpatialGrid::Initialize(const FVector3 &InMin, const FVector3 &InMax,
                                     const FVector3 &InIdealRatio)
{
    Cells.clear();
    Objects.clear();
    VisitStamps.clear();
    Min = InMin;
    Max = InMax;
    IdealRatio = SanitizeCellSize(InIdealRatio);

    const FVector3 Extent = Max - Min;
    DimX = std::max(1, CeilToInt(Extent.X / IdealRatio.X));
    DimY = std::max(1, CeilToInt(Extent.Y / IdealRatio.Y));
    DimZ = std::max(1, CeilToInt(Extent.Z / IdealRatio.Z));

    MinInt = FVectorInt(0, 0, 0);
    MaxInt = FVectorInt(DimX - 1, DimY - 1, DimZ - 1);

    Cells.resize(DimX * DimY * DimZ);
    CurrentVisitStamp = 1;
}

void FDynamicSpatialGrid::InitializeDynamic(const TArray<FGridObject> &InArray)
{
    if (InArray.empty())
    {
        Clear();
        return;
    }

    Min = InArray[0].Bounds.Min;
    Max = InArray[0].Bounds.Max;

    for (const FGridObject &Element : InArray)
    {
        const FAABB &AABB = Element.Bounds;

        Min.X = std::min(Min.X, AABB.Min.X);
        Min.Y = std::min(Min.Y, AABB.Min.Y);
        Min.Z = std::min(Min.Z, AABB.Min.Z);

        Max.X = std::max(Max.X, AABB.Max.X);
        Max.Y = std::max(Max.Y, AABB.Max.Y);
        Max.Z = std::max(Max.Z, AABB.Max.Z);
    }

    const FVector3 Size = Max - Min;
    FVector3       Ratio = Size;

    if (std::abs(Ratio.X) < Epsilon)
        Ratio.X = 1.0f;
    if (std::abs(Ratio.Y) < Epsilon)
        Ratio.Y = 1.0f;
    if (std::abs(Ratio.Z) < Epsilon)
        Ratio.Z = 1.0f;

    Ratio.Normalize();

    const int ElementCount = static_cast<int>(InArray.size());

    float BestScore = FLT_MAX;
    float BestScale = 1.0f;

    const float MaxGridCountInOneSideF = Size.X / Ratio.X;
    const int   MaxGridCountInOneSide =
        std::max(1, static_cast<int>(std::floor(MaxGridCountInOneSideF)));

    for (int Scale = MaxGridCountInOneSide; Scale >= 1; --Scale)
    {
        const FVector3 CandidateCellSize = Ratio * static_cast<float>(Scale);

        const float XCount = std::max(1.0f, Size.X / CandidateCellSize.X);
        const float YCount = std::max(1.0f, Size.Y / CandidateCellSize.Y);
        const float ZCount = std::max(1.0f, Size.Z / CandidateCellSize.Z);

        const float GridCount = XCount * YCount * ZCount;
        if (GridCount > static_cast<float>(ElementCount))
            break;

        FDynamicSpatialGrid Temp;
        Temp.Initialize(Min, Max, CandidateCellSize);
        Temp.Objects.reserve(ElementCount);
        Temp.VisitStamps.reserve(ElementCount);

        for (const FGridObject &Element : InArray)
        {
            Temp.Insert(Element);
        }

        const float Score = Temp.GetBuildScore(XCount, YCount, ZCount);

        if (Score < BestScore)
        {
            BestScore = Score;
            BestScale = static_cast<float>(Scale);
        }
    }

    Initialize(Min, Max, Ratio * BestScale);
    Objects.reserve(ElementCount);
    VisitStamps.reserve(ElementCount);

    for (const FGridObject &Element : InArray)
    {
        Insert(Element);
    }
}

void FDynamicSpatialGrid::Insert(const FGridObject &Item)
{
    const FAABB   &AABB = Item.Bounds;
    const FVector3 ItemMinLocal = AABB.Min - Min;
    const FVector3 ItemMaxLocal = AABB.Max - Min;

    const int RawMinX = FloorToInt(ItemMinLocal.X / IdealRatio.X);
    const int RawMinY = FloorToInt(ItemMinLocal.Y / IdealRatio.Y);
    const int RawMinZ = FloorToInt(ItemMinLocal.Z / IdealRatio.Z);

    // Bias the max corner inward to avoid over-including the next cell when the
    // AABB lands exactly on a cell boundary.
    const FVector3 AdjustedMaxLocal(std::max(ItemMinLocal.X, ItemMaxLocal.X - Epsilon),
                                    std::max(ItemMinLocal.Y, ItemMaxLocal.Y - Epsilon),
                                    std::max(ItemMinLocal.Z, ItemMaxLocal.Z - Epsilon));

    const int RawMaxX = FloorToInt(AdjustedMaxLocal.X / IdealRatio.X);
    const int RawMaxY = FloorToInt(AdjustedMaxLocal.Y / IdealRatio.Y);
    const int RawMaxZ = FloorToInt(AdjustedMaxLocal.Z / IdealRatio.Z);

    if (RawMaxX < 0 || RawMaxY < 0 || RawMaxZ < 0)
        return;

    if (RawMinX >= DimX || RawMinY >= DimY || RawMinZ >= DimZ)
        return;

    const int MinX = std::max(0, RawMinX);
    const int MinY = std::max(0, RawMinY);
    const int MinZ = std::max(0, RawMinZ);
    const int MaxX = std::min(DimX - 1, RawMaxX);
    const int MaxY = std::min(DimY - 1, RawMaxY);
    const int MaxZ = std::min(DimZ - 1, RawMaxZ);

    const int ObjectIndex = static_cast<int>(Objects.size());
    Objects.push_back(Item);
    VisitStamps.push_back(0u);

    for (int Z = MinZ; Z <= MaxZ; ++Z)
    {
        for (int Y = MinY; Y <= MaxY; ++Y)
        {
            for (int X = MinX; X <= MaxX; ++X)
            {
                Cells[GetCellIndex(X, Y, Z)].push_back(ObjectIndex);
            }
        }
    }
}

FGridObject *FDynamicSpatialGrid::CastRay(const FRay &InRay)
{
    FVoxelTraversalState State;
    if (!InitializeVoxelTraversal(InRay, State))
        return nullptr;

    FGridObject   *BestItem = nullptr;
    float          BestT = FLT_MAX;
    const uint32_t QueryVisitStamp = BeginVisitQuery();

    while (true)
    {
        TArray<int> &CandidateIndices = Cells[GetCellIndex(State.Current)];

        for (int ObjectIndex : CandidateIndices)
        {
            if (VisitStamps[ObjectIndex] == QueryVisitStamp)
                continue;

            VisitStamps[ObjectIndex] = QueryVisitStamp;

            float        HitT = 0.0f;
            FGridObject &Item = Objects[ObjectIndex];
            const FAABB &AABB = Item.Bounds;
            if (IntersectRayAABB(AABB.Min, AABB.Max, InRay, HitT))
            {
                if (HitT < BestT)
                {
                    BestT = HitT;
                    BestItem = &Item;
                }
            }
        }

        const float NextBoundaryT = std::min(State.tMaxX, std::min(State.tMaxY, State.tMaxZ));

        if (BestItem != nullptr && BestT <= NextBoundaryT)
            break;

        if (NextBoundaryT > State.ExitT)
            break;

        if (!GetNextRayPoint(State))
            break;
    }

    return BestItem;
}

void *FDynamicSpatialGrid::CastRayUserData(const FRay &InRay)
{
    FGridObject *HitObject = CastRay(InRay);
    return HitObject ? HitObject->UserData : nullptr;
}

TArray<void *> FDynamicSpatialGrid::CastRayAll(const FRay &InRay)
{
    FVoxelTraversalState State;

    TArray<void *> Results;
    if (!InitializeVoxelTraversal(InRay, State))
        return Results;

    FGridObject   *BestItem = nullptr;
    float          BestT = FLT_MAX;
    const uint32_t QueryVisitStamp = BeginVisitQuery();

    while (true)
    {
        TArray<int> &CandidateIndices = Cells[GetCellIndex(State.Current)];

        for (int ObjectIndex : CandidateIndices)
        {
            if (VisitStamps[ObjectIndex] == QueryVisitStamp)
                continue;

            VisitStamps[ObjectIndex] = QueryVisitStamp;

            float        HitT = 0.0f;
            FGridObject &Item = Objects[ObjectIndex];
            const FAABB &AABB = Item.Bounds;
            if (IntersectRayAABB(AABB.Min, AABB.Max, InRay, HitT))
            {
                Results.push_back(Item.UserData);
            }
        }

        const float NextBoundaryT = std::min(State.tMaxX, std::min(State.tMaxY, State.tMaxZ));

        if (BestItem != nullptr && BestT <= NextBoundaryT)
            break;

        if (NextBoundaryT > State.ExitT)
            break;

        if (!GetNextRayPoint(State))
            break;
    }


    return Results;
}

bool FDynamicSpatialGrid::IsCollideRay(const FVector3 &InMin, const FVector3 &InMax,
                                       const FRay &InRay) const
{
    float EntryT = 0.0f;
    float ExitT = 0.0f;
    return IntersectRayAABB(InMin, InMax, InRay, EntryT, ExitT);
}
