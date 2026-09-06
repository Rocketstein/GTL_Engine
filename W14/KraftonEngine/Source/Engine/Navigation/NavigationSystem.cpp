#include "Navigation/NavigationSystem.h"

#include "Navigation/GridNavMesh.h"
#include "Navigation/NavigationData.h"
#include "Core/Logging/Log.h"
#include "Core/Types/EngineTypes.h"
#include "GameFramework/Actor/NavMeshBoundsVolume.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldSettings.h"
#include "Math/MathUtils.h"
#include "Math/Quat.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
	struct FGridKey
	{
		int32 X = 0;
		int32 Y = 0;

		bool operator==(const FGridKey& Other) const
		{
			return X == Other.X && Y == Other.Y;
		}
	};

	struct FGridKeyHash
	{
		size_t operator()(const FGridKey& Key) const
		{
			const uint64 A = static_cast<uint32>(Key.X);
			const uint64 B = static_cast<uint32>(Key.Y);
			return static_cast<size_t>((A << 32) ^ B ^ 0x9e3779b97f4a7c15ull);
		}
	};

	struct FOpenNode
	{
		FGridKey Key;
		float F = 0.0f;
		float G = 0.0f;
		bool operator<(const FOpenNode& Other) const
		{
			return F > Other.F;
		}
	};

	struct FGridNodeRecord
	{
		FGridKey Parent;
		float G = FLT_MAX;
		bool bHasParent = false;
	};

	float Heuristic(const FGridKey& A, const FGridKey& B)
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

	FVector FlatDirection(const FVector& Start, const FVector& End)
	{
		FVector Delta = End - Start;
		Delta.Z = 0.0f;
		if (Delta.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}
		return Delta.Normalized();
	}

	FVector CellToWorld(const FVector& Origin, float CellSize, const FGridKey& Key)
	{
		return FVector(Origin.X + static_cast<float>(Key.X) * CellSize, Origin.Y + static_cast<float>(Key.Y) * CellSize, Origin.Z);
	}

	FGridKey WorldToCell(const FVector& Origin, float CellSize, const FVector& Point)
	{
		return FGridKey{
			static_cast<int32>(std::lround((Point.X - Origin.X) / CellSize)),
			static_cast<int32>(std::lround((Point.Y - Origin.Y) / CellSize))
		};
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
}

UNavigationSystem* UNavigationSystem::GetCurrent(UWorld* World)
{
	return World ? World->GetNavigationSystem() : nullptr;
}

UWorld* UNavigationSystem::GetOwningWorld() const
{
	return GetTypedOuter<UWorld>();
}


ANavigationData* UNavigationSystem::FindNavigationDataActor() const
{
	if (ANavigationData* Cached = CachedMainNavData.Get())
	{
		return Cached;
	}
	UWorld* World = GetOwningWorld();
	if (!World)
	{
		return nullptr;
	}

	ANavigationData* FirstData = nullptr;
	for (AActor* Actor : World->GetActors())
	{
		if (AGridNavMesh* Grid = Cast<AGridNavMesh>(Actor))
		{
			CachedMainNavData.Reset(Grid);
			return Grid;
		}
		if (!FirstData)
		{
			FirstData = Cast<ANavigationData>(Actor);
		}
	}
	if (FirstData)
	{
		CachedMainNavData.Reset(FirstData);
	}
	return FirstData;
}

AGridNavMesh* UNavigationSystem::EnsureGridNavMeshActor()
{
	if (AGridNavMesh* Existing = Cast<AGridNavMesh>(FindNavigationDataActor()))
	{
		CachedMainNavData.Reset(Existing);
		return Existing;
	}
	UWorld* World = GetOwningWorld();
	if (!World)
	{
		return nullptr;
	}
	AGridNavMesh* Created = World->SpawnActor<AGridNavMesh>();
	if (!Created)
	{
		return nullptr;
	}
	PushSettingsToGridNavMesh(*Created);
	CachedMainNavData.Reset(Created);
	return Created;
}

