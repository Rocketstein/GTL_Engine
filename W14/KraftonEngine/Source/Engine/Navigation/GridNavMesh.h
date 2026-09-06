#pragma once

#include "Navigation/NavigationData.h"
#include "Core/Types/EngineTypes.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

class ANavMeshBoundsVolume;
class UNavigationSystem;

#include "Source/Engine/Navigation/GridNavMesh.generated.h"

struct FGridNavCellKey
{
	int32 X = 0;
	int32 Y = 0;

	bool operator==(const FGridNavCellKey& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}
};

struct FGridNavCellKeyHash
{
	size_t operator()(const FGridNavCellKey& Key) const
	{
		const uint64 A = static_cast<uint32>(Key.X);
		const uint64 B = static_cast<uint32>(Key.Y);
		return static_cast<size_t>((A << 32) ^ B ^ 0x9e3779b97f4a7c15ull);
	}
};

struct FGridNavCell
{
	FVector Location = FVector::ZeroVector;
	bool bWalkable = false;
	// Horizontal free radius from this cell center to the nearest blocking obstacle.
	// Path queries still re-check cached obstacle bounds with the requesting agent height,
	// but this gives a cheap first-order clearance value for debugging and filtering.
	float ClearanceRadius = 0.0f;
};

// ============================================================
// AGridNavMesh
// Recast를 붙이기 전 단계의 실제 NavigationData 구현.
// NavMeshBoundsVolume 내부를 grid로 스캔해 walkable cell 그래프를 캐시한다.
// ============================================================
UCLASS()
class AGridNavMesh : public ANavigationData
{
public:
	GENERATED_BODY()
	AGridNavMesh();
	~AGridNavMesh() override = default;

	bool RebuildNavigationData() override;
	void ClearNavigationData() override;
	void PostEditProperty(const char* PropertyName) override;

