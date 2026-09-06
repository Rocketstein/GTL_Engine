#include "Navigation/GridNavMesh.h"

#include "Component/PrimitiveComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Debug/DrawDebugHelpers.h"
#include "Debug/DebugDrawQueue.h"
#include "GameFramework/Actor/NavMeshBoundsVolume.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Quat.h"
#include "Navigation/NavigationSystem.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <limits>
#include <queue>
#include <vector>

namespace
{
	struct FGridOpenNode
	{
		FGridNavCellKey Key;
		float F = 0.0f;
		float G = 0.0f;
		bool operator<(const FGridOpenNode& Other) const
		{
			return F > Other.F;
		}
	};

	struct FGridSearchRecord
	{
		FGridNavCellKey Parent;
		float G = FLT_MAX;
		bool bHasParent = false;
	};

	float KeyHeuristic(const FGridNavCellKey& A, const FGridNavCellKey& B)
	{
		const float DX = static_cast<float>(A.X - B.X);
		const float DY = static_cast<float>(A.Y - B.Y);
		return sqrtf(DX * DX + DY * DY);
	}

	float HorizontalDistanceSquared(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}

	float HorizontalDistanceToBox2D(const FVector& Point, const FBoundingBox& Box)
	{
		const float DX = (Point.X < Box.Min.X) ? (Box.Min.X - Point.X) : ((Point.X > Box.Max.X) ? (Point.X - Box.Max.X) : 0.0f);
		const float DY = (Point.Y < Box.Min.Y) ? (Box.Min.Y - Point.Y) : ((Point.Y > Box.Max.Y) ? (Point.Y - Box.Max.Y) : 0.0f);
		return sqrtf(DX * DX + DY * DY);
	}

	void AddPathPointIfDistinct(FNavigationPath& Path, const FVector& Location)
	{
		if (!Path.PathPoints.empty())
		{
			const FVector Last = Path.PathPoints.back().Location;
			if (FVector::DistSquared(Last, Location) < 0.01f)
			{
				return;
			}
		}
		Path.PathPoints.emplace_back(Location);
	}

	bool IsGridNavigationRelevantPrimitive(const UPrimitiveComponent* Primitive)
	{
		if (!Primitive)
		{
			return false;
		}
		if (Cast<ANavMeshBoundsVolume>(Primitive->GetOwner()))
		{
			return false;
		}
		return Primitive->CanEverAffectNavigation()
			&& Primitive->IsQueryCollisionEnabled()
			&& Primitive->GetCollisionObjectType() == ECollisionChannel::WorldStatic;
	}

	int32 ComputeDebugDrawStride(int32 TotalCells, int32 MaxCells)
	{
		if (MaxCells <= 0 || TotalCells <= MaxCells)
		{
			return 1;
		}
		return std::max(1, static_cast<int32>(ceilf(static_cast<float>(TotalCells) / static_cast<float>(MaxCells))));
	}

	float Clamp01(float Value)
	{
		return std::max(0.0f, std::min(1.0f, Value));
	}

	uint32 LerpByte(uint32 A, uint32 B, float Alpha)
	{
		const float T = Clamp01(Alpha);
		return static_cast<uint32>(static_cast<float>(A) + (static_cast<float>(B) - static_cast<float>(A)) * T);
	}

	FColor LerpColor(const FColor& A, const FColor& B, float Alpha)
	{
		return FColor(
			LerpByte(A.R, B.R, Alpha),
			LerpByte(A.G, B.G, Alpha),
			LerpByte(A.B, B.B, Alpha),
			LerpByte(A.A, B.A, Alpha));
	}
}


AGridNavMesh::AGridNavMesh()
{
	bNeedsTick = false;
	SupportedAgent.Radius = 0.42f;
	SupportedAgent.Height = 2.10f;
	SupportedAgent.StepHeight = 0.60f;
	SupportedAgent.MaxClimbHeight = 0.60f;
	SupportedAgent.MaxDropHeight = 0.60f;
	SupportedAgent.MaxSlopeDegrees = 50.0f;
}

UNavigationSystem* AGridNavMesh::GetNavigationSystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetNavigationSystem() : nullptr;
}

void AGridNavMesh::SetCellSize(float InCellSize)
{
	CellSize = std::max(0.25f, InCellSize);
	bNavigationDataBuilt = false;
}

void AGridNavMesh::SetSupportedAgentForBuild(const FNavAgentProperties& InAgent)
{
	SupportedAgent.Radius = std::max(0.01f, InAgent.Radius);
	SupportedAgent.Height = std::max(SupportedAgent.Radius * 2.0f, InAgent.Height);
	SupportedAgent.StepHeight = std::max(0.0f, InAgent.StepHeight);
	SupportedAgent.MaxClimbHeight = std::max(0.0f, InAgent.GetEffectiveMaxClimbHeight());
	SupportedAgent.MaxDropHeight = std::max(0.0f, InAgent.GetEffectiveMaxDropHeight());
	SupportedAgent.MaxSlopeDegrees = std::max(0.0f, std::min(89.0f, InAgent.MaxSlopeDegrees));
	bNavigationDataBuilt = false;
}

void AGridNavMesh::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);

	if (!PropertyName)
	{
		return;
	}

	const bool bNavigationBuildProperty =
		strcmp(PropertyName, "CellSize") == 0 || strcmp(PropertyName, "Cell Size") == 0 ||
		strcmp(PropertyName, "MaxSearchNodes") == 0 || strcmp(PropertyName, "Max Search Nodes") == 0 ||
		strcmp(PropertyName, "ProjectionUp") == 0 || strcmp(PropertyName, "Projection Up") == 0 ||
		strcmp(PropertyName, "ProjectionDown") == 0 || strcmp(PropertyName, "Projection Down") == 0 ||
		strcmp(PropertyName, "DirectPathSegmentLength") == 0 || strcmp(PropertyName, "Direct Path Segment Length") == 0 ||
		strcmp(PropertyName, "WalkableFloorMaxThickness") == 0 || strcmp(PropertyName, "Walkable Floor Max Thickness") == 0 ||
		strcmp(PropertyName, "ObstaclePadding") == 0 || strcmp(PropertyName, "Obstacle Padding") == 0 ||
		strcmp(PropertyName, "bUsePhysicsProjectionFallback") == 0 || strcmp(PropertyName, "Physics Projection Fallback") == 0 ||
		strcmp(PropertyName, "NearestCellSearchRings") == 0 || strcmp(PropertyName, "Nearest Cell Search Rings") == 0 ||
		strcmp(PropertyName, "bDrawDebugOnBuild") == 0 || strcmp(PropertyName, "Draw On Build") == 0 ||
		strcmp(PropertyName, "bDrawBlockedCells") == 0 || strcmp(PropertyName, "Draw Blocked Cells") == 0 ||
		strcmp(PropertyName, "bDrawHeightColors") == 0 || strcmp(PropertyName, "Draw Height Colors") == 0 ||
		strcmp(PropertyName, "bDrawHeightContours") == 0 || strcmp(PropertyName, "Draw Height Contours") == 0 ||
		strcmp(PropertyName, "DebugHeightContourInterval") == 0 || strcmp(PropertyName, "Height Contour Interval") == 0 ||
		strcmp(PropertyName, "DebugDrawDuration") == 0 || strcmp(PropertyName, "Debug Draw Duration") == 0 ||
		strcmp(PropertyName, "DebugDrawMaxCells") == 0 || strcmp(PropertyName, "Debug Draw Max Cells") == 0;

	if (!bNavigationBuildProperty)
	{
		return;
	}

	SetCellSize(CellSize);
	MaxSearchNodes = std::max(16, MaxSearchNodes);
	ProjectionUp = std::max(0.0f, ProjectionUp);
	ProjectionDown = std::max(0.0f, ProjectionDown);
	DirectPathSegmentLength = std::max(0.1f, DirectPathSegmentLength);
	WalkableFloorMaxThickness = std::max(0.01f, WalkableFloorMaxThickness);
	ObstaclePadding = std::max(0.0f, ObstaclePadding);
	NearestCellSearchRings = std::max(1, NearestCellSearchRings);
	DebugHeightContourInterval = std::max(0.0f, DebugHeightContourInterval);
	DebugDrawDuration = std::max(0.0f, DebugDrawDuration);
	DebugDrawMaxCells = std::max(0, DebugDrawMaxCells);

	if (UNavigationSystem* NavSys = GetNavigationSystem())
	{
		NavSys->RequestNavigationRebuild("GridNavMesh property edited", false);
	}
	else
	{
		RebuildNavigationData();
	}
}