void UNavigationSystem::PushSettingsToGridNavMesh(AGridNavMesh& Grid) const
{
	Grid.SetCellSize(CellSize);
	FNavAgentProperties BuildAgent;
	BuildAgent.Radius = AgentRadius;
	BuildAgent.Height = AgentHeight;
	BuildAgent.StepHeight = AgentStepHeight;
	BuildAgent.MaxClimbHeight = AgentMaxClimbHeight;
	BuildAgent.MaxDropHeight = AgentMaxDropHeight;
	BuildAgent.MaxSlopeDegrees = AgentMaxSlopeDegrees;
	Grid.SetSupportedAgentForBuild(BuildAgent);
	Grid.MaxSearchNodes = MaxSearchNodes;
	Grid.ProjectionUp = ProjectionUp;
	Grid.ProjectionDown = ProjectionDown;
	Grid.DirectPathSegmentLength = DirectPathSegmentLength;
	Grid.ObstaclePadding = ObstaclePadding;
	Grid.bUsePhysicsProjectionFallback = bUsePhysicsProjectionFallback;
	Grid.bDrawDebugOnBuild = bDrawDebugOnBuild;
	Grid.bDrawBlockedCells = bDrawBlockedCells;
	Grid.bDrawHeightColors = bDrawHeightColors;
	Grid.bDrawHeightContours = bDrawHeightContours;
	Grid.DebugHeightContourInterval = DebugHeightContourInterval;
	Grid.DebugDrawDuration = DebugDrawDuration;
	Grid.DebugDrawMaxCells = DebugDrawMaxCells;
}

void UNavigationSystem::ApplyWorldSettings(const FNavigationWorldSettings& Settings)
{
	CellSize = std::max(0.25f, Settings.CellSize);
	MaxSearchNodes = std::max(16, Settings.MaxSearchNodes);
	AgentRadius = std::max(0.01f, Settings.AgentRadius);
	AgentHeight = std::max(AgentRadius * 2.0f, Settings.AgentHeight);
	AgentStepHeight = std::max(0.0f, Settings.AgentStepHeight);
	AgentMaxClimbHeight = std::max(0.0f, Settings.AgentMaxClimbHeight);
	AgentMaxDropHeight = std::max(0.0f, Settings.AgentMaxDropHeight);
	AgentMaxSlopeDegrees = std::max(0.0f, std::min(89.0f, Settings.AgentMaxSlopeDegrees));
	ProjectionUp = std::max(0.0f, Settings.ProjectionUp);
	ProjectionDown = std::max(0.0f, Settings.ProjectionDown);
	DirectPathSegmentLength = std::max(0.1f, Settings.DirectPathSegmentLength);
	ObstaclePadding = std::max(0.0f, Settings.ObstaclePadding);
	bUsePhysicsProjectionFallback = Settings.bUsePhysicsProjectionFallback;
	bUseNavigationData = Settings.bUseNavigationData;
	bAutoRebuildOnPathRequest = Settings.bAutoRebuildOnPathRequest;
	bAllowRuntimeFallback = Settings.bAllowRuntimeFallback;
	bEnableRuntimeAutoRebuild = Settings.bEnableRuntimeAutoRebuild;
	RuntimeRebuildDelay = std::max(0.0f, Settings.RuntimeRebuildDelay);
	RuntimeRebuildMinInterval = std::max(0.0f, Settings.RuntimeRebuildMinInterval);
	bDrawDebugOnBuild = Settings.bDrawDebugOnBuild;
	bDrawBlockedCells = Settings.bDrawBlockedCells;
	bDrawHeightColors = Settings.bDrawHeightColors;
	bDrawHeightContours = Settings.bDrawHeightContours;
	DebugHeightContourInterval = std::max(0.0f, Settings.DebugHeightContourInterval);
	DebugDrawDuration = std::max(0.0f, Settings.DebugDrawDuration);
	DebugDrawMaxCells = std::max(0, Settings.DebugDrawMaxCells);

	if (AGridNavMesh* Grid = GetGridNavMesh())
	{
		PushSettingsToGridNavMesh(*Grid);
	}
}

