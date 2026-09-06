#pragma once
#include "Object/Object.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Collision/WorldPrimitivePickingBVH.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "Component/CameraComponent.h"
#include "GameFramework/WorldContext.h"
#include "Render/Scene/FScene.h"
#include "Render/Types/LODContext.h"
#include <cstddef>
#include <Collision/Octree.h>
#include <Collision/SpatialPartition.h>
#include "Serialization/Archive.h"
#include <unordered_set>

class UCameraComponent;
class UPrimitiveComponent;
class AGameModeBase;
class ULevel;

class FActorIterator
{
public:
	FActorIterator(const TArray<ULevel*>& InLevels, int32 InLevelIndex, int32 InActorIndex)
		: Levels(InLevels), LevelIndex(InLevelIndex), ActorIndex(InActorIndex)
	{
		AdvanceToValid();
	}

	FActorIterator& operator++()
	{
		++ActorIndex;
		AdvanceToValid();
		return *this;
	}

	bool operator!=(const FActorIterator& Other) const
	{
		return LevelIndex != Other.LevelIndex || ActorIndex != Other.ActorIndex;
	}

	bool operator==(const FActorIterator& Other) const
	{
		return LevelIndex == Other.LevelIndex && ActorIndex == Other.ActorIndex;
	}

	AActor* operator*() const
	{
		return Levels[LevelIndex]->GetActors()[ActorIndex];
	}

private:
	void AdvanceToValid()
	{
		while (LevelIndex < Levels.size())
		{
			if (Levels[LevelIndex] && ActorIndex < Levels[LevelIndex]->GetActors().size())
			{
				return;
			}
			++LevelIndex;
			ActorIndex = 0;
		}
	}

	const TArray<ULevel*>& Levels;
	int32 LevelIndex;
	int32 ActorIndex;
};

class FActorRange
{
public:
	FActorRange(const TArray<ULevel*>& InLevels) : Levels(InLevels) {}

	FActorIterator begin() const { return FActorIterator(Levels, 0, 0); }
	FActorIterator end() const { return FActorIterator(Levels, static_cast<int32>(Levels.size()), 0); }

	bool IsEmpty() const
	{
		return begin() == end();
	}

	TArray<AActor*> ToArray() const
	{
		TArray<AActor*> Arr;
		for (AActor* Actor : *this)
		{
			Arr.push_back(Actor);
		}
		return Arr;
	}

private:
	const TArray<ULevel*>& Levels;
};

struct FStreamingLevelInfo
{
	FString LevelPath;
	FName LevelName;
	bool bIsLoaded = false;
	bool bShouldBeVisible = true;
	ULevel* LoadedLevel = nullptr;
};

class UWorld : public UObject {
public:
	DECLARE_CLASS(UWorld, UObject)
	UWorld() = default;
	~UWorld() override;

	// --- 월드 타입 ---
	EWorldType GetWorldType() const { return WorldType; }
	void SetWorldType(EWorldType InType) { WorldType = InType; }

	// 월드 복제 — 자체 Actor 리스트를 순회하며 각 Actor를 새 World로 Duplicate.
	// UObject::Duplicate는 Serialize 왕복만 수행하므로 UWorld처럼 컨테이너 기반 상태가 있는
	// 타입은 별도 오버라이드가 필요하다.
	UObject* Duplicate(UObject* NewOuter = nullptr) const override;

	// 지정된 WorldType으로 복제 — Actor 복제 전에 WorldType이 설정되므로
	// EditorOnly 컴포넌트의 CreateRenderState()에서 올바르게 판별 가능.
	UWorld* DuplicateAs(EWorldType InWorldType) const;

	void Serialize(FArchive& Ar) override;

	// 레벨 관리
	void AddLevel(ULevel* InLevel);
	void RemoveLevel(ULevel* InLevel);
	void SetCurrentLevel(ULevel* InLevel) { CurrentLevel = InLevel; }
	ULevel* GetCurrentLevel() const { return CurrentLevel; }
	const TArray<ULevel*>& GetLevels() const { return Levels; }
	void ClearLevels();

	// 스트리밍 레벨
	void AddStreamingLevel(const FString& LevelPath);
	void LoadStreamingLevel(const FString& LevelPath);
	void UnloadStreamingLevel(const FName& LevelName);
	const TArray<FStreamingLevelInfo>& GetStreamingLevels() const { return StreamingLevels; }