void AGridNavMesh::ClearNavigationData()
{
	if (UWorld* World = GetWorld())
	{
		World->GetScene().GetDebugDrawQueue().ClearCategory(EDebugDrawCategory::Navigation);
	}
	WalkableCells.clear();
	BlockedCells.clear();
	IsolatedCells.clear();
	CachedNavigationPrimitives.clear();
	BlockedCellCount = 0;
	LastBuildBoundsCount = 0;
	LastBuildCandidateCount = 0;
	LastComponentCount = 0;
	LastLargestComponentSize = 0;
	LastIsolatedCellCount = 0;
	LastBuildDurationMs = 0.0f;
	LastBuildMessage.clear();
	CachedBounds = FBoundingBox();
	bNavigationDataBuilt = false;
}

bool AGridNavMesh::CollectBuildBounds(TArray<FBuildBounds>& OutBounds) const
{
	SCOPE_STAT_CAT("GridNavMesh.CollectBuildBounds", "NavMesh");
	OutBounds.clear();
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (AActor* Actor : World->GetActors())
	{
		ANavMeshBoundsVolume* Volume = Cast<ANavMeshBoundsVolume>(Actor);
		if (!Volume)
		{
			continue;
		}
		UBoxComponent* Box = Volume->GetBoundsComponent();
		if (!Box)
		{
			continue;
		}
		const FVector Center = Box->GetWorldLocation();
		const FVector Extent = Box->GetScaledBoxExtent();
		FBuildBounds Build;
		Build.Bounds = FBoundingBox(Center - Extent, Center + Extent);
		if (Build.Bounds.IsValid())
		{
			OutBounds.push_back(Build);
		}
	}

	if (!OutBounds.empty())
	{
		return true;
	}

	// Fallback: NavMeshBoundsVolume이 없으면 WorldStatic query collision primitive들의 AABB를 합쳐
	// 최소한의 NavigationData를 만들 수 있게 한다. 실제 게임 맵에서는 BoundsVolume 배치를 권장한다.
	FBoundingBox Combined;
	bool bHasStaticPrimitive = false;
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || Cast<ANavMeshBoundsVolume>(Actor))
		{
			continue;
		}
		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!IsGridNavigationRelevantPrimitive(Primitive))
			{
				continue;
			}
			const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
			if (!Bounds.IsValid())
			{
				continue;
			}
			Combined.Expand(Bounds.Min);
			Combined.Expand(Bounds.Max);
			bHasStaticPrimitive = true;
		}
	}

	if (!bHasStaticPrimitive || !Combined.IsValid())
	{
		return false;
	}

	const FVector Margin(CellSize * 2.0f, CellSize * 2.0f, ProjectionUp + ProjectionDown);
	FBuildBounds Build;
	Build.Bounds = FBoundingBox(Combined.Min - Margin, Combined.Max + Margin);
	OutBounds.push_back(Build);
	return true;
}

void AGridNavMesh::CollectNavigationPrimitives(TArray<FNavigationPrimitiveBounds>& OutPrimitives) const
{
	SCOPE_STAT_CAT("GridNavMesh.CollectPrimitives", "NavMesh");
	OutPrimitives.clear();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float MaxFloorThickness = std::max(0.01f, WalkableFloorMaxThickness);
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || Cast<ANavMeshBoundsVolume>(Actor))
		{
			continue;
		}
		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!IsGridNavigationRelevantPrimitive(Primitive))
			{
				continue;
			}
			const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
			if (!Bounds.IsValid())
			{
				continue;
			}
			const FVector Extent = Bounds.GetExtent();
			FNavigationPrimitiveBounds Entry;
			Entry.Bounds = Bounds;
			Entry.bFloorCandidate = Extent.Z <= MaxFloorThickness;
			Entry.bObstacle = !Entry.bFloorCandidate;
			OutPrimitives.push_back(Entry);
		}
	}
}

void AGridNavMesh::BuildNavigationPrimitiveSpatialIndex(
	const TArray<FNavigationPrimitiveBounds>& Primitives,
	const TArray<FBuildBounds>& BuildBounds,
	FNavigationPrimitiveSpatialIndex& OutIndex) const
{
	SCOPE_STAT_CAT("GridNavMesh.BuildPrimitiveSpatialIndex", "NavMesh");
	OutIndex.FloorCandidatesByCell.clear();
	OutIndex.ObstacleCandidatesByCell.clear();
	OutIndex.EmptyCandidates.clear();
	OutIndex.FloorReferenceCount = 0;
	OutIndex.ObstacleReferenceCount = 0;

	const float Grid = std::max(0.25f, CellSize);
	const float HalfCell = std::max(0.05f, CellSize * 0.5f);
	const float RequiredRadius = std::max(0.01f, GetEffectiveAgentRadius(SupportedAgent) + std::max(0.0f, ObstaclePadding));

	auto AddPrimitiveToCells = [Grid, &BuildBounds](
		std::unordered_map<FGridNavCellKey, TArray<int32>, FGridNavCellKeyHash>& Map,
		const FBoundingBox& Bounds,
		float ExpandXY,
		int32 PrimitiveIndex) -> int32
	{
		int32 ReferenceCount = 0;
		for (const FBuildBounds& Build : BuildBounds)
		{
			if (!Build.Bounds.IsValid())
			{
				continue;
			}
			if (Bounds.Max.X + ExpandXY < Build.Bounds.Min.X || Bounds.Min.X - ExpandXY > Build.Bounds.Max.X ||
				Bounds.Max.Y + ExpandXY < Build.Bounds.Min.Y || Bounds.Min.Y - ExpandXY > Build.Bounds.Max.Y)
			{
				continue;
			}

			const int32 PrimitiveMinX = static_cast<int32>(floorf((Bounds.Min.X - ExpandXY) / Grid)) - 1;
			const int32 PrimitiveMaxX = static_cast<int32>(floorf((Bounds.Max.X + ExpandXY) / Grid)) + 1;
			const int32 PrimitiveMinY = static_cast<int32>(floorf((Bounds.Min.Y - ExpandXY) / Grid)) - 1;
			const int32 PrimitiveMaxY = static_cast<int32>(floorf((Bounds.Max.Y + ExpandXY) / Grid)) + 1;
			const int32 BuildMinX = static_cast<int32>(floorf(Build.Bounds.Min.X / Grid)) - 1;
			const int32 BuildMaxX = static_cast<int32>(ceilf(Build.Bounds.Max.X / Grid)) + 1;
			const int32 BuildMinY = static_cast<int32>(floorf(Build.Bounds.Min.Y / Grid)) - 1;
			const int32 BuildMaxY = static_cast<int32>(ceilf(Build.Bounds.Max.Y / Grid)) + 1;
			const int32 MinX = std::max(PrimitiveMinX, BuildMinX);
			const int32 MaxX = std::min(PrimitiveMaxX, BuildMaxX);
			const int32 MinY = std::max(PrimitiveMinY, BuildMinY);
			const int32 MaxY = std::min(PrimitiveMaxY, BuildMaxY);

			for (int32 X = MinX; X <= MaxX; ++X)
			{
				for (int32 Y = MinY; Y <= MaxY; ++Y)
				{
					Map[FGridNavCellKey{X, Y}].push_back(PrimitiveIndex);
					++ReferenceCount;
				}
			}
		}
		return ReferenceCount;
	};

	for (int32 Index = 0; Index < static_cast<int32>(Primitives.size()); ++Index)
	{
		const FNavigationPrimitiveBounds& Primitive = Primitives[Index];
		if (!Primitive.Bounds.IsValid())
		{
			continue;
		}
		if (Primitive.bFloorCandidate)
		{
			OutIndex.FloorReferenceCount += AddPrimitiveToCells(
				OutIndex.FloorCandidatesByCell,
				Primitive.Bounds,
				HalfCell,
				Index);
		}
		if (Primitive.bObstacle)
		{
			OutIndex.ObstacleReferenceCount += AddPrimitiveToCells(
				OutIndex.ObstacleCandidatesByCell,
				Primitive.Bounds,
				RequiredRadius,
				Index);
		}
	}

	auto CompactCellCandidates = [](
		std::unordered_map<FGridNavCellKey, TArray<int32>, FGridNavCellKeyHash>& Map) -> int32
	{
		int32 ReferenceCount = 0;
		for (auto& Entry : Map)
		{
			TArray<int32>& Candidates = Entry.second;
			std::sort(Candidates.begin(), Candidates.end());
			Candidates.erase(std::unique(Candidates.begin(), Candidates.end()), Candidates.end());
			ReferenceCount += static_cast<int32>(Candidates.size());
		}
		return ReferenceCount;
	};
	OutIndex.FloorReferenceCount = CompactCellCandidates(OutIndex.FloorCandidatesByCell);
	OutIndex.ObstacleReferenceCount = CompactCellCandidates(OutIndex.ObstacleCandidatesByCell);
}

