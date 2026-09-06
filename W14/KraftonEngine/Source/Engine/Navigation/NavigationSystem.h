#pragma once

#include "Object/Object.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Core/Types/CollisionTypes.h"
#include "Navigation/NavTypes.h"

class AActor;
class ANavMeshBoundsVolume;
class ANavigationData;
class AGridNavMesh;
class UWorld;
struct FNavigationWorldSettings;

#include "Source/Engine/Navigation/NavigationSystem.generated.h"

// ============================================================
// UNavigationSystem — 월드 단위 네비게이션 서비스.
// Recast가 없는 현재 엔진에서 언리얼식 API를 먼저 고정하고,
// 내부 구현은 floor projection + grid A* + physics sweep으로 제공한다.
// ============================================================
UCLASS()
class UNavigationSystem : public UObject
{
public:
	GENERATED_BODY()
	UNavigationSystem() = default;
	~UNavigationSystem() override = default;

	UFUNCTION(Pure, Category="Navigation")
	static UNavigationSystem* GetCurrent(UWorld* World);

	UFUNCTION(Callable, Category="Navigation")
	bool RebuildNavigation();
	UFUNCTION(Callable, Category="Navigation")
	void InvalidateNavigationData();
	UFUNCTION(Callable, Category="Navigation")
	void RequestNavigationRebuild(const FString& Reason = "Navigation data changed", bool bImmediate = false);

	void Tick(float DeltaTime);
	void ApplyWorldSettings(const FNavigationWorldSettings& Settings);
	void ExportWorldSettings(FNavigationWorldSettings& OutSettings) const;

	UFUNCTION(Pure, Category="Navigation")
	ANavigationData* GetMainNavigationData() const;
	UFUNCTION(Pure, Category="Navigation")
	AGridNavMesh* GetGridNavMesh() const;
	UFUNCTION(Pure, Category="Navigation")
	bool HasBuiltNavigationData() const;
	UFUNCTION(Callable, Category="Navigation|Debug")
	void DebugDrawNavigationData(float Duration = 0.0f) const;
	UFUNCTION(Pure, Category="Navigation|Debug")
	FString GetLastQueryMessage() const { return LastQueryMessage; }
	UFUNCTION(Pure, Category="Navigation|Debug")
	FString GetDebugSummary() const;

	UFUNCTION(Callable, Category="Navigation")
	bool ProjectPointToNavigation(const FVector& Point, FVector& OutProjectedPoint, const FNavAgentProperties& AgentProps) const;