	// Actor lifecycle
	template<typename T>
	T* SpawnActor();
	void DestroyActor(AActor* Actor);
	void AddActor(AActor* Actor);
	bool MoveActorBefore(AActor* ActorToMove, AActor* BeforeActor);
	bool MoveActorToIndex(AActor* ActorToMove, size_t TargetIndex);
	void MarkWorldPrimitivePickingBVHDirty();
	void BuildWorldPrimitivePickingBVHNow() const;
	void BeginDeferredPickingBVHUpdate();
	void EndDeferredPickingBVHUpdate();
	void WarmupPickingData() const;
	bool RaycastPrimitives(const FRay& Ray, FRayHitResult& OutHitResult, AActor*& OutActor) const;
	void CollectWorldPrimitivePickingBVHDebugBounds(TArray<FBoundingBox>& OutBounds) const;
	bool GetPartitionRootBounds(FBoundingBox& OutBounds) const;

	FActorRange GetActors() const { return FActorRange(Levels); }

	ULevel* GetPersistentLevel() const { return PersistentLevel; }
	AGameModeBase* GetAuthorGameMode() const { return AuthorGameMode; }

	// LOD 컨텍스트를 FFrameContext에 전달 (Collect 단계에서 LOD 인라인 갱신용)
	FLODUpdateContext PrepareLODContext();

	void InitWorld();      // Set up the world before gameplay begins
	void BeginPlay();      // Triggers BeginPlay on all actors
	void Tick(float InRawDeltaTime, ELevelTick TickType);  // Drives the game loop every frame
	void EndPlay();        // Cleanup before world is destroyed

	bool HasBegunPlay() const { return bHasBegunPlay; }

	// GameMode — BeginPlay 시점에 PersistentLevel/ProjectSettings의 클래스명으로 스폰됨
	AGameModeBase* GetAuthGameMode() const { return AuthorGameMode; }
	void SpawnGameMode();

	// Active Camera — EditorViewportClient 또는 PlayerController가 세팅
	void SetActiveCamera(UCameraComponent* InCamera) { ActiveCamera = InCamera; }
	UCameraComponent* GetActiveCamera() const { return ActiveCamera; }

	// FScene — 렌더 프록시 관리자
	FScene& GetScene() { return Scene; }
	const FScene& GetScene() const { return Scene; }
	
	FSpatialPartition& GetPartition() { return Partition; }
	const FOctree* GetOctree() const { return Partition.GetOctree(); }
	void InsertActorToOctree(AActor* actor);
	void RemoveActorToOctree(AActor* actor);
	void UpdateActorInOctree(AActor* actor);

	// Adds a primitive component to the pending overlap update array
	void AddPendingOverlapComponent(UPrimitiveComponent* InComp);
	void RemovePendingOverlapComponent(UPrimitiveComponent* InComp);

	// Hit Time-System
	// 전역 시간 배율 조절
	float GetGlobalTimeDilation() const { return GlobalTimeDilation; }

	void SetGlobalTimeDilation(float InTimeDilation);

	float GetRawDeltaTime() const { return RawDeltaTime; }
	float GetDeltaTime() const { return DeltaTime; }

private:
	// Overlaps
	void ProcessOverlapEvents();
	void ResolvePenetration(UPrimitiveComponent* A, UPrimitiveComponent* B, const FHitResult& Hit);

private:
	ULevel* PersistentLevel = nullptr;
	TArray<ULevel*> Levels;
	TArray<FStreamingLevelInfo> StreamingLevels;
	ULevel* CurrentLevel = nullptr;
	AGameModeBase* AuthorGameMode = nullptr;

	UCameraComponent* ActiveCamera = nullptr;
	UCameraComponent* LastLODUpdateCamera = nullptr;
	EWorldType WorldType = EWorldType::Editor;
	bool bHasBegunPlay = false;
	bool bHasLastFullLODUpdateCameraPos = false;
	mutable FWorldPrimitivePickingBVH WorldPrimitivePickingBVH;
	int32 DeferredPickingBVHUpdateDepth = 0;
	bool bDeferredPickingBVHDirty = false;
	uint32 VisibleProxyBuildFrame = 0;
	FVector LastFullLODUpdateCameraForward = FVector(1, 0, 0);
	FVector LastFullLODUpdateCameraPos = FVector(0, 0, 0);
	FScene Scene;
	FTickManager TickManager;
	FSpatialPartition Partition;

	// Hit-stop, slomo 목적
	float RawDeltaTime = 0.f;					// 프레임 시간 
	float DeltaTime = 0.f;						// 게임 시간

	float GlobalTimeDilation = 1.0f;			// 전역 시간 배율
	const float MinGlobalTimeDilation = 0.0f;	// 최소
	const float MaxGlobalTimeDilation = 20.0f;	// 최대

	std::unordered_set<UPrimitiveComponent*> PendingOverlapComponents;
};

template<typename T>
inline T* UWorld::SpawnActor()
{
	// create and register an actor
	T* Actor = UObjectManager::Get().CreateObject<T>(CurrentLevel);
	AddActor(Actor); // BeginPlay 트리거는 AddActor 내부에서 bHasBegunPlay 가드로 처리
	return Actor;
}