FGridNavCellKey AGridNavMesh::WorldToCell(const FVector& Point) const
{
	const float Grid = std::max(0.25f, CellSize);
	return FGridNavCellKey{
		static_cast<int32>(floorf(Point.X / Grid)),
		static_cast<int32>(floorf(Point.Y / Grid))
	};
}

FVector AGridNavMesh::CellToWorldGuess(const FGridNavCellKey& Key, float Z) const
{
	const float Grid = std::max(0.25f, CellSize);
	return FVector((static_cast<float>(Key.X) + 0.5f) * Grid, (static_cast<float>(Key.Y) + 0.5f) * Grid, Z);
}

bool AGridNavMesh::IsInsideAnyBuildBounds(const FVector& Point, const TArray<FBuildBounds>& Bounds) const
{
	if (Bounds.empty())
	{
		return true;
	}
	for (const FBuildBounds& B : Bounds)
	{
		if (B.Bounds.IsContains(Point))
		{
			return true;
		}
	}
	return false;
}

bool AGridNavMesh::ProjectPointToWalkableSurfaceByPrimitiveBounds(
	const FVector& Guess,
	const TArray<FBuildBounds>& Bounds,
	const TArray<FNavigationPrimitiveBounds>& Primitives,
	const TArray<int32>& CandidateIndices,
	FVector& OutProjectedLocation) const
{
	if (!IsInsideAnyBuildBounds(Guess, Bounds))
	{
		return false;
	}

	const float HalfCell = std::max(0.05f, CellSize * 0.5f);
	const float StartZ = Guess.Z + ProjectionUp;
	const float EndZ = Guess.Z - ProjectionDown;
	bool bFound = false;
	float BestZ = -FLT_MAX;

	for (int32 PrimitiveIndex : CandidateIndices)
	{
		if (PrimitiveIndex < 0 || PrimitiveIndex >= static_cast<int32>(Primitives.size()))
		{
			continue;
		}
		const FNavigationPrimitiveBounds& PrimitiveBounds = Primitives[PrimitiveIndex];
		if (!PrimitiveBounds.bFloorCandidate)
		{
			continue;
		}
		const FBoundingBox& B = PrimitiveBounds.Bounds;
		if (!B.IsValid())
		{
			continue;
		}
		if (Guess.X < B.Min.X - HalfCell || Guess.X > B.Max.X + HalfCell ||
			Guess.Y < B.Min.Y - HalfCell || Guess.Y > B.Max.Y + HalfCell)
		{
			continue;
		}
		const float TopZ = B.Max.Z;
		if (TopZ < EndZ || TopZ > StartZ)
		{
			continue;
		}
		if (TopZ > BestZ)
		{
			BestZ = TopZ;
			bFound = true;
		}
	}

	if (!bFound)
	{
		return false;
	}
	OutProjectedLocation = FVector(Guess.X, Guess.Y, BestZ);
	return IsInsideAnyBuildBounds(OutProjectedLocation, Bounds);
}

float AGridNavMesh::GetEffectiveAgentRadius(const FNavAgentProperties& AgentProps) const
{
	// 질의 반경은 빌드 에이전트를 넘지 못한다(헤더 주석 참조). 빌드 시엔 AgentProps==SupportedAgent 라 무효과.
	return std::min(AgentProps.Radius, SupportedAgent.Radius);
}

float AGridNavMesh::GetEffectiveAgentHeight(const FNavAgentProperties& AgentProps) const
{
	return std::min(AgentProps.Height, SupportedAgent.Height);
}

bool AGridNavMesh::HasAgentFootprintByPrimitiveBounds(
	const FVector& ProjectedLocation,
	const FNavAgentProperties& AgentProps,
	const TArray<FNavigationPrimitiveBounds>& Primitives,
	const TArray<int32>& CandidateIndices,
	float* OutClearanceRadius) const
{
	const float RequiredRadius = std::max(0.01f, GetEffectiveAgentRadius(AgentProps) + std::max(0.0f, ObstaclePadding));
	const float AgentBottom = ProjectedLocation.Z + 0.05f;
	const float AgentHeight = std::max(RequiredRadius * 2.0f, GetEffectiveAgentHeight(AgentProps));
	const float AgentTop = ProjectedLocation.Z + AgentHeight;
	float ClearanceRadius = FLT_MAX;

	for (int32 PrimitiveIndex : CandidateIndices)
	{
		if (PrimitiveIndex < 0 || PrimitiveIndex >= static_cast<int32>(Primitives.size()))
		{
			continue;
		}
		const FNavigationPrimitiveBounds& PrimitiveBounds = Primitives[PrimitiveIndex];
		if (!PrimitiveBounds.bObstacle)
		{
			continue;
		}
		const FBoundingBox& B = PrimitiveBounds.Bounds;
		if (!B.IsValid())
		{
			continue;
		}
		// Obstacles entirely below the current floor do not block this floor.  This allows
		// valid upper platforms, while still treating vertical walls that overlap the
		// agent's capsule height as blocking geometry.
		if (B.Max.Z <= ProjectedLocation.Z + 0.02f)
		{
			continue;
		}
		if (B.Min.Z >= AgentTop || B.Max.Z <= AgentBottom)
		{
			continue;
		}

		const float DistanceToObstacle = HorizontalDistanceToBox2D(ProjectedLocation, B);
		ClearanceRadius = std::min(ClearanceRadius, DistanceToObstacle);
		if (DistanceToObstacle < RequiredRadius)
		{
			if (OutClearanceRadius)
			{
				*OutClearanceRadius = DistanceToObstacle;
			}
			return false;
		}
	}

	if (OutClearanceRadius)
	{
		*OutClearanceRadius = ClearanceRadius == FLT_MAX ? 1000000.0f : ClearanceRadius;
	}
	return true;
}