	bool ProjectPointToNavigation(const FVector& Point, FVector& OutProjectedPoint, const FNavAgentProperties& AgentProps) const override;
	bool FindPath(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const override;
	bool NavigationRaycast(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const override;
	bool GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FVector& OutPoint, const FNavAgentProperties& AgentProps) const override;

	void DebugDrawNavigationData(float Duration = 0.0f) const override;

	UFUNCTION(Pure, Category="Navigation|Grid")
	int32 GetNavigationNodeCount() const override { return static_cast<int32>(WalkableCells.size()); }
	UFUNCTION(Pure, Category="Navigation|Grid")
	int32 GetBlockedNodeCount() const override { return BlockedCellCount; }
	UFUNCTION(Pure, Category="Navigation|Grid")
	float GetCellSize() const { return CellSize; }
	UFUNCTION(Pure, Category="Navigation|Grid")
	int32 GetLastBuildBoundsCount() const { return LastBuildBoundsCount; }
	UFUNCTION(Pure, Category="Navigation|Grid")
	int32 GetLastBuildCandidateCount() const { return LastBuildCandidateCount; }
	UFUNCTION(Pure, Category="Navigation|Grid")
	float GetLastBuildDurationMs() const { return LastBuildDurationMs; }
	UFUNCTION(Pure, Category="Navigation|Grid")
	FString GetLastBuildMessage() const { return LastBuildMessage; }
	UFUNCTION(Callable, Category="Navigation|Grid")
	void SetCellSize(float InCellSize);
	UFUNCTION(Pure, Category="Navigation|Grid")
	bool IsLocationOnNavData(const FVector& Location) const;

	void SetSupportedAgentForBuild(const FNavAgentProperties& InAgent);

	UPROPERTY(Edit, Save, Category="Navigation|Grid", DisplayName="Cell Size", Min=0.25f, Max=50.0f, Speed=0.05f)
	float CellSize = 0.75f;
	UPROPERTY(Edit, Save, Category="Navigation|Grid", DisplayName="Max Search Nodes", Min=16.0f, Max=131072.0f, Speed=64.0f)
	int32 MaxSearchNodes = 32768;
	UPROPERTY(Edit, Save, Category="Navigation|Grid", DisplayName="Projection Up", Min=0.0f, Max=100.0f, Speed=0.1f)
	float ProjectionUp = 5.0f;
	UPROPERTY(Edit, Save, Category="Navigation|Grid", DisplayName="Projection Down", Min=0.0f, Max=100.0f, Speed=0.1f)
	float ProjectionDown = 10.0f;
	UPROPERTY(Edit, Save, Category="Navigation|Grid", DisplayName="Direct Path Segment Length", Min=0.1f, Max=100.0f, Speed=0.05f)
	float DirectPathSegmentLength = 0.75f;
	UPROPERTY(Edit, Save, Category="Navigation|Grid", DisplayName="Walkable Floor Max Thickness", Min=0.01f, Max=100.0f, Speed=0.1f)
	float WalkableFloorMaxThickness = 1.0f;
	UPROPERTY(Edit, Save, Category="Navigation|Grid", DisplayName="Obstacle Padding", Min=0.0f, Max=10.0f, Speed=0.01f)
	float ObstaclePadding = 0.05f;
	UPROPERTY(Edit, Save, Category="Navigation|Grid", DisplayName="Physics Projection Fallback")
	bool bUsePhysicsProjectionFallback = false;
	UPROPERTY(Edit, Save, Category="Navigation|Grid", DisplayName="Nearest Cell Search Rings", Min=1.0f, Max=64.0f, Speed=1.0f)
	int32 NearestCellSearchRings = 12;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Draw On Build")
	bool bDrawDebugOnBuild = true;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Draw Blocked Cells")
	bool bDrawBlockedCells = true;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Draw Height Colors")
	bool bDrawHeightColors = true;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Draw Height Contours")
	bool bDrawHeightContours = true;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Height Contour Interval", Min=0.0f, Max=20.0f, Speed=0.01f)
	float DebugHeightContourInterval = 0.50f;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Debug Draw Duration", Min=0.0f, Max=60.0f, Speed=0.1f)
	float DebugDrawDuration = 30.0f;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Debug Draw Max Cells", Min=0.0f, Max=200000.0f, Speed=100.0f)
	int32 DebugDrawMaxCells = 20000;

private:
	struct FBuildBounds
	{
		FBoundingBox Bounds;
	};

	struct FNavigationPrimitiveBounds
	{
		FBoundingBox Bounds;
		bool bFloorCandidate = false;
		bool bObstacle = false;
	};

	struct FNavigationPrimitiveSpatialIndex
	{
		std::unordered_map<FGridNavCellKey, TArray<int32>, FGridNavCellKeyHash> FloorCandidatesByCell;
		std::unordered_map<FGridNavCellKey, TArray<int32>, FGridNavCellKeyHash> ObstacleCandidatesByCell;
		TArray<int32> EmptyCandidates;
		int32 FloorReferenceCount = 0;
		int32 ObstacleReferenceCount = 0;
	};

	struct FBuildCellResult
	{
		bool bWalkable = false;
		bool bBlockedByObstacle = false;
		FVector DebugLocation = FVector::ZeroVector;
		float ClearanceRadius = 0.0f;
	};

	UNavigationSystem* GetNavigationSystem() const;
	bool CollectBuildBounds(TArray<FBuildBounds>& OutBounds) const;
	void CollectNavigationPrimitives(TArray<FNavigationPrimitiveBounds>& OutPrimitives) const;
	void BuildNavigationPrimitiveSpatialIndex(const TArray<FNavigationPrimitiveBounds>& Primitives, const TArray<FBuildBounds>& BuildBounds, FNavigationPrimitiveSpatialIndex& OutIndex) const;
	FGridNavCellKey WorldToCell(const FVector& Point) const;
	FVector CellToWorldGuess(const FGridNavCellKey& Key, float Z) const;
	bool IsInsideAnyBuildBounds(const FVector& Point, const TArray<FBuildBounds>& Bounds) const;
	FBuildCellResult BuildCell(const FGridNavCellKey& Key, float GuessZ, const TArray<FBuildBounds>& Bounds, const TArray<FNavigationPrimitiveBounds>& Primitives, const FNavigationPrimitiveSpatialIndex& SpatialIndex, FGridNavCell& OutCell) const;
	bool ProjectPointToWalkableSurfaceByPrimitiveBounds(const FVector& Guess, const TArray<FBuildBounds>& Bounds, const TArray<FNavigationPrimitiveBounds>& Primitives, const TArray<int32>& CandidateIndices, FVector& OutProjectedLocation) const;
	bool HasAgentFootprint(const FVector& ProjectedLocation, const FNavAgentProperties& AgentProps, const TArray<FNavigationPrimitiveBounds>& Primitives, float* OutClearanceRadius = nullptr) const;
	bool HasAgentFootprintByPrimitiveBounds(const FVector& ProjectedLocation, const FNavAgentProperties& AgentProps, const TArray<FNavigationPrimitiveBounds>& Primitives, const TArray<int32>& CandidateIndices, float* OutClearanceRadius = nullptr) const;
	bool HasAgentFootprintByPrimitiveBounds(const FVector& ProjectedLocation, const FNavAgentProperties& AgentProps, const TArray<FNavigationPrimitiveBounds>& Primitives, float* OutClearanceRadius = nullptr) const;
	const FGridNavCell* FindCell(const FGridNavCellKey& Key) const;
	// 그리드 그래프와 각 셀의 ClearanceRadius 는 빌드 시 SupportedAgent 풋프린트로 검증됐다. 질의가
	// 빌드 에이전트보다 큰 풋프린트로 들어오면, 그리드가 보장한 적 없는 여유를 다시 요구해 walkable
	// 셀을 거부하고 → 경로가 있어도 FindPath 가 실패한다(예: 반경 1.8 보스 vs 1.2 로 빌드한 navmesh).
	// 그래서 질의 풋프린트(반경/높이)를 빌드 에이전트로 상한 클램프한다. 그리드가 빌드한 것 이상은
	// 약속하지 않는다 — 빌드보다 물리적으로 큰 몸체는 런타임 분리(Separation) 스티어링이 처리한다.
	float GetEffectiveAgentRadius(const FNavAgentProperties& AgentProps) const;
	float GetEffectiveAgentHeight(const FNavAgentProperties& AgentProps) const;
	bool IsCellUsableForAgent(const FGridNavCellKey& Key, const FNavAgentProperties& AgentProps, const FGridNavCell*& OutCell) const;
	bool FindCellAtPointStrict(const FVector& Point, const FNavAgentProperties& AgentProps, FGridNavCellKey& OutKey, FVector& OutLocation) const;
	bool FindNearestCell(const FVector& Point, FGridNavCellKey& OutKey, FVector& OutLocation) const;
	bool FindNearestCellForAgent(const FVector& Point, const FNavAgentProperties& AgentProps, FGridNavCellKey& OutKey, FVector& OutLocation) const;
	bool IsHeightTransitionAllowed(const FVector& From, const FVector& To, const FNavAgentProperties& AgentProps) const;
	bool HasCachedSegment(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const;
	FColor GetHeightDebugColor(float HeightZ, float MinZ, float MaxZ) const;
	bool ShouldDrawHeightContour(float HeightZ, float MinZ) const;
	bool BuildDirectCachedPath(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const;
	bool IsDiagonalMoveAllowed(const FGridNavCellKey& From, const FGridNavCellKey& Dir, const FNavAgentProperties& AgentProps) const;
	void SmoothPath(const FNavAgentProperties& AgentProps, FNavigationPath& InOutPath) const;
	// 빌드 후 walkable 셀을 A* 와 동일한 통행 규칙(인접성/대각 코너/경사 전이)으로 flood-fill 해
	// 연결 컴포넌트를 라벨링한다. 가장 큰 컴포넌트에 속하지 않는 셀 = "섬(island)" → IsolatedCells.
	// 경로가 특정 위치에서만 실패하는 건 거의 항상 그 위치가 섬에 갇혀 있기 때문이다.
	void ComputeConnectivity();

	std::unordered_map<FGridNavCellKey, FGridNavCell, FGridNavCellKeyHash> WalkableCells;
	std::unordered_map<FGridNavCellKey, FVector, FGridNavCellKeyHash> BlockedCells;
	std::unordered_set<FGridNavCellKey, FGridNavCellKeyHash> IsolatedCells;
	TArray<FNavigationPrimitiveBounds> CachedNavigationPrimitives;
	int32 BlockedCellCount = 0;
	int32 LastBuildBoundsCount = 0;
	int32 LastBuildCandidateCount = 0;
	int32 LastComponentCount = 0;
	int32 LastLargestComponentSize = 0;
	int32 LastIsolatedCellCount = 0;
	float LastBuildDurationMs = 0.0f;
	FString LastBuildMessage;
	FBoundingBox CachedBounds;
};