void UNavigationSystem::ExportWorldSettings(FNavigationWorldSettings& OutSettings) const
{
	OutSettings.CellSize = CellSize;
	OutSettings.MaxSearchNodes = MaxSearchNodes;
	OutSettings.AgentRadius = AgentRadius;
	OutSettings.AgentHeight = AgentHeight;
	OutSettings.AgentStepHeight = AgentStepHeight;
	OutSettings.AgentMaxClimbHeight = AgentMaxClimbHeight;
	OutSettings.AgentMaxDropHeight = AgentMaxDropHeight;
	OutSettings.AgentMaxSlopeDegrees = AgentMaxSlopeDegrees;
	OutSettings.ProjectionUp = ProjectionUp;
	OutSettings.ProjectionDown = ProjectionDown;
	OutSettings.DirectPathSegmentLength = DirectPathSegmentLength;
	OutSettings.ObstaclePadding = ObstaclePadding;
	OutSettings.bUsePhysicsProjectionFallback = bUsePhysicsProjectionFallback;
	OutSettings.bUseNavigationData = bUseNavigationData;
	OutSettings.bAutoRebuildOnPathRequest = bAutoRebuildOnPathRequest;
	OutSettings.bAllowRuntimeFallback = bAllowRuntimeFallback;
	OutSettings.bEnableRuntimeAutoRebuild = bEnableRuntimeAutoRebuild;
	OutSettings.RuntimeRebuildDelay = RuntimeRebuildDelay;
	OutSettings.RuntimeRebuildMinInterval = RuntimeRebuildMinInterval;
	OutSettings.bDrawDebugOnBuild = bDrawDebugOnBuild;
	OutSettings.bDrawBlockedCells = bDrawBlockedCells;
	OutSettings.bDrawHeightColors = bDrawHeightColors;
	OutSettings.bDrawHeightContours = bDrawHeightContours;
	OutSettings.DebugHeightContourInterval = DebugHeightContourInterval;
	OutSettings.DebugDrawDuration = DebugDrawDuration;
	OutSettings.DebugDrawMaxCells = DebugDrawMaxCells;
}

ANavigationData* UNavigationSystem::GetMainNavigationData() const
{
	return FindNavigationDataActor();
}

AGridNavMesh* UNavigationSystem::GetGridNavMesh() const
{
	return Cast<AGridNavMesh>(FindNavigationDataActor());
}

bool UNavigationSystem::HasBuiltNavigationData() const
{
	ANavigationData* Data = FindNavigationDataActor();
	return Data && Data->IsNavigationDataBuilt();
}

void UNavigationSystem::InvalidateNavigationData()
{
	if (bEnableRuntimeAutoRebuild)
	{
		RequestNavigationRebuild("Navigation data invalidated", false);
		return;
	}

	if (ANavigationData* Data = FindNavigationDataActor())
	{
		Data->ClearNavigationData();
	}
}

void UNavigationSystem::RequestNavigationRebuild(const FString& Reason, bool bImmediate)
{
	if (bImmediate)
	{
		RebuildNavigation();
		return;
	}

	if (!bEnableRuntimeAutoRebuild)
	{
		LastQueryMessage = FString("Navigation rebuild request ignored: runtime auto rebuild disabled");
		if (!Reason.empty())
		{
			LastQueryMessage += FString("; Trigger=") + Reason;
		}
		return;
	}

	bRebuildPending = true;
	PendingRebuildAge = 0.0f;
	PendingRebuildReason = Reason.empty() ? FString("Navigation data changed") : Reason;
	LastQueryMessage = FString("Navigation rebuild queued: ") + PendingRebuildReason;
}

void UNavigationSystem::Tick(float DeltaTime)
{
	SCOPE_STAT_CAT("NavigationSystem.Tick", "NavMesh");
	TimeSinceLastRebuild += std::max(0.0f, DeltaTime);
	if (!bRebuildPending || !bEnableRuntimeAutoRebuild)
	{
		return;
	}

	PendingRebuildAge += std::max(0.0f, DeltaTime);
	if (PendingRebuildAge < RuntimeRebuildDelay)
	{
		return;
	}
	if (TimeSinceLastRebuild < RuntimeRebuildMinInterval)
	{
		return;
	}

	const FString Reason = PendingRebuildReason;
	bRebuildPending = false;
	PendingRebuildAge = 0.0f;
	PendingRebuildReason.clear();
	RebuildNavigation();
	if (!Reason.empty())
	{
		LastQueryMessage += FString("; Trigger=") + Reason;
	}
}