bool AGridNavMesh::HasAgentFootprintByPrimitiveBounds(
	const FVector& ProjectedLocation,
	const FNavAgentProperties& AgentProps,
	const TArray<FNavigationPrimitiveBounds>& Primitives,
	float* OutClearanceRadius) const
{
	const float RequiredRadius = std::max(0.01f, GetEffectiveAgentRadius(AgentProps) + std::max(0.0f, ObstaclePadding));
	const float AgentBottom = ProjectedLocation.Z + 0.05f;
	const float AgentHeight = std::max(RequiredRadius * 2.0f, GetEffectiveAgentHeight(AgentProps));
	const float AgentTop = ProjectedLocation.Z + AgentHeight;
	float ClearanceRadius = FLT_MAX;

	for (const FNavigationPrimitiveBounds& PrimitiveBounds : Primitives)
	{
		if (!PrimitiveBounds.bObstacle)
		{
			continue;
		}
		const FBoundingBox& B = PrimitiveBounds.Bounds;
		if (!B.IsValid())
		{
			continue;
		}
		// Obstacles entirely below the current floor do not block this floor.  This allows
		// valid upper platforms, while still treating vertical walls that overlap the
		// agent's capsule height as blocking geometry.
		if (B.Max.Z <= ProjectedLocation.Z + 0.02f)
		{
			continue;
		}
		if (B.Min.Z >= AgentTop || B.Max.Z <= AgentBottom)
		{
			continue;
		}

		const float DistanceToObstacle = HorizontalDistanceToBox2D(ProjectedLocation, B);
		ClearanceRadius = std::min(ClearanceRadius, DistanceToObstacle);
		if (DistanceToObstacle < RequiredRadius)
		{
			if (OutClearanceRadius)
			{
				*OutClearanceRadius = DistanceToObstacle;
			}
			return false;
		}
	}

	if (OutClearanceRadius)
	{
		*OutClearanceRadius = ClearanceRadius == FLT_MAX ? 1000000.0f : ClearanceRadius;
	}
	return true;
}

bool AGridNavMesh::HasAgentFootprint(
	const FVector& ProjectedLocation,
	const FNavAgentProperties& AgentProps,
	const TArray<FNavigationPrimitiveBounds>& Primitives,
	float* OutClearanceRadius) const
{
	return HasAgentFootprintByPrimitiveBounds(ProjectedLocation, AgentProps, Primitives, OutClearanceRadius);
}

AGridNavMesh::FBuildCellResult AGridNavMesh::BuildCell(
	const FGridNavCellKey& Key,
	float GuessZ,
	const TArray<FBuildBounds>& Bounds,
	const TArray<FNavigationPrimitiveBounds>& Primitives,
	const FNavigationPrimitiveSpatialIndex& SpatialIndex,
	FGridNavCell& OutCell) const
{
	FBuildCellResult Result;
	const FVector Guess = CellToWorldGuess(Key, GuessZ);
	if (!IsInsideAnyBuildBounds(Guess, Bounds))
	{
		return Result;
	}
	FVector Projected;
	const auto FloorIt = SpatialIndex.FloorCandidatesByCell.find(Key);
	const TArray<int32>& FloorCandidates = FloorIt != SpatialIndex.FloorCandidatesByCell.end()
		? FloorIt->second
		: SpatialIndex.EmptyCandidates;
	bool bProjected = ProjectPointToWalkableSurfaceByPrimitiveBounds(Guess, Bounds, Primitives, FloorCandidates, Projected);
	if (!bProjected && bUsePhysicsProjectionFallback)
	{
		if (UNavigationSystem* NavSys = GetNavigationSystem())
		{
			bProjected = NavSys->ProjectPointToNavigationRuntime(Guess, Projected, SupportedAgent, ProjectionUp, ProjectionDown);
		}
	}
	if (!bProjected || !IsInsideAnyBuildBounds(Projected, Bounds))
	{
		return Result;
	}
	Result.DebugLocation = Projected;
	float ClearanceRadius = 0.0f;
	const auto ObstacleIt = SpatialIndex.ObstacleCandidatesByCell.find(Key);
	const TArray<int32>& ObstacleCandidates = ObstacleIt != SpatialIndex.ObstacleCandidatesByCell.end()
		? ObstacleIt->second
		: SpatialIndex.EmptyCandidates;
	if (!HasAgentFootprintByPrimitiveBounds(Projected, SupportedAgent, Primitives, ObstacleCandidates, &ClearanceRadius))
	{
		Result.DebugLocation = Projected;
		Result.ClearanceRadius = ClearanceRadius;
		Result.bBlockedByObstacle = true;
		return Result;
	}
	OutCell.Location = Projected;
	OutCell.bWalkable = true;
	OutCell.ClearanceRadius = ClearanceRadius;
	Result.ClearanceRadius = ClearanceRadius;
	Result.bWalkable = true;
	return Result;
}