	UFUNCTION(Callable, Category="Navigation")
	bool FindPathSync(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const;

	UFUNCTION(Callable, Category="Navigation")
	bool NavigationRaycast(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const;

	UFUNCTION(Callable, Category="Navigation")
	bool GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FVector& OutPoint, const FNavAgentProperties& AgentProps) const;

	UFUNCTION(Pure, Category="Navigation")	
	bool HasNavigationBounds() const;
	UFUNCTION(Pure, Category="Navigation")
	bool IsPointInsideNavigationBounds(const FVector& Point) const;

	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Cell Size", Min=0.25f, Max=50.0f, Speed=0.05f)
	float CellSize = 0.75f;
	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Max Search Nodes", Min=16.0f, Max=131072.0f, Speed=64.0f)
	int32 MaxSearchNodes = 32768;
	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Agent Radius", Min=0.01f, Max=100.0f, Speed=0.01f)
	float AgentRadius = 0.42f;
	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Agent Height", Min=0.01f, Max=200.0f, Speed=0.05f)
	float AgentHeight = 2.10f;
	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Agent Step Height", Min=0.0f, Max=50.0f, Speed=0.01f)
	float AgentStepHeight = 0.60f;
	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Agent Max Climb Height", Min=0.0f, Max=50.0f, Speed=0.01f)
	float AgentMaxClimbHeight = 0.60f;
	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Agent Max Drop Height", Min=0.0f, Max=50.0f, Speed=0.01f)
	float AgentMaxDropHeight = 0.60f;
	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Agent Max Slope Degrees", Min=0.0f, Max=89.0f, Speed=1.0f)
	float AgentMaxSlopeDegrees = 50.0f;
	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Projection Up", Min=0.0f, Max=100.0f, Speed=0.1f)
	float ProjectionUp = 5.0f;
	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Projection Down", Min=0.0f, Max=100.0f, Speed=0.1f)
	float ProjectionDown = 10.0f;
	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Direct Path Segment Length", Min=0.1f, Max=100.0f, Speed=0.05f)
	float DirectPathSegmentLength = 0.75f;
	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Obstacle Padding", Min=0.0f, Max=10.0f, Speed=0.01f)
	float ObstaclePadding = 0.05f;
	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Physics Projection Fallback")
	bool bUsePhysicsProjectionFallback = false;
	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Use Built Navigation Data")
	bool bUseNavigationData = true;
	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Auto Rebuild On Path Request")
	bool bAutoRebuildOnPathRequest = true;
	UPROPERTY(Edit, Save, Category="Navigation", DisplayName="Allow Runtime Fallback")
	bool bAllowRuntimeFallback = false;
	UPROPERTY(Edit, Save, Category="Navigation|Runtime", DisplayName="Enable Runtime Auto Rebuild")
	bool bEnableRuntimeAutoRebuild = true;
	UPROPERTY(Edit, Save, Category="Navigation|Runtime", DisplayName="Runtime Rebuild Delay", Min=0.0f, Max=5.0f, Speed=0.01f)
	float RuntimeRebuildDelay = 0.35f;
	UPROPERTY(Edit, Save, Category="Navigation|Runtime", DisplayName="Runtime Rebuild Min Interval", Min=0.0f, Max=10.0f, Speed=0.01f)
	float RuntimeRebuildMinInterval = 0.50f;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Draw Navigation On Build")
	bool bDrawDebugOnBuild = true;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Draw Blocked Cells")
	bool bDrawBlockedCells = true;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Draw Height Colors")
	bool bDrawHeightColors = true;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Draw Height Contours")
	bool bDrawHeightContours = true;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Height Contour Interval", Min=0.0f, Max=20.0f, Speed=0.01f)
	float DebugHeightContourInterval = 0.50f;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Navigation Debug Draw Duration", Min=0.0f, Max=60.0f, Speed=0.1f)
	float DebugDrawDuration = 30.0f;
	UPROPERTY(Edit, Save, Category="Navigation|Debug", DisplayName="Debug Draw Max Cells", Min=0.0f, Max=200000.0f, Speed=100.0f)
	int32 DebugDrawMaxCells = 20000;

	// Raw runtime helpers used by NavigationData builders. These do not query cached
	// NavigationData again, so they are safe to call while building a nav graph.
	bool ProjectPointToNavigationRuntime(const FVector& Point, FVector& OutProjectedPoint, const FNavAgentProperties& AgentProps, float InProjectionUp = -1.0f, float InProjectionDown = -1.0f) const;
	bool CanTraverseSegmentRuntime(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const;
	bool IsWalkableFloorHitRuntime(const FHitResult& Hit, const FNavAgentProperties& AgentProps) const;

private:
	UWorld* GetOwningWorld() const;
	bool IsPointAllowedByBounds(const FVector& Point) const;
	bool IsWalkableFloorHit(const FHitResult& Hit, const FNavAgentProperties& AgentProps) const;
	bool HasBlockingObstacleBetween(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const;
	bool CanTraverseSegment(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const;
	bool BuildDirectPathIfReachable(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const;
	bool FindGridPath(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const;
	void SmoothPath(const FNavAgentProperties& AgentProps, FNavigationPath& InOutPath) const;
	ANavigationData* FindNavigationDataActor() const;
	AGridNavMesh* EnsureGridNavMeshActor();
	void PushSettingsToGridNavMesh(AGridNavMesh& Grid) const;
	bool SetLastQueryMessage(const FString& Message) const { LastQueryMessage = Message; return false; }

	mutable TWeakObjectPtr<ANavigationData> CachedMainNavData = nullptr;
	mutable FString LastQueryMessage;
	bool bRebuildPending = false;
	float PendingRebuildAge = 0.0f;
	float TimeSinceLastRebuild = 1000000.0f;
	FString PendingRebuildReason;
};