bool UNavigationSystem::RebuildNavigation()
{
	SCOPE_STAT_CAT("NavigationSystem.Rebuild", "NavMesh");
	LastQueryMessage.clear();
	AGridNavMesh* Grid = EnsureGridNavMeshActor();
	if (Grid)
	{
		PushSettingsToGridNavMesh(*Grid);
		const bool bBuilt = Grid->RebuildNavigationData();
		CachedMainNavData.Reset(Grid);
		LastQueryMessage = Grid->GetLastBuildMessage();
		TimeSinceLastRebuild = 0.0f;
		// Game 빌드에는 에디터 nav 오버레이가 없어 빌드 결과를 볼 수 없다. 빌드마다 결과를 로그로 남겨
		// "BeginPlay 시점 navmesh 가 비었는지"(Walkable=0 → 보스 길찾기 실패의 정체)를 콘솔/로그에서 확인.
		UE_LOG("[Nav] RebuildNavigation Built=%s %s", bBuilt ? "true" : "false", LastQueryMessage.c_str());
		return bBuilt;
	}

	ANavigationData* Data = FindNavigationDataActor();
	const bool bBuilt = Data && Data->RebuildNavigationData();
	LastQueryMessage = bBuilt ? "RebuildNavigation succeeded" : "RebuildNavigation failed: no NavigationData";
	TimeSinceLastRebuild = 0.0f;
	UE_LOG("[Nav] RebuildNavigation Built=%s %s", bBuilt ? "true" : "false", LastQueryMessage.c_str());
	return bBuilt;
}

void UNavigationSystem::DebugDrawNavigationData(float Duration) const
{
	if (ANavigationData* Data = FindNavigationDataActor())
	{
		Data->DebugDrawNavigationData(Duration);
	}
}

FString UNavigationSystem::GetDebugSummary() const
{
	ANavigationData* Data = FindNavigationDataActor();
	if (!Data)
	{
		return "NavigationData=None; Built=false";
	}
	return FString("NavigationData=") + Data->GetName()
		+ "; Built=" + (Data->IsNavigationDataBuilt() ? "true" : "false")
		+ "; Pending=" + (bRebuildPending ? "true" : "false")
		+ "; CellSize=" + std::to_string(CellSize)
		+ "; AgentRadius=" + std::to_string(AgentRadius)
		+ "; Climb=" + std::to_string(AgentMaxClimbHeight)
		+ "; Drop=" + std::to_string(AgentMaxDropHeight)
		+ "; Padding=" + std::to_string(ObstaclePadding)
		+ "; Nodes=" + std::to_string(Data->GetNavigationNodeCount())
		+ "; Blocked=" + std::to_string(Data->GetBlockedNodeCount())
		+ "; LastQuery=" + LastQueryMessage;
}

bool UNavigationSystem::HasNavigationBounds() const
{
	UWorld* World = GetOwningWorld();
	if (!World)
	{
		return false;
	}
	for (AActor* Actor : World->GetActors())
	{
		if (Cast<ANavMeshBoundsVolume>(Actor))
		{
			return true;
		}
	}
	return false;
}

bool UNavigationSystem::IsPointInsideNavigationBounds(const FVector& Point) const
{
	UWorld* World = GetOwningWorld();
	if (!World)
	{
		return false;
	}
	bool bHasBounds = false;
	for (AActor* Actor : World->GetActors())
	{
		ANavMeshBoundsVolume* Bounds = Cast<ANavMeshBoundsVolume>(Actor);
		if (!Bounds)
		{
			continue;
		}
		bHasBounds = true;
		if (Bounds->ContainsPoint(Point))
		{
			return true;
		}
	}
	return !bHasBounds;
}

bool UNavigationSystem::IsPointAllowedByBounds(const FVector& Point) const
{
	return IsPointInsideNavigationBounds(Point);
}

bool UNavigationSystem::IsWalkableFloorHitRuntime(const FHitResult& Hit, const FNavAgentProperties& AgentProps) const
{
	if (!Hit.bHit)
	{
		return false;
	}
	const float MinWalkableZ = cosf(AgentProps.MaxSlopeDegrees * FMath::DegToRad);
	const FVector Normal = Hit.WorldNormal.IsNearlyZero() ? Hit.ImpactNormal : Hit.WorldNormal;
	if (!Normal.IsNearlyZero() && Normal.Z < MinWalkableZ)
	{
		return false;
	}
	return true;
}