void AGridNavMesh::ComputeConnectivity()
{
	SCOPE_STAT_CAT("GridNavMesh.ComputeConnectivity", "NavMesh");
	IsolatedCells.clear();
	LastComponentCount = 0;
	LastLargestComponentSize = 0;
	LastIsolatedCellCount = 0;
	if (WalkableCells.empty())
	{
		return;
	}

	// 모든 walkable 셀은 빌드 시 SupportedAgent 풋프린트를 이미 통과했다 → SupportedAgent 기준 "사용 가능"은
	// WalkableCells 멤버십과 동치다. 그래서 flood-fill 은 비싼 풋프린트 재검사 없이 맵 조회 + 경사 전이만
	// 본다(질의 에이전트는 빌드 에이전트로 클램프되므로 이 연결성은 실제 경로 연결성을 대표한다).
	const FGridNavCellKey Directions[8] = {
		{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
		{ 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
	};

	std::unordered_map<FGridNavCellKey, int32, FGridNavCellKeyHash> Component;
	Component.reserve(WalkableCells.size());
	int32 NextComponent = 0;
	int32 LargestComponent = -1;
	size_t LargestSize = 0;

	std::queue<FGridNavCellKey> Frontier;
	for (const auto& Seed : WalkableCells)
	{
		if (Component.find(Seed.first) != Component.end())
		{
			continue;
		}
		const int32 ComponentId = NextComponent++;
		size_t ComponentSize = 0;
		Component[Seed.first] = ComponentId;
		Frontier.push(Seed.first);

		while (!Frontier.empty())
		{
			const FGridNavCellKey Key = Frontier.front();
			Frontier.pop();
			++ComponentSize;
			const FGridNavCell* Cell = FindCell(Key);
			if (!Cell)
			{
				continue;
			}
			for (const FGridNavCellKey& Dir : Directions)
			{
				const FGridNavCellKey Next{ Key.X + Dir.X, Key.Y + Dir.Y };
				if (Component.find(Next) != Component.end())
				{
					continue;
				}
				const FGridNavCell* NextCell = FindCell(Next);
				if (!NextCell)
				{
					continue;
				}
				// 대각 코너 컷 방지: 두 직교 이웃이 모두 walkable 이어야 대각 이동 허용(A* 와 동일).
				if (Dir.X != 0 && Dir.Y != 0)
				{
					if (!FindCell(FGridNavCellKey{ Key.X + Dir.X, Key.Y }) ||
						!FindCell(FGridNavCellKey{ Key.X, Key.Y + Dir.Y }))
					{
						continue;
					}
				}
				if (!IsHeightTransitionAllowed(Cell->Location, NextCell->Location, SupportedAgent))
				{
					continue;
				}
				Component[Next] = ComponentId;
				Frontier.push(Next);
			}
		}

		if (ComponentSize > LargestSize)
		{
			LargestSize = ComponentSize;
			LargestComponent = ComponentId;
		}
	}

	for (const auto& Pair : WalkableCells)
	{
		auto It = Component.find(Pair.first);
		if (It == Component.end() || It->second != LargestComponent)
		{
			IsolatedCells.insert(Pair.first);
		}
	}

	LastComponentCount = NextComponent;
	LastLargestComponentSize = static_cast<int32>(LargestSize);
	LastIsolatedCellCount = static_cast<int32>(IsolatedCells.size());
}

bool AGridNavMesh::RebuildNavigationData()
{
	SCOPE_STAT_CAT("GridNavMesh.RebuildNavigationData", "NavMesh");
	const auto BuildStart = std::chrono::steady_clock::now();
	ClearNavigationData();

	TArray<FBuildBounds> Bounds;
	if (!CollectBuildBounds(Bounds))
	{
		LastBuildMessage = "GridNavMesh build failed: no NavMeshBoundsVolume or WorldStatic primitives";
		return false;
	}
	LastBuildBoundsCount = static_cast<int32>(Bounds.size());

	for (const FBuildBounds& B : Bounds)
	{
		CachedBounds.Expand(B.Bounds.Min);
		CachedBounds.Expand(B.Bounds.Max);
	}

	TArray<FNavigationPrimitiveBounds> NavigationPrimitives;
	CollectNavigationPrimitives(NavigationPrimitives);
	CachedNavigationPrimitives = NavigationPrimitives;

	FNavigationPrimitiveSpatialIndex PrimitiveSpatialIndex;
	BuildNavigationPrimitiveSpatialIndex(NavigationPrimitives, Bounds, PrimitiveSpatialIndex);

	const float Grid = std::max(0.25f, CellSize);
	for (const FBuildBounds& B : Bounds)
	{
		const int32 MinX = static_cast<int32>(floorf(B.Bounds.Min.X / Grid)) - 1;
		const int32 MaxX = static_cast<int32>(ceilf(B.Bounds.Max.X / Grid)) + 1;
		const int32 MinY = static_cast<int32>(floorf(B.Bounds.Min.Y / Grid)) - 1;
		const int32 MaxY = static_cast<int32>(ceilf(B.Bounds.Max.Y / Grid)) + 1;
		const float GuessZ = B.Bounds.GetCenter().Z;

		for (int32 X = MinX; X <= MaxX; ++X)
		{
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				++LastBuildCandidateCount;
				const FGridNavCellKey Key{X, Y};
				if (WalkableCells.find(Key) != WalkableCells.end())
				{
					continue;
				}
				FGridNavCell Cell;
				const FBuildCellResult Result = BuildCell(Key, GuessZ, Bounds, NavigationPrimitives, PrimitiveSpatialIndex, Cell);
				if (Result.bWalkable)
				{
					WalkableCells[Key] = Cell;
				}
				else if (Result.bBlockedByObstacle)
				{
					BlockedCells[Key] = Result.DebugLocation;
					++BlockedCellCount;
				}
			}
		}
	}

	bNavigationDataBuilt = !WalkableCells.empty();
	ComputeConnectivity();
	const auto BuildEnd = std::chrono::steady_clock::now();
	LastBuildDurationMs = static_cast<float>(std::chrono::duration<double, std::milli>(BuildEnd - BuildStart).count());
	LastBuildMessage = FString("GridNavMesh build ")
		+ (bNavigationDataBuilt ? "succeeded" : "failed")
		+ "; Bounds=" + std::to_string(LastBuildBoundsCount)
		+ "; Candidates=" + std::to_string(LastBuildCandidateCount)
		+ "; NavPrimitives=" + std::to_string(static_cast<int32>(NavigationPrimitives.size()))
		+ "; IndexedFloorCells=" + std::to_string(static_cast<int32>(PrimitiveSpatialIndex.FloorCandidatesByCell.size()))
		+ "; IndexedObstacleCells=" + std::to_string(static_cast<int32>(PrimitiveSpatialIndex.ObstacleCandidatesByCell.size()))
		+ "; IndexedFloorRefs=" + std::to_string(PrimitiveSpatialIndex.FloorReferenceCount)
		+ "; IndexedObstacleRefs=" + std::to_string(PrimitiveSpatialIndex.ObstacleReferenceCount)
		+ "; Walkable=" + std::to_string(GetNavigationNodeCount())
		+ "; Blocked=" + std::to_string(BlockedCellCount)
		// 연결성 진단: Components 가 1 보다 크거나 Isolated 가 0 보다 크면, 그 Isolated 셀 위에 선 에이전트는
		// 메인 navmesh(LargestComp)로 가는 경로를 못 찾는다 = "특정 위치에서 길 못 찾음"의 정체.
		+ "; Components=" + std::to_string(LastComponentCount)
		+ "; LargestComp=" + std::to_string(LastLargestComponentSize)
		+ "; Isolated=" + std::to_string(LastIsolatedCellCount)
		+ "; TimeMs=" + std::to_string(LastBuildDurationMs);
	if (bNavigationDataBuilt && bDrawDebugOnBuild)
	{
		DebugDrawNavigationData(DebugDrawDuration);
	}
	return bNavigationDataBuilt;
}

const FGridNavCell* AGridNavMesh::FindCell(const FGridNavCellKey& Key) const
{
	auto It = WalkableCells.find(Key);
	return It != WalkableCells.end() ? &It->second : nullptr;
}

bool AGridNavMesh::IsCellUsableForAgent(const FGridNavCellKey& Key, const FNavAgentProperties& AgentProps, const FGridNavCell*& OutCell) const
{
	OutCell = FindCell(Key);
	if (!OutCell || !OutCell->bWalkable)
	{
		return false;
	}

	const float RequiredRadius = std::max(0.01f, GetEffectiveAgentRadius(AgentProps) + std::max(0.0f, ObstaclePadding));
	if (OutCell->ClearanceRadius > 0.0f && OutCell->ClearanceRadius + 0.001f < RequiredRadius)
	{
		return false;
	}

	// Re-check against the cached source obstacle bounds using the requesting agent's
	// radius and height.  The grid is built once, but path queries must remain agent-aware;
	// otherwise larger bodies can receive routes that only fit the smallest build agent.
	if (!CachedNavigationPrimitives.empty())
	{
		return HasAgentFootprint(OutCell->Location, AgentProps, CachedNavigationPrimitives, nullptr);
	}
	return true;
}

bool AGridNavMesh::FindCellAtPointStrict(const FVector& Point, const FNavAgentProperties& AgentProps, FGridNavCellKey& OutKey, FVector& OutLocation) const
{
	if (!bNavigationDataBuilt || WalkableCells.empty())
	{
		return false;
	}
	OutKey = WorldToCell(Point);
	const FGridNavCell* Cell = nullptr;
	if (!IsCellUsableForAgent(OutKey, AgentProps, Cell))
	{
		return false;
	}
	OutLocation = Cell->Location;
	return true;
}

bool AGridNavMesh::FindNearestCell(const FVector& Point, FGridNavCellKey& OutKey, FVector& OutLocation) const
{
	if (!bNavigationDataBuilt || WalkableCells.empty())
	{
		return false;
	}
	const FGridNavCellKey Base = WorldToCell(Point);
	const float Grid = std::max(0.25f, CellSize);
	float BestDistSq = FLT_MAX;
	bool bFound = false;

	for (int32 Ring = 0; Ring <= std::max(1, NearestCellSearchRings); ++Ring)
	{
		// 첫 ring 에서 멈추지 않고 진짜 최근접(3D)을 찾는다(FindNearestCellForAgent 와 동일한 이유).
		if (bFound)
		{
			const float RingMinDist = static_cast<float>(Ring - 1) * Grid;
			if (RingMinDist > 0.0f && RingMinDist * RingMinDist >= BestDistSq)
			{
				break;
			}
		}
		for (int32 DX = -Ring; DX <= Ring; ++DX)
		{
			for (int32 DY = -Ring; DY <= Ring; ++DY)
			{
				if (Ring != 0 && std::abs(DX) != Ring && std::abs(DY) != Ring)
				{
					continue;
				}
				const FGridNavCellKey Key{Base.X + DX, Base.Y + DY};
				const FGridNavCell* Cell = FindCell(Key);
				if (!Cell)
				{
					continue;
				}
				// 수평 거리로 고른다(캡슐 중심 Z 편향 방지 — FindNearestCellForAgent 와 동일한 이유).
				const float DistSq = HorizontalDistanceSquared(Point, Cell->Location);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					OutKey = Key;
					OutLocation = Cell->Location;
					bFound = true;
				}
			}
		}
	}
	return bFound;
}