bool UNavigationSystem::IsWalkableFloorHit(const FHitResult& Hit, const FNavAgentProperties& AgentProps) const
{
	return IsWalkableFloorHitRuntime(Hit, AgentProps);
}

bool UNavigationSystem::ProjectPointToNavigation(const FVector& Point, FVector& OutProjectedPoint, const FNavAgentProperties& AgentProps) const
{
	SCOPE_STAT_CAT("NavigationSystem.ProjectPoint", "NavMesh");
	LastQueryMessage.clear();
	if (bUseNavigationData)
	{
		ANavigationData* Data = FindNavigationDataActor();
		if ((!Data || !Data->IsNavigationDataBuilt()) && bAutoRebuildOnPathRequest)
		{
			const_cast<UNavigationSystem*>(this)->RebuildNavigation();
			Data = FindNavigationDataActor();
		}
		if (Data && Data->IsNavigationDataBuilt() && Data->ProjectPointToNavigation(Point, OutProjectedPoint, AgentProps))
		{
			LastQueryMessage = "ProjectPointToNavigation succeeded on NavigationData";
			return true;
		}
		if (!bAllowRuntimeFallback)
		{
			return SetLastQueryMessage("ProjectPointToNavigation failed: no built NavigationData and runtime fallback disabled");
		}
	}
	if (ProjectPointToNavigationRuntime(Point, OutProjectedPoint, AgentProps))
	{
		LastQueryMessage = "ProjectPointToNavigation succeeded by runtime fallback";
		return true;
	}
	return SetLastQueryMessage("ProjectPointToNavigation failed: no walkable floor or outside navigation bounds");
}

bool UNavigationSystem::ProjectPointToNavigationRuntime(const FVector& Point, FVector& OutProjectedPoint, const FNavAgentProperties& AgentProps, float InProjectionUp, float InProjectionDown) const
{
	SCOPE_STAT_CAT("NavigationSystem.ProjectPointRuntime", "NavMesh");
	UWorld* World = GetOwningWorld();
	if (!World)
	{
		return false;
	}

	const float Up = InProjectionUp >= 0.0f ? InProjectionUp : ProjectionUp;
	const float Down = InProjectionDown >= 0.0f ? InProjectionDown : ProjectionDown;
	const FVector Start(Point.X, Point.Y, Point.Z + Up);
	const FVector Dir(0.0f, 0.0f, -1.0f);
	FHitResult Hit;
	if (!World->PhysicsRaycastByObjectTypes(Start, Dir, Up + Down, Hit, ObjectTypeBit(ECollisionChannel::WorldStatic)))
	{
		return false;
	}
	if (!IsWalkableFloorHitRuntime(Hit, AgentProps))
	{
		return false;
	}
	// Some mesh raycast paths only populate Distance/HitComponent and leave WorldHitLocation at its
	// default value.  Navigation projection must not treat that default as the floor position.
	// Reconstruct the hit location from the ray whenever Distance is valid; this is also the
	// most stable source for vertical projection queries.
	if (Hit.Distance < FLT_MAX * 0.5f)
	{
		OutProjectedPoint = Start + Dir * Hit.Distance;
	}
	else
	{
		OutProjectedPoint = Hit.WorldHitLocation;
	}
	if (!IsPointAllowedByBounds(OutProjectedPoint))
	{
		return false;
	}
	return true;
}