bool AGridNavMesh::FindNearestCellForAgent(const FVector& Point, const FNavAgentProperties& AgentProps, FGridNavCellKey& OutKey, FVector& OutLocation) const
{
	if (!bNavigationDataBuilt || WalkableCells.empty())
	{
		return false;
	}
	const FGridNavCellKey Base = WorldToCell(Point);
	const float Grid = std::max(0.25f, CellSize);
	float BestDistSq = FLT_MAX;
	bool bFound = false;

	for (int32 Ring = 0; Ring <= std::max(1, NearestCellSearchRings); ++Ring)
	{
		// 거리는 3D 로 재는데 탐색은 XY ring 순서다 → 첫 ring 에서 멈추면, XY 는 가깝지만 Z 가 크게
		// 다른 림/플랫폼 셀을 골라버린다(구덩이·단차 근처). 진짜 최근접을 보장하려면, 이 ring 이상에선
		// best 보다 가까운 셀이 존재할 수 없을 때까지 확장한다. ring R 셀의 수평거리 하한 = (R-1)·Grid
		// 이고 3D 거리 ≥ 수평거리 이므로, (R-1)·Grid 가 현재 best 거리 이상이면 더 볼 필요 없다.
		if (bFound)
		{
			const float RingMinDist = static_cast<float>(Ring - 1) * Grid;
			if (RingMinDist > 0.0f && RingMinDist * RingMinDist >= BestDistSq)
			{
				break;
			}
		}
		for (int32 DX = -Ring; DX <= Ring; ++DX)
		{
			for (int32 DY = -Ring; DY <= Ring; ++DY)
			{
				if (Ring != 0 && std::abs(DX) != Ring && std::abs(DY) != Ring)
				{
					continue;
				}
				const FGridNavCellKey Key{Base.X + DX, Base.Y + DY};
				const FGridNavCell* Cell = nullptr;
				if (!IsCellUsableForAgent(Key, AgentProps, Cell))
				{
					continue;
				}
				// 수평 거리로 고른다. 쿼리 점은 보통 캡슐 "중심"(지면보다 한참 위)이라 3D 거리를 쓰면
				// 옆의 더 높은 셀이 더 가깝다고 잘못 판정된다. 컬럼당 셀은 하나뿐이라 수평 거리가 유일·정답.
				const float DistSq = HorizontalDistanceSquared(Point, Cell->Location);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					OutKey = Key;
					OutLocation = Cell->Location;
					bFound = true;
				}
			}
		}
	}
	return bFound;
}

bool AGridNavMesh::ProjectPointToNavigation(const FVector& Point, FVector& OutProjectedPoint, const FNavAgentProperties& AgentProps) const
{
	SCOPE_STAT_CAT("GridNavMesh.ProjectPoint", "NavMesh");
	FGridNavCellKey Key;
	return FindNearestCellForAgent(Point, AgentProps, Key, OutProjectedPoint);
}

bool AGridNavMesh::IsHeightTransitionAllowed(const FVector& From, const FVector& To, const FNavAgentProperties& AgentProps) const
{
	const float DeltaZ = To.Z - From.Z;
	const float Tolerance = 0.02f;

	// 인접 그리드 셀은 항상 최소 한 CellSize 만큼 수평으로 떨어져 있으므로, 두 표면점 사이의 높이차는
	// 수직 "단차"가 아니라 사실상 "경사로(ramp)"다. MaxSlopeDegrees 로 걸을 수 있다고 판정된 연속 비탈은
	// 그 셀 간 상승량이 이산 단차 한계(MaxClimbHeight)를 넘더라도 통과시켜야 한다. 그렇지 않으면 걸을 수
	// 있는 비탈이 등고선 띠 단위로 끊긴 섬이 되어 FindPath 가 그 위를 가로지르지 못한다(예: 40~50° 비탈).
	const float HorizontalDist = sqrtf(HorizontalDistanceSquared(From, To));
	const float SlopeDeg = std::max(0.0f, std::min(89.0f, AgentProps.MaxSlopeDegrees));
	const float SlopeAllowance = HorizontalDist * tanf(SlopeDeg * FMath::DegToRad);

	if (DeltaZ > 0.0f)
	{
		const float MaxUp = std::max(AgentProps.GetEffectiveMaxClimbHeight(), SlopeAllowance) + Tolerance;
		return DeltaZ <= MaxUp;
	}
	const float MaxDown = std::max(AgentProps.GetEffectiveMaxDropHeight(), SlopeAllowance) + Tolerance;
	return -DeltaZ <= MaxDown;
}

bool AGridNavMesh::HasCachedSegment(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const
{
	if (!bNavigationDataBuilt || WalkableCells.empty())
	{
		return false;
	}
	const float RequiredRadius = std::max(0.01f, GetEffectiveAgentRadius(AgentProps) + std::max(0.0f, ObstaclePadding));
	const float Dist = sqrtf(HorizontalDistanceSquared(Start, End));
	const float Grid = std::max(0.25f, CellSize);
	const float SampleStep = std::max(0.1f, std::min(Grid * 0.5f, RequiredRadius));
	const int32 Segments = std::max(1, static_cast<int32>(ceilf(Dist / SampleStep)));
	FVector Previous = Start;
	FGridNavCellKey PreviousKey;
	bool bHasPreviousKey = false;
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(Segments);
		const FVector Sample(Start.X + (End.X - Start.X) * Alpha, Start.Y + (End.Y - Start.Y) * Alpha, Start.Z + (End.Z - Start.Z) * Alpha);
		FGridNavCellKey Key;
		FVector CellLocation;
		if (!FindCellAtPointStrict(Sample, AgentProps, Key, CellLocation))
		{
			return false;
		}
		if (const FGridNavCell* Cell = FindCell(Key))
		{
			if (Cell->ClearanceRadius > 0.0f)
			{
				const float OffsetH = sqrtf(HorizontalDistanceSquared(Sample, Cell->Location));
				if (Cell->ClearanceRadius - OffsetH + 0.001f < RequiredRadius)
				{
					return false;
				}
			}
		}
		if (bHasPreviousKey)
		{
			const FGridNavCellKey Step{ Key.X - PreviousKey.X, Key.Y - PreviousKey.Y };
			if (std::abs(Step.X) > 1 || std::abs(Step.Y) > 1)
			{
				return false;
			}
			if (!IsDiagonalMoveAllowed(PreviousKey, Step, AgentProps))
			{
				return false;
			}
		}
		if (!IsHeightTransitionAllowed(Previous, CellLocation, AgentProps))
		{
			return false;
		}
		Previous = CellLocation;
		PreviousKey = Key;
		bHasPreviousKey = true;
	}
	return true;
}

bool AGridNavMesh::BuildDirectCachedPath(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const
{
	SCOPE_STAT_CAT("GridNavMesh.BuildDirectCachedPath", "NavMesh");
	const float Dist = sqrtf(HorizontalDistanceSquared(Start, End));
	const int32 Segments = std::max(1, static_cast<int32>(ceilf(Dist / std::max(0.1f, DirectPathSegmentLength))));
	FVector Previous = Start;
	for (int32 Index = 1; Index <= Segments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(Segments);
		const FVector Sample(Start.X + (End.X - Start.X) * Alpha, Start.Y + (End.Y - Start.Y) * Alpha, Start.Z + (End.Z - Start.Z) * Alpha);
		FGridNavCellKey SampleKey;
		FVector Projected;
		if (!FindCellAtPointStrict(Sample, AgentProps, SampleKey, Projected))
		{
			return false;
		}
		if (!HasCachedSegment(Previous, Projected, AgentProps))
		{
			return false;
		}
		Previous = Projected;
	}
	OutPath.Reset();
	AddPathPointIfDistinct(OutPath, Start);
	AddPathPointIfDistinct(OutPath, End);
	OutPath.bIsPartial = false;
	OutPath.bIsValid = true;
	return true;
}

bool AGridNavMesh::IsDiagonalMoveAllowed(const FGridNavCellKey& From, const FGridNavCellKey& Dir, const FNavAgentProperties& AgentProps) const
{
	if (Dir.X == 0 || Dir.Y == 0)
	{
		return true;
	}
	const FGridNavCellKey A{From.X + Dir.X, From.Y};
	const FGridNavCellKey B{From.X, From.Y + Dir.Y};
	const FGridNavCell* CellA = nullptr;
	const FGridNavCell* CellB = nullptr;
	return IsCellUsableForAgent(A, AgentProps, CellA) && IsCellUsableForAgent(B, AgentProps, CellB);
}

bool AGridNavMesh::FindPath(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const
{
	SCOPE_STAT_CAT("GridNavMesh.FindPath", "NavMesh");
	OutPath.Reset();
	FGridNavCellKey StartKey;
	FGridNavCellKey GoalKey;
	FVector StartLocation;
	FVector GoalLocation;
	if (!FindNearestCellForAgent(Start, AgentProps, StartKey, StartLocation) || !FindNearestCellForAgent(End, AgentProps, GoalKey, GoalLocation))
	{
		return false;
	}
	if (BuildDirectCachedPath(StartLocation, GoalLocation, AgentProps, OutPath))
	{
		AddPathPointIfDistinct(OutPath, End);
		return true;
	}

	std::priority_queue<FGridOpenNode> Open;
	std::unordered_map<FGridNavCellKey, FGridSearchRecord, FGridNavCellKeyHash> Records;
	std::unordered_set<FGridNavCellKey, FGridNavCellKeyHash> Closed;
	std::unordered_map<FGridNavCellKey, bool, FGridNavCellKeyHash> UsableCellCache;
	UsableCellCache.reserve(static_cast<size_t>(std::min(std::max(16, MaxSearchNodes), 4096)));
	auto IsUsableCached = [this, &AgentProps, &UsableCellCache](const FGridNavCellKey& Key, const FGridNavCell*& OutCell) -> bool
	{
		auto Cached = UsableCellCache.find(Key);
		if (Cached != UsableCellCache.end())
		{
			OutCell = Cached->second ? FindCell(Key) : nullptr;
			return Cached->second && OutCell;
		}
		const bool bUsable = IsCellUsableForAgent(Key, AgentProps, OutCell);
		UsableCellCache[Key] = bUsable;
		return bUsable;
	};
	auto IsDiagonalMoveAllowedCached = [&IsUsableCached](const FGridNavCellKey& From, const FGridNavCellKey& Dir) -> bool
	{
		if (Dir.X == 0 || Dir.Y == 0)
		{
			return true;
		}
		const FGridNavCellKey A{From.X + Dir.X, From.Y};
		const FGridNavCellKey B{From.X, From.Y + Dir.Y};
		const FGridNavCell* CellA = nullptr;
		const FGridNavCell* CellB = nullptr;
		return IsUsableCached(A, CellA) && IsUsableCached(B, CellB);
	};

	FGridSearchRecord StartRecord;
	StartRecord.G = 0.0f;
	Records[StartKey] = StartRecord;
	Open.push(FGridOpenNode{StartKey, KeyHeuristic(StartKey, GoalKey), 0.0f});

	const FGridNavCellKey Directions[8] = {
		{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
		{ 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
	};

	FGridNavCellKey BestKey = StartKey;
	float BestH = KeyHeuristic(StartKey, GoalKey);
	bool bReachedGoal = false;
	int32 Visited = 0;

	while (!Open.empty() && Visited < MaxSearchNodes)
	{
		const FGridOpenNode Current = Open.top();
		Open.pop();
		if (Closed.find(Current.Key) != Closed.end())
		{
			continue;
		}
		Closed.insert(Current.Key);
		++Visited;

		const float H = KeyHeuristic(Current.Key, GoalKey);
		if (H < BestH)
		{
			BestH = H;
			BestKey = Current.Key;
		}
		if (Current.Key == GoalKey)
		{
			BestKey = Current.Key;
			bReachedGoal = true;
			break;
		}

		const FGridNavCell* CurrentCell = nullptr;
		if (!IsUsableCached(Current.Key, CurrentCell))
		{
			continue;
		}

		for (const FGridNavCellKey& Dir : Directions)
		{
			if (!IsDiagonalMoveAllowedCached(Current.Key, Dir))
			{
				continue;
			}
			const FGridNavCellKey Next{Current.Key.X + Dir.X, Current.Key.Y + Dir.Y};
			if (Closed.find(Next) != Closed.end())
			{
				continue;
			}
			const FGridNavCell* NextCell = nullptr;
			if (!IsUsableCached(Next, NextCell))
			{
				continue;
			}
			// Adjacent cells have already been filtered for agent radius/height clearance,
			// and diagonal corner cutting is checked by IsDiagonalMoveAllowed().  Avoid the
			// expensive sampled segment validation in the hot A* neighbor loop; reserve it
			// for direct-path and smoothing checks where a segment can span many cells.
			if (!IsHeightTransitionAllowed(CurrentCell->Location, NextCell->Location, AgentProps))
			{
				continue;
			}

			const float StepCost = (Dir.X != 0 && Dir.Y != 0) ? 1.41421356f : 1.0f;
			const float NewG = Records[Current.Key].G + StepCost;
			auto Existing = Records.find(Next);
			if (Existing != Records.end() && NewG >= Existing->second.G)
			{
				continue;
			}

			FGridSearchRecord Record;
			Record.Parent = Current.Key;
			Record.G = NewG;
			Record.bHasParent = true;
			Records[Next] = Record;
			Open.push(FGridOpenNode{Next, NewG + KeyHeuristic(Next, GoalKey), NewG});
		}
	}

	if (Records.find(BestKey) == Records.end())
	{
		return false;
	}

	TArray<FGridNavCellKey> ReversedKeys;
	FGridNavCellKey Cursor = BestKey;
	for (int32 Guard = 0; Guard < MaxSearchNodes; ++Guard)
	{
		ReversedKeys.push_back(Cursor);
		if (Cursor == StartKey)
		{
			break;
		}
		auto It = Records.find(Cursor);
		if (It == Records.end() || !It->second.bHasParent)
		{
			break;
		}
		Cursor = It->second.Parent;
	}
	if (ReversedKeys.empty() || !(ReversedKeys.back() == StartKey))
	{
		return false;
	}

	OutPath.Reset();
	AddPathPointIfDistinct(OutPath, StartLocation);
	for (auto It = ReversedKeys.rbegin(); It != ReversedKeys.rend(); ++It)
	{
		const FGridNavCell* Cell = nullptr;
		if (IsCellUsableForAgent(*It, AgentProps, Cell))
		{
			AddPathPointIfDistinct(OutPath, Cell->Location);
		}
	}
	if (bReachedGoal)
	{
		AddPathPointIfDistinct(OutPath, GoalLocation);
		AddPathPointIfDistinct(OutPath, End);
	}
	OutPath.bIsPartial = !bReachedGoal;
	OutPath.bIsValid = !OutPath.PathPoints.empty();
	SmoothPath(AgentProps, OutPath);
	return OutPath.IsValid();
}

bool AGridNavMesh::NavigationRaycast(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const
{
	SCOPE_STAT_CAT("GridNavMesh.NavigationRaycast", "NavMesh");
	FVector ProjectedStart;
	FVector ProjectedEnd;
	if (!ProjectPointToNavigation(Start, ProjectedStart, AgentProps) || !ProjectPointToNavigation(End, ProjectedEnd, AgentProps))
	{
		return true;
	}
	FNavigationPath DirectPath;
	return !BuildDirectCachedPath(ProjectedStart, ProjectedEnd, AgentProps, DirectPath);
}

bool AGridNavMesh::GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FVector& OutPoint, const FNavAgentProperties& AgentProps) const
{
	SCOPE_STAT_CAT("GridNavMesh.GetRandomReachablePoint", "NavMesh");
	if (!bNavigationDataBuilt || WalkableCells.empty())
	{
		return false;
	}
	FVector ProjectedOrigin;
	FGridNavCellKey OriginKey;
	if (!FindNearestCellForAgent(Origin, AgentProps, OriginKey, ProjectedOrigin))
	{
		return false;
	}

	const float RadiusSq = Radius * Radius;
	FVector Best = ProjectedOrigin;
	float BestDistSq = -1.0f;
	int32 Considered = 0;
	for (const auto& Pair : WalkableCells)
	{
		const FVector& Candidate = Pair.second.Location;
		const float DistSq = HorizontalDistanceSquared(ProjectedOrigin, Candidate);
		if (DistSq > RadiusSq)
		{
			continue;
		}
		++Considered;
		// Deterministic pseudo-random spread without depending on global RNG state.
		const uint32 Hash = static_cast<uint32>((Pair.first.X * 73856093) ^ (Pair.first.Y * 19349663));
		const float Score = static_cast<float>(Hash & 0xffff) + DistSq * 0.001f;
		if (Score > BestDistSq)
		{
			FNavigationPath Path;
			if (FindPath(ProjectedOrigin, Candidate, AgentProps, Path) && !Path.bIsPartial)
			{
				BestDistSq = Score;
				Best = Candidate;
			}
		}
	}
	OutPoint = Considered > 0 ? Best : ProjectedOrigin;
	return true;
}

bool AGridNavMesh::IsLocationOnNavData(const FVector& Location) const
{
	FGridNavCellKey Key;
	FVector Projected;
	return FindNearestCellForAgent(Location, SupportedAgent, Key, Projected) && FVector::DistSquared(Location, Projected) <= CellSize * CellSize;
}

void AGridNavMesh::SmoothPath(const FNavAgentProperties& AgentProps, FNavigationPath& InOutPath) const
{
	SCOPE_STAT_CAT("GridNavMesh.SmoothPath", "NavMesh");
	if (InOutPath.PathPoints.size() <= 2)
	{
		return;
	}
	TArray<FNavPathPoint> Smoothed;
	size_t Current = 0;
	Smoothed.push_back(InOutPath.PathPoints[0]);
	while (Current + 1 < InOutPath.PathPoints.size())
	{
		size_t Best = Current + 1;
		for (size_t Candidate = InOutPath.PathPoints.size() - 1; Candidate > Current + 1; --Candidate)
		{
			if (HasCachedSegment(InOutPath.PathPoints[Current].Location, InOutPath.PathPoints[Candidate].Location, AgentProps))
			{
				Best = Candidate;
				break;
			}
		}
		Smoothed.push_back(InOutPath.PathPoints[Best]);
		Current = Best;
	}
	InOutPath.PathPoints = Smoothed;
}

FColor AGridNavMesh::GetHeightDebugColor(float HeightZ, float MinZ, float MaxZ) const
{
	if (!bDrawHeightColors || MaxZ <= MinZ + 0.001f)
	{
		return FColor(40, 180, 80);
	}

	const float T = Clamp01((HeightZ - MinZ) / (MaxZ - MinZ));
	// Low to high: deep blue -> cyan -> green -> yellow -> red.  This makes
	// walkable surfaces on top of walls/boxes immediately stand out from floor cells.
	if (T < 0.25f)
	{
		return LerpColor(FColor(45, 90, 220), FColor(40, 210, 220), T / 0.25f);
	}
	if (T < 0.50f)
	{
		return LerpColor(FColor(40, 210, 220), FColor(40, 190, 80), (T - 0.25f) / 0.25f);
	}
	if (T < 0.75f)
	{
		return LerpColor(FColor(40, 190, 80), FColor(230, 220, 45), (T - 0.50f) / 0.25f);
	}
	return LerpColor(FColor(230, 220, 45), FColor(230, 60, 45), (T - 0.75f) / 0.25f);
}

bool AGridNavMesh::ShouldDrawHeightContour(float HeightZ, float MinZ) const
{
	if (!bDrawHeightColors || !bDrawHeightContours || DebugHeightContourInterval <= 0.001f)
	{
		return false;
	}
	const float LocalHeight = HeightZ - MinZ;
	const float Interval = std::max(0.001f, DebugHeightContourInterval);
	const float Mod = fmodf(fabsf(LocalHeight), Interval);
	const float DistanceToLine = std::min(Mod, Interval - Mod);
	return DistanceToLine <= std::max(0.015f, Interval * 0.06f);
}

void AGridNavMesh::DebugDrawNavigationData(float Duration) const
{
	UWorld* World = GetWorld();
	if (!World || !bNavigationDataBuilt)
	{
		return;
	}

	FDebugDrawQueue& Queue = World->GetScene().GetDebugDrawQueue();
	Queue.ClearCategory(EDebugDrawCategory::Navigation);

	const float Grid = std::max(0.25f, CellSize);
	const float Half = Grid * 0.42f;
	const int32 TotalCells = static_cast<int32>(WalkableCells.size() + (bDrawBlockedCells ? BlockedCells.size() : 0));
	const int32 Stride = ComputeDebugDrawStride(TotalCells, std::max(0, DebugDrawMaxCells));

	float MinHeight = FLT_MAX;
	float MaxHeight = -FLT_MAX;
	for (const auto& Pair : WalkableCells)
	{
		MinHeight = std::min(MinHeight, Pair.second.Location.Z);
		MaxHeight = std::max(MaxHeight, Pair.second.Location.Z);
	}
	if (WalkableCells.empty())
	{
		MinHeight = 0.0f;
		MaxHeight = 0.0f;
	}

	auto AddNavLine = [&Queue, Duration](const FVector& A, const FVector& B, const FColor& Color)
	{
		Queue.AddLine(A, B, Color, Duration, EDebugDrawCategory::Navigation);
	};

	auto AddNavBox = [&Queue, Duration](const FVector& Center, const FVector& Extent, const FColor& Color)
	{
		Queue.AddBox(Center, Extent, Color, Duration, EDebugDrawCategory::Navigation);
	};

	auto DrawCellSquare = [&AddNavLine, Half](const FVector& Center, const FColor& Color)
	{
		const FVector A = Center + FVector(-Half, -Half, 0.0f);
		const FVector B = Center + FVector(Half, -Half, 0.0f);
		const FVector C = Center + FVector(Half, Half, 0.0f);
		const FVector D = Center + FVector(-Half, Half, 0.0f);
		AddNavLine(A, B, Color);
		AddNavLine(B, C, Color);
		AddNavLine(C, D, Color);
		AddNavLine(D, A, Color);
	};

	auto DrawContourMark = [&AddNavLine, Half](const FVector& Center)
	{
		const float Inner = Half * 0.55f;
		AddNavLine(Center + FVector(-Inner, 0.0f, 0.0f), Center + FVector(Inner, 0.0f, 0.0f), FColor(245, 245, 245));
		AddNavLine(Center + FVector(0.0f, -Inner, 0.0f), Center + FVector(0.0f, Inner, 0.0f), FColor(245, 245, 245));
	};

	if (CachedBounds.IsValid())
	{
		AddNavBox(CachedBounds.GetCenter(), CachedBounds.GetExtent(), FColor(80, 160, 255));
	}

	int32 DrawIndex = 0;
	for (const auto& Pair : WalkableCells)
	{
		if ((DrawIndex++ % Stride) != 0)
		{
			continue;
		}
		// 메인 navmesh 와 끊긴 "섬" 셀은 밝은 마젠타로 강조한다. 여기 선 에이전트는 경로를 못 찾는다.
		const bool bIsolated = IsolatedCells.find(Pair.first) != IsolatedCells.end();
		const FVector Center = Pair.second.Location + FVector(0.0f, 0.0f, bIsolated ? 6.0f : 4.0f);
		if (bIsolated)
		{
			DrawCellSquare(Center, FColor(255, 0, 220));
			DrawContourMark(Center + FVector(0.0f, 0.0f, 0.02f));
			continue;
		}
		DrawCellSquare(Center, GetHeightDebugColor(Pair.second.Location.Z, MinHeight, MaxHeight));
		if (ShouldDrawHeightContour(Pair.second.Location.Z, MinHeight))
		{
			DrawContourMark(Center + FVector(0.0f, 0.0f, 0.02f));
		}
	}

	// Blocked cells must be drawn last and above the walkable grid.  Otherwise an
	// obstacle footprint can be correctly blocked but still look green because a
	// nearby walkable surface line is drawn over it.
	if (bDrawBlockedCells)
	{
		for (const auto& Pair : BlockedCells)
		{
			if ((DrawIndex++ % Stride) != 0)
			{
				continue;
			}
			const FVector Center = Pair.second + FVector(0.0f, 0.0f, 7.0f);
			AddNavLine(Center + FVector(-Half, -Half, 0.0f), Center + FVector(Half, Half, 0.0f), FColor(230, 35, 35));
			AddNavLine(Center + FVector(Half, -Half, 0.0f), Center + FVector(-Half, Half, 0.0f), FColor(230, 35, 35));
			DrawCellSquare(Center, FColor(180, 30, 30));
		}
	}
}