bool UNavigationSystem::HasBlockingObstacleBetween(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const
{
	SCOPE_STAT_CAT("NavigationSystem.ObstacleSweep", "NavMesh");
	UWorld* World = GetOwningWorld();
	if (!World)
	{
		return true;
	}
	FVector Direction = FlatDirection(Start, End);
	if (Direction.IsNearlyZero())
	{
		return false;
	}
	const FVector SweepStart(Start.X, Start.Y, Start.Z + std::max(AgentProps.Radius, AgentProps.Height * 0.5f));
	const FVector SweepEnd(End.X, End.Y, End.Z + std::max(AgentProps.Radius, AgentProps.Height * 0.5f));
	FHitResult Hit;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(std::max(0.05f, AgentProps.Radius));
	return World->PhysicsSweepByObjectTypes(SweepStart, SweepEnd, FQuat(), Shape, Hit, ObjectTypeBit(ECollisionChannel::WorldStatic));
}

bool UNavigationSystem::CanTraverseSegment(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const
{
	return CanTraverseSegmentRuntime(Start, End, AgentProps);
}

bool UNavigationSystem::CanTraverseSegmentRuntime(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const
{
	if (!IsPointAllowedByBounds(Start) || !IsPointAllowedByBounds(End))
	{
		return false;
	}
	const float DeltaZ = End.Z - Start.Z;
	const float HeightTolerance = 0.02f;
	// 두 점이 수평으로 떨어져 있으면 높이차는 수직 단차가 아니라 경사로다. MaxSlopeDegrees 로 걸을 수 있는
	// 연속 비탈은 셀 간 상승량이 이산 단차 한계를 넘더라도 통과시킨다(GridNavMesh 와 동일한 규칙).
	const float HorizontalDist = sqrtf(HorizontalDistanceSquared(Start, End));
	const float SlopeDeg = std::max(0.0f, std::min(89.0f, AgentProps.MaxSlopeDegrees));
	const float SlopeAllowance = HorizontalDist * tanf(SlopeDeg * FMath::DegToRad);
	if (DeltaZ > std::max(AgentProps.GetEffectiveMaxClimbHeight(), SlopeAllowance) + HeightTolerance)
	{
		return false;
	}
	if (-DeltaZ > std::max(AgentProps.GetEffectiveMaxDropHeight(), SlopeAllowance) + HeightTolerance)
	{
		return false;
	}
	if (HasBlockingObstacleBetween(Start, End, AgentProps))
	{
		return false;
	}
	return true;
}

bool UNavigationSystem::NavigationRaycast(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const
{
	SCOPE_STAT_CAT("NavigationSystem.NavigationRaycast", "NavMesh");
	if (bUseNavigationData)
	{
		ANavigationData* Data = FindNavigationDataActor();
		if ((!Data || !Data->IsNavigationDataBuilt()) && bAutoRebuildOnPathRequest)
		{
			const_cast<UNavigationSystem*>(this)->RebuildNavigation();
			Data = FindNavigationDataActor();
		}
		if (Data && Data->IsNavigationDataBuilt())
		{
			return Data->NavigationRaycast(Start, End, AgentProps);
		}
		if (!bAllowRuntimeFallback)
		{
			return true;
		}
	}

	FVector ProjectedStart;
	FVector ProjectedEnd;
	if (!ProjectPointToNavigationRuntime(Start, ProjectedStart, AgentProps) || !ProjectPointToNavigationRuntime(End, ProjectedEnd, AgentProps))
	{
		return true;
	}
	return !CanTraverseSegmentRuntime(ProjectedStart, ProjectedEnd, AgentProps);
}

bool UNavigationSystem::BuildDirectPathIfReachable(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const
{
	SCOPE_STAT_CAT("NavigationSystem.BuildDirectPath", "NavMesh");
	const float Dist = sqrtf(HorizontalDistanceSquared(Start, End));
	const int32 Segments = std::max(1, static_cast<int32>(ceilf(Dist / std::max(0.1f, DirectPathSegmentLength))));
	FVector Previous = Start;
	for (int32 Index = 1; Index <= Segments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(Segments);
		const FVector Sample(Start.X + (End.X - Start.X) * Alpha, Start.Y + (End.Y - Start.Y) * Alpha, Start.Z + (End.Z - Start.Z) * Alpha);
		FVector Projected;
		if (!ProjectPointToNavigationRuntime(Sample, Projected, AgentProps))
		{
			return false;
		}
		if (!CanTraverseSegmentRuntime(Previous, Projected, AgentProps))
		{
			return false;
		}
		Previous = Projected;
	}

	OutPath.Reset();
	AddPathPointIfDistinct(OutPath, Start);
	AddPathPointIfDistinct(OutPath, End);
	OutPath.bIsValid = true;
	return true;
}

bool UNavigationSystem::FindPathSync(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const
{
	SCOPE_STAT_CAT("NavigationSystem.FindPathSync", "NavMesh");
	LastQueryMessage.clear();
	OutPath.Reset();
	if (bUseNavigationData)
	{
		ANavigationData* Data = FindNavigationDataActor();
		if ((!Data || !Data->IsNavigationDataBuilt()) && bAutoRebuildOnPathRequest)
		{
			const_cast<UNavigationSystem*>(this)->RebuildNavigation();
			Data = FindNavigationDataActor();
		}
		if (Data && Data->IsNavigationDataBuilt() && Data->FindPath(Start, End, AgentProps, OutPath))
		{
			LastQueryMessage = FString("FindPathSync succeeded on NavigationData; Points=") + std::to_string(OutPath.Num()) + (OutPath.bIsPartial ? "; Partial=true" : "; Partial=false");
			return true;
		}
		if (!bAllowRuntimeFallback)
		{
			return SetLastQueryMessage("FindPathSync failed: NavigationData did not return a path and runtime fallback is disabled");
		}
	}

	FVector ProjectedStart;
	FVector ProjectedEnd;
	if (!ProjectPointToNavigationRuntime(Start, ProjectedStart, AgentProps))
	{
		return SetLastQueryMessage("FindPathSync failed: start point is not on navigation");
	}
	if (!ProjectPointToNavigationRuntime(End, ProjectedEnd, AgentProps))
	{
		return SetLastQueryMessage("FindPathSync failed: end point is not on navigation");
	}
	if (BuildDirectPathIfReachable(ProjectedStart, ProjectedEnd, AgentProps, OutPath))
	{
		LastQueryMessage = FString("FindPathSync succeeded by direct runtime path; Points=") + std::to_string(OutPath.Num());
		return true;
	}
	if (FindGridPath(ProjectedStart, ProjectedEnd, AgentProps, OutPath))
	{
		LastQueryMessage = FString("FindPathSync succeeded by runtime grid path; Points=") + std::to_string(OutPath.Num()) + (OutPath.bIsPartial ? "; Partial=true" : "; Partial=false");
		return true;
	}
	return SetLastQueryMessage("FindPathSync failed: no connected path found");
}

bool UNavigationSystem::FindGridPath(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const
{
	SCOPE_STAT_CAT("NavigationSystem.FindGridPath", "NavMesh");
	const float GridSize = std::max(0.5f, CellSize);
	const FGridKey StartKey{0, 0};
	const FGridKey GoalKey = WorldToCell(Start, GridSize, End);
	const int32 Bound = std::max(4, static_cast<int32>(ceilf(sqrtf(HorizontalDistanceSquared(Start, End)) / GridSize)) + 16);

	std::priority_queue<FOpenNode> Open;
	std::unordered_map<FGridKey, FGridNodeRecord, FGridKeyHash> Records;
	std::unordered_set<FGridKey, FGridKeyHash> Closed;
	std::unordered_map<FGridKey, FVector, FGridKeyHash> ProjectedCache;

	auto GetProjected = [&](const FGridKey& Key, FVector& Out) -> bool
	{
		auto Found = ProjectedCache.find(Key);
		if (Found != ProjectedCache.end())
		{
			Out = Found->second;
			return true;
		}
		const FVector Guess = CellToWorld(Start, GridSize, Key);
		if (!ProjectPointToNavigationRuntime(Guess, Out, AgentProps))
		{
			return false;
		}
		ProjectedCache[Key] = Out;
		return true;
	};

	FGridNodeRecord StartRecord;
	StartRecord.G = 0.0f;
	Records[StartKey] = StartRecord;
	ProjectedCache[StartKey] = Start;
	ProjectedCache[GoalKey] = End;
	Open.push(FOpenNode{StartKey, Heuristic(StartKey, GoalKey), 0.0f});

	const FGridKey Directions[8] = {
		{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
		{ 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
	};

	FGridKey BestKey = StartKey;
	float BestHeuristic = Heuristic(StartKey, GoalKey);
	int32 Visited = 0;
	bool bReachedGoal = false;

	while (!Open.empty() && Visited < MaxSearchNodes)
	{
		const FOpenNode Current = Open.top();
		Open.pop();
		if (Closed.find(Current.Key) != Closed.end())
		{
			continue;
		}
		Closed.insert(Current.Key);
		++Visited;

		const float CurrentH = Heuristic(Current.Key, GoalKey);
		if (CurrentH < BestHeuristic)
		{
			BestHeuristic = CurrentH;
			BestKey = Current.Key;
		}

		if (Current.Key == GoalKey)
		{
			BestKey = Current.Key;
			bReachedGoal = true;
			break;
		}

		FVector CurrentLocation;
		if (!GetProjected(Current.Key, CurrentLocation))
		{
			continue;
		}

		for (const FGridKey& Dir : Directions)
		{
			const FGridKey Next{ Current.Key.X + Dir.X, Current.Key.Y + Dir.Y };
			if (std::abs(Next.X) > Bound || std::abs(Next.Y) > Bound)
			{
				continue;
			}
			if (Closed.find(Next) != Closed.end())
			{
				continue;
			}

			FVector NextLocation;
			if (!GetProjected(Next, NextLocation))
			{
				continue;
			}
			if (!CanTraverseSegmentRuntime(CurrentLocation, NextLocation, AgentProps))
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

			FGridNodeRecord Rec;
			Rec.Parent = Current.Key;
			Rec.G = NewG;
			Rec.bHasParent = true;
			Records[Next] = Rec;
			Open.push(FOpenNode{ Next, NewG + Heuristic(Next, GoalKey), NewG });
		}
	}

	if (Records.find(BestKey) == Records.end())
	{
		return false;
	}

	TArray<FGridKey> ReversedKeys;
	FGridKey Cursor = BestKey;
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
	for (auto It = ReversedKeys.rbegin(); It != ReversedKeys.rend(); ++It)
	{
		FVector Location;
		if (GetProjected(*It, Location))
		{
			AddPathPointIfDistinct(OutPath, Location);
		}
	}
	if (bReachedGoal)
	{
		AddPathPointIfDistinct(OutPath, End);
	}
	OutPath.bIsPartial = !bReachedGoal;
	OutPath.bIsValid = !OutPath.PathPoints.empty();
	SmoothPath(AgentProps, OutPath);
	return OutPath.IsValid();
}

void UNavigationSystem::SmoothPath(const FNavAgentProperties& AgentProps, FNavigationPath& InOutPath) const
{
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
			if (CanTraverseSegmentRuntime(InOutPath.PathPoints[Current].Location, InOutPath.PathPoints[Candidate].Location, AgentProps))
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

bool UNavigationSystem::GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FVector& OutPoint, const FNavAgentProperties& AgentProps) const
{
	if (bUseNavigationData)
	{
		ANavigationData* Data = FindNavigationDataActor();
		if ((!Data || !Data->IsNavigationDataBuilt()) && bAutoRebuildOnPathRequest)
		{
			const_cast<UNavigationSystem*>(this)->RebuildNavigation();
			Data = FindNavigationDataActor();
		}
		if (Data && Data->IsNavigationDataBuilt() && Data->GetRandomReachablePointInRadius(Origin, Radius, OutPoint, AgentProps))
		{
			return true;
		}
		if (!bAllowRuntimeFallback)
		{
			return false;
		}
	}

	FVector ProjectedOrigin;
	if (!ProjectPointToNavigationRuntime(Origin, ProjectedOrigin, AgentProps))
	{
		return false;
	}

	const int32 RingCount = 8;
	const int32 SamplesPerRing = 12;
	for (int32 Ring = 1; Ring <= RingCount; ++Ring)
	{
		const float R = Radius * static_cast<float>(Ring) / static_cast<float>(RingCount);
		for (int32 Sample = 0; Sample < SamplesPerRing; ++Sample)
		{
			const float Angle = (static_cast<float>(Sample) / static_cast<float>(SamplesPerRing)) * 2.0f * FMath::Pi;
			const FVector Candidate(Origin.X + cosf(Angle) * R, Origin.Y + sinf(Angle) * R, Origin.Z);
			FVector Projected;
			if (!ProjectPointToNavigationRuntime(Candidate, Projected, AgentProps))
			{
				continue;
			}
			FNavigationPath Path;
			if (FindPathSync(ProjectedOrigin, Projected, AgentProps, Path) && !Path.bIsPartial)
			{
				OutPoint = Projected;
				return true;
			}
		}
	}
	OutPoint = ProjectedOrigin;
	return true;
}
