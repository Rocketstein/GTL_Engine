#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Collision/CollisionDispatcher.h"
#include "Component/Shape/ShapeComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Component/CameraComponent.h"
#include "Render/Types/LODContext.h"
#include <algorithm>
#include <filesystem>
#include "Profiling/Stats.h"
#include "GameFramework/GameModeBase.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"
#include "Core/ProjectSettings.h"
#include "Collision/RayUtils.h"
IMPLEMENT_CLASS(UWorld, UObject)

namespace
{
	bool RaycastPrimitivesFallback(const TArray<ULevel*>& Levels, const FRay& Ray, FRayHitResult& OutHitResult, AActor*& OutActor)
	{
		FRayHitResult BestHit{};
		AActor* BestActor = nullptr;

		for (ULevel* Level : Levels)
		{
			if (!Level)
			{
				continue;
			}

			for (AActor* Actor : Level->GetActors())
			{
				if (!Actor || !Actor->IsVisible())
				{
					continue;
				}

				for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
				{
					if (!Primitive || !Primitive->IsVisible() || !Primitive->ParticipatesInPickingSpatialStructure())
					{
						continue;
					}

					FRayHitResult CandidateHit{};
					if (!Primitive->LineTraceComponent(Ray, CandidateHit))
					{
						float AABBTMin = 0.0f;
						float AABBTMax = 0.0f;
						const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
						if (!FRayUtils::IntersectRayAABB(Ray, Bounds.Min, Bounds.Max, AABBTMin, AABBTMax))
						{
							continue;
						}

						CandidateHit.HitComponent = Primitive;
						CandidateHit.Distance = AABBTMin >= 0.0f ? AABBTMin : AABBTMax;
						CandidateHit.WorldHitLocation = Ray.Origin + Ray.Direction * CandidateHit.Distance;
						CandidateHit.bHit = true;
					}

					if (CandidateHit.Distance < BestHit.Distance)
					{
						BestHit = CandidateHit;
						BestActor = Actor;
					}
				}
			}
		}

		if (!BestActor)
		{
			return false;
		}

		OutHitResult = BestHit;
		OutActor = BestActor;
		return true;
	}
}


UWorld::~UWorld()
{
	EndPlay();
}

UObject* UWorld::Duplicate(UObject* NewOuter) const
{
	UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
	if (!NewWorld)
	{
		return nullptr;
	}
	NewWorld->SetOuter(NewOuter);
	NewWorld->InitWorld();
	if (PersistentLevel && NewWorld->GetPersistentLevel())
	{
		NewWorld->GetPersistentLevel()->SetOutlinerFolders(PersistentLevel->GetOutlinerFolders());
	}

	if (PersistentLevel)
	{
		NewWorld->ClearLevels();
		NewWorld->PersistentLevel = Cast<ULevel>(PersistentLevel->Duplicate(NewWorld));
		if (NewWorld->PersistentLevel)
		{
			NewWorld->PersistentLevel->SetWorld(NewWorld);
			NewWorld->Levels.push_back(NewWorld->PersistentLevel);
			NewWorld->CurrentLevel = NewWorld->PersistentLevel;

			for (AActor* Actor : NewWorld->PersistentLevel->GetActors())
			{
				if (Actor)
				{
					NewWorld->InsertActorToOctree(Actor);
				}
			}
			NewWorld->MarkWorldPrimitivePickingBVHDirty();
		}
	}

	NewWorld->StreamingLevels = StreamingLevels;

	NewWorld->PostDuplicate();
	return NewWorld;
}

UWorld* UWorld::DuplicateAs(EWorldType InWorldType) const
{
	UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
	if (!NewWorld) return nullptr;

	NewWorld->SetWorldType(InWorldType);
	NewWorld->InitWorld();
	if (PersistentLevel && NewWorld->GetPersistentLevel())
	{
		NewWorld->GetPersistentLevel()->SetOutlinerFolders(PersistentLevel->GetOutlinerFolders());
	}

	// TODO: 임시 처리. 추후 제거 하고 Level Duplicate 하는 과정 추가해야할듯
	NewWorld->PersistentLevel->SetGameModeClassName(PersistentLevel->GetGameModeClassName());

	if (PersistentLevel)
	{
		NewWorld->ClearLevels();
		NewWorld->PersistentLevel = Cast<ULevel>(PersistentLevel->Duplicate(NewWorld));
		if (NewWorld->PersistentLevel)
		{
			NewWorld->PersistentLevel->SetWorld(NewWorld);
			NewWorld->Levels.push_back(NewWorld->PersistentLevel);
			NewWorld->CurrentLevel = NewWorld->PersistentLevel;

			for (AActor* Actor : NewWorld->PersistentLevel->GetActors())
			{
				if (Actor)
				{
					NewWorld->InsertActorToOctree(Actor);
				}
			}
			NewWorld->MarkWorldPrimitivePickingBVHDirty();
		}
	}

	NewWorld->StreamingLevels = StreamingLevels;

	NewWorld->PostDuplicate();
	return NewWorld;
}

void UWorld::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	if (Ar.IsSaving())
	{
		//  Serialize Persistent Level Data (Only persistent level data is stored in this file)
		if (PersistentLevel)
		{
			PersistentLevel->Serialize(Ar);
		}

		// Serialize Streaming Levels Metadata (References to other files)
		int32 StreamingCount = static_cast<int32>(StreamingLevels.size());
		Ar << StreamingCount;
		for (auto& Info : StreamingLevels)
		{
			Ar << Info.LevelPath;
			Ar << Info.LevelName;
			Ar << Info.bShouldBeVisible;
		}
	}
	else if (Ar.IsLoading())
	{
		ClearLevels();
		StreamingLevels.clear();

		// Deserialize Persistent Level Data
		PersistentLevel = UObjectManager::Get().CreateObject<ULevel>(this);
		PersistentLevel->SetWorld(this);
		PersistentLevel->Serialize(Ar);
		Levels.push_back(PersistentLevel);
		CurrentLevel = PersistentLevel;

		for (AActor* Actor : PersistentLevel->GetActors())
		{
			if (Actor)
			{
				InsertActorToOctree(Actor);
			}
		}
		MarkWorldPrimitivePickingBVHDirty();

		// Deserialize Streaming Levels Metadata
		int32 StreamingCount = 0;
		Ar << StreamingCount;
		for (int32 i = 0; i < StreamingCount; ++i)
		{
			FStreamingLevelInfo Info;
			Ar << Info.LevelPath;
			Ar << Info.LevelName;
			Ar << Info.bShouldBeVisible;
			StreamingLevels.push_back(Info);
		}

		// Auto-load streaming levels 
		for (auto& Info : StreamingLevels)
		{
			LoadStreamingLevel(Info.LevelPath);
		}
	}
}

void UWorld::AddStreamingLevel(const FString& LevelPath)
{
	for (const auto& Info : StreamingLevels)
	{
		if (Info.LevelPath == LevelPath) return;
	}

	FStreamingLevelInfo NewInfo;
	NewInfo.LevelPath = LevelPath;
	NewInfo.LevelName = FName(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(LevelPath)).stem().wstring()));
	StreamingLevels.push_back(NewInfo);
	
	LoadStreamingLevel(LevelPath);
}

void UWorld::LoadStreamingLevel(const FString& LevelPath)
{
	FStreamingLevelInfo* TargetInfo = nullptr;
	for (auto& Info : StreamingLevels)
	{
		if (Info.LevelPath == LevelPath)
		{
			TargetInfo = &Info;
			break;
		}
	}

	if (!TargetInfo || TargetInfo->bIsLoaded) return;

	// Load Level from separate file
	ULevel* NewLevel = UObjectManager::Get().CreateObject<ULevel>(this);
	NewLevel->SetWorld(this);

	FWindowsBinReader Ar(LevelPath);
	if (Ar.IsValid())
	{
		NewLevel->Serialize(Ar);
		AddLevel(NewLevel);
		TargetInfo->LoadedLevel = NewLevel;
		TargetInfo->bIsLoaded = true;

		if (bHasBegunPlay) NewLevel->BeginPlay();
	}
	else
	{
		UObjectManager::Get().DestroyObject(NewLevel);
	}
}

void UWorld::UnloadStreamingLevel(const FName& LevelName)
{
	for (auto& Info : StreamingLevels)
	{
		if (Info.LevelName == LevelName && Info.bIsLoaded)
		{
			if (Info.LoadedLevel)
			{
				Info.LoadedLevel->EndPlay();
				RemoveLevel(Info.LoadedLevel);
				UObjectManager::Get().DestroyObject(Info.LoadedLevel);
				Info.LoadedLevel = nullptr;
			}
			Info.bIsLoaded = false;
			return;
		}
	}
}

void UWorld::AddLevel(ULevel* InLevel)
{
	if (!InLevel) return;
	if (std::find(Levels.begin(), Levels.end(), InLevel) == Levels.end())
	{
		InLevel->SetWorld(this);
		Levels.push_back(InLevel);
		if (!PersistentLevel) PersistentLevel = InLevel;
	}
}

void UWorld::RemoveLevel(ULevel* InLevel)
{
	if (!InLevel) return;
	auto it = std::find(Levels.begin(), Levels.end(), InLevel);
	if (it != Levels.end())
	{
		if (CurrentLevel == InLevel) CurrentLevel = (Levels.size() > 1) ? (Levels[0] == InLevel ? Levels[1] : Levels[0]) : nullptr;
		if (PersistentLevel == InLevel) PersistentLevel = (Levels.size() > 1) ? (Levels[0] == InLevel ? Levels[1] : Levels[0]) : nullptr;
		Levels.erase(it);
	}
}

void UWorld::ClearLevels()
{
	// Scene 전환, World 정리 후 Slomo, DeltaTime 남는 문제 방지
	RawDeltaTime = 0.f;
	DeltaTime = 0.f;
	SetGlobalTimeDilation(1.0f);

	TickManager.Reset();
	AuthorGameMode = nullptr;
	ActiveCamera = nullptr;
	LastLODUpdateCamera = nullptr;
	bHasLastFullLODUpdateCameraPos = false;

	for (ULevel* Level : Levels)
	{
		if (Level)
		{
			Level->EndPlay();
			UObjectManager::Get().DestroyObject(Level);
		}
	}
	Levels.clear();
	PersistentLevel = nullptr;
	CurrentLevel = nullptr;
	PendingOverlapComponents.clear();
}

void UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor) return;
	Actor->EndPlay();
	
	ULevel* OwningLevel = Cast<ULevel>(Actor->GetOuter());
	if (OwningLevel)
	{
		OwningLevel->RemoveActor(Actor);
	}

	MarkWorldPrimitivePickingBVHDirty();
	Partition.RemoveActor(Actor);

	UObjectManager::Get().DestroyObject(Actor);
}

void UWorld::AddActor(AActor* Actor)
{
	if (!Actor || !CurrentLevel)
	{
		return;
	}

	CurrentLevel->AddActor(Actor);

	InsertActorToOctree(Actor);
	MarkWorldPrimitivePickingBVHDirty();

	if (bHasBegunPlay && !Actor->HasActorBegunPlay())
	{
		Actor->BeginPlay();
	}
}

bool UWorld::MoveActorBefore(AActor* ActorToMove, AActor* BeforeActor)
{
	if (!PersistentLevel)
	{
		return false;
	}

	const bool bMoved = PersistentLevel->MoveActorBefore(ActorToMove, BeforeActor);
	if (bMoved)
	{
		MarkWorldPrimitivePickingBVHDirty();
	}
	return bMoved;
}

bool UWorld::MoveActorToIndex(AActor* ActorToMove, size_t TargetIndex)
{
	if (!PersistentLevel)
	{
		return false;
	}

	const bool bMoved = PersistentLevel->MoveActorToIndex(ActorToMove, TargetIndex);
	if (bMoved)
	{
		MarkWorldPrimitivePickingBVHDirty();
	}
	return bMoved;
}

void UWorld::MarkWorldPrimitivePickingBVHDirty()
{
	if (DeferredPickingBVHUpdateDepth > 0)
	{
		bDeferredPickingBVHDirty = true;
		return;
	}

	WorldPrimitivePickingBVH.MarkDirty();
}

void UWorld::BuildWorldPrimitivePickingBVHNow() const
{
	WorldPrimitivePickingBVH.BuildNow(GetActors().ToArray());
}

void UWorld::BeginDeferredPickingBVHUpdate()
{
	++DeferredPickingBVHUpdateDepth;
}

void UWorld::EndDeferredPickingBVHUpdate()
{
	if (DeferredPickingBVHUpdateDepth <= 0)
	{
		return;
	}

	--DeferredPickingBVHUpdateDepth;
	if (DeferredPickingBVHUpdateDepth == 0 && bDeferredPickingBVHDirty)
	{
		bDeferredPickingBVHDirty = false;
		BuildWorldPrimitivePickingBVHNow();
	}
}

void UWorld::WarmupPickingData() const
{
	for (AActor* Actor : GetActors())
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!Primitive || !Primitive->IsVisible() || !Primitive->IsA<UStaticMeshComponent>())
			{
				continue;
			}

			UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>(Primitive);
			if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
			{
				StaticMesh->EnsureMeshTrianglePickingBVHBuilt();
			}
		}
	}

	BuildWorldPrimitivePickingBVHNow();
}

bool UWorld::RaycastPrimitives(const FRay& Ray, FRayHitResult& OutHitResult, AActor*& OutActor) const
{
	WorldPrimitivePickingBVH.EnsureBuilt(GetActors().ToArray());
	if (WorldPrimitivePickingBVH.Raycast(Ray, OutHitResult, OutActor))
	{
		return true;
	}

	// BVH broad-phase가 stale bounds나 특수 primitive 때문에 miss하는 경우를 위해
	// 클릭 입력에서는 느리지만 정확한 직접 교차 검사로 한 번 더 확인한다.
	return RaycastPrimitivesFallback(Levels, Ray, OutHitResult, OutActor);
}

void UWorld::CollectWorldPrimitivePickingBVHDebugBounds(TArray<FBoundingBox>& OutBounds) const
{
	WorldPrimitivePickingBVH.EnsureBuilt(GetActors().ToArray());
	WorldPrimitivePickingBVH.CollectDebugBounds(OutBounds);
}

bool UWorld::GetPartitionRootBounds(FBoundingBox& OutBounds) const
{
	return Partition.GetRootBounds(OutBounds);
}
void UWorld::InsertActorToOctree(AActor* Actor)
{
	Partition.InsertActor(Actor);
}

void UWorld::RemoveActorToOctree(AActor* Actor)
{
	Partition.RemoveActor(Actor);
}

void UWorld::UpdateActorInOctree(AActor* Actor)
{
	Partition.UpdateActor(Actor);
}

FLODUpdateContext UWorld::PrepareLODContext()
{
	if (!ActiveCamera) return {};

	const FVector CameraPos = ActiveCamera->GetWorldLocation();
	const FVector CameraForward = ActiveCamera->GetForwardVector();

	const uint32 LODUpdateFrame = VisibleProxyBuildFrame++;
	const uint32 LODUpdateSlice = LODUpdateFrame & (LOD_UPDATE_SLICE_COUNT - 1);
	const bool bShouldStaggerLOD = Scene.GetProxyCount() >= LOD_STAGGER_MIN_VISIBLE;

	const bool bForceFullLODRefresh =
		!bShouldStaggerLOD
		|| LastLODUpdateCamera != ActiveCamera
		|| !bHasLastFullLODUpdateCameraPos
		|| FVector::DistSquared(CameraPos, LastFullLODUpdateCameraPos) >= LOD_FULL_UPDATE_CAMERA_MOVE_SQ
		|| CameraForward.Dot(LastFullLODUpdateCameraForward) < LOD_FULL_UPDATE_CAMERA_ROTATION_DOT;

	if (bForceFullLODRefresh)
	{
		LastLODUpdateCamera = ActiveCamera;
		LastFullLODUpdateCameraPos = CameraPos;
		LastFullLODUpdateCameraForward = CameraForward;
		bHasLastFullLODUpdateCameraPos = true;
	}

	FLODUpdateContext Ctx;
	Ctx.CameraPos = CameraPos;
	Ctx.LODUpdateFrame = LODUpdateFrame;
	Ctx.LODUpdateSlice = LODUpdateSlice;
	Ctx.bForceFullRefresh = bForceFullLODRefresh;
	Ctx.bValid = true;
	return Ctx;
}

void UWorld::InitWorld()
{
	Partition.Reset(FBoundingBox());
	AuthorGameMode = nullptr;
	ActiveCamera = nullptr;
	LastLODUpdateCamera = nullptr;
	bHasLastFullLODUpdateCameraPos = false;
	ClearLevels();

	PersistentLevel = UObjectManager::Get().CreateObject<ULevel>(this);
	PersistentLevel->SetWorld(this);
	Levels.push_back(PersistentLevel);
	CurrentLevel = PersistentLevel;

	// GameMode는 BeginPlay 시점에 스폰 — Editor 월드는 GameMode 인스턴스 없이 둔다.
}

void UWorld::SpawnGameMode()
{
	if (AuthorGameMode) return;

	// 1) 레벨이 명시한 GameMode 클래스 우선
	FString ClassName;
	if (PersistentLevel)
	{
		ClassName = PersistentLevel->GetGameModeClassName();
	}
	// 2) 없으면 ProjectSettings의 기본값 사용
	if (ClassName.empty())
	{
		ClassName = FProjectSettings::Get().Game.DefaultGameModeClass;
	}
	if (ClassName.empty())
	{
		ClassName = "AGameModeBase";
	}
	if (ClassName == "None" || ClassName == "none" || ClassName == "NoneGameMode")
	{
		return;
	}

	UObject* Obj = FObjectFactory::Get().Create(ClassName, CurrentLevel);
	AGameModeBase* GameMode = Cast<AGameModeBase>(Obj);
	if (!GameMode)
	{
		// 폴백 — 잘못된 클래스명이거나 등록 안된 경우
		if (Obj) UObjectManager::Get().DestroyObject(Obj);
		GameMode = SpawnActor<AGameModeBase>();
	}
	else
	{
		AddActor(GameMode);
	}
	AuthorGameMode = GameMode;
}

void UWorld::BeginPlay()
{
	if (bHasBegunPlay) return;
	bHasBegunPlay = true;

	SpawnGameMode();

	for (ULevel* Level : Levels)
	{
		if (Level) Level->BeginPlay();
	}
	if (AuthorGameMode)
		AuthorGameMode->StartPlay();
}

void UWorld::Tick(float InRawDeltaTime, ELevelTick TickType)
{
	// Global Time Dilation 조절
	RawDeltaTime = std::max(0.0f, InRawDeltaTime);

	if (TickType == LEVELTICK_PauseTick)
	{
		DeltaTime = 0.f;
	}
	else
	{
		DeltaTime = RawDeltaTime * GlobalTimeDilation;
	}

	{
		SCOPE_STAT_CAT("FlushPrimitive", "1_WorldTick");
		Partition.FlushPrimitive();
	}

	Scene.GetDebugDrawQueue().Tick(DeltaTime);
	TickManager.Tick(this, DeltaTime, TickType);
	ProcessOverlapEvents();
}

void UWorld::AddPendingOverlapComponent(UPrimitiveComponent* InComp) {
	if (InComp) PendingOverlapComponents.insert(InComp);
}

void UWorld::RemovePendingOverlapComponent(UPrimitiveComponent* InComp) {
	if (InComp) PendingOverlapComponents.erase(InComp);
}

void UWorld::SetGlobalTimeDilation(float InTimeDilation)
{
	GlobalTimeDilation = std::clamp(InTimeDilation, MinGlobalTimeDilation, MaxGlobalTimeDilation);
}

void UWorld::ProcessOverlapEvents() {
	const FOctree* Octree = GetOctree();
	if (!Octree) return;

	TArray<UPrimitiveComponent*> Batch(PendingOverlapComponents.begin(), PendingOverlapComponents.end());
	PendingOverlapComponents.clear();

	for (auto* Component : Batch) {
		if (!Component) continue;
		UShapeComponent* Shape = dynamic_cast<UShapeComponent*>(Component);
		if (!Shape || !Shape->IsCollisionEnabled() || Shape->GetOverlapBehaviour() == EOverlapBehaviour::Ignore) continue;

		// End overlaps that are no longer valid
		TArray<FOverlapInfo> Prev = Shape->GetOverlapInfos();
		for (const FOverlapInfo& PrevInfo : Prev) {
			UShapeComponent* Other = dynamic_cast<UShapeComponent*>(PrevInfo.HitResult.Component);
			if (!Other || !FCollisionDispatcher::Get().CheckCollision(Shape, Other)) {
				Shape->EndComponentOverlap(PrevInfo.HitResult.Component);
				Shape->ShapeColor = FColor(0, 0, 255);
			}

			if (Other) {
				Other->MarkUpdateOverlaps();
			}
		}

		// Broad phase
		TArray<UPrimitiveComponent*> Broad;
		Octree->QueryAABB(Shape->GetWorldBoundingBox(), Broad);

		// Narrow phase
		for (auto* Candidate : Broad) {
			if (!Candidate || Candidate == Shape) continue;
			UShapeComponent* Other = dynamic_cast<UShapeComponent*>(Candidate);
			if (!Other || !Other->IsCollisionEnabled()) continue;
			if (Shape->GetOverlapBehaviour() == EOverlapBehaviour::Hit && Other->GetOverlapBehaviour() == EOverlapBehaviour::Hit) {
				// Hit
				FOverlapInfo Info;
				Info.HitResult.bBlocking = true;
				Info.HitResult.Component = Other;
				if (FCollisionDispatcher::Get().CheckCollision(Shape, Other, Info)) {
					Shape->BeginComponentOverlap(Info, true);
					Shape->ShapeColor = FColor(0, 255, 0);
				}
				ResolvePenetration(Shape, Other, Info.HitResult);
				Other->MarkUpdateOverlaps();
			}
			// 한쪽이라도 Overlap이면 overlap pair를 만듭니다.
			// 예: ItemTrigger=Overlap, PlayerCollider=Hit 조합에서도 item 쪽 Lua OnBeginOverlap이 반드시 호출되어야 합니다.
			else if (Shape->GetOverlapBehaviour() == EOverlapBehaviour::Overlap ||
				Other->GetOverlapBehaviour() == EOverlapBehaviour::Overlap) {
				// Overlap
				FOverlapInfo Info;
				Info.HitResult.bBlocking = false;
				Info.HitResult.Component = Other;
				if (FCollisionDispatcher::Get().CheckCollision(Shape, Other, Info)) {
					Shape->BeginComponentOverlap(Info, true);
					Shape->ShapeColor = FColor(255, 0, 0);

					// 기존 코드는 Shape 기준으로만 이벤트를 보냈습니다.
					// reverse overlap을 함께 등록해야 Other owner의 ScriptComponent도 selfComp=Other 기준으로 begin/end pair를 유지할 수 있습니다.
					FOverlapInfo ReverseInfo = Info;
					ReverseInfo.HitResult.Component = Shape;
					Other->BeginComponentOverlap(ReverseInfo, true);
				}
				Other->MarkUpdateOverlaps();
			}
		}
	}
}

// Separate Components on Hit if they are both set to Block
void UWorld::ResolvePenetration(UPrimitiveComponent* A, UPrimitiveComponent* B, const FHitResult& Hit) {
	// Simple rule: move whichever component has a movement component, or split 50/50
	if (!A || !A->GetOwner() || !A->GetOwner()->GetRootComponent()) return;
	if (!B || !B->GetOwner() || !B->GetOwner()->GetRootComponent()) return;
	UPrimitiveComponent* PrimA = dynamic_cast<UPrimitiveComponent*>(A->GetOwner()->GetRootComponent());
	UPrimitiveComponent* PrimB = dynamic_cast<UPrimitiveComponent*>(B->GetOwner()->GetRootComponent());
	if (!PrimA || !PrimB) return;

	bool aMovable = PrimA->GetMobility() == EComponentMobility::Movable;
	bool bMovable = PrimB->GetMobility() == EComponentMobility::Movable;

	FVector correction = Hit.ImpactNormal * Hit.PenetrationDepth;
	if (aMovable && !bMovable)
		A->GetOwner()->SetActorLocation(A->GetOwner()->GetActorLocation() + correction);
	else if (bMovable && !aMovable)
		B->GetOwner()->SetActorLocation(B->GetOwner()->GetActorLocation() - correction);
	else if (aMovable && bMovable) {
		A->GetOwner()->SetActorLocation(A->GetOwner()->GetActorLocation() + correction * 0.5f);
		B->GetOwner()->SetActorLocation(B->GetOwner()->GetActorLocation() - correction * 0.5f);
	}
	// both static -> do nothing
}

void UWorld::EndPlay()
{
	// Scene 전환, World 정리 후 Slomo, DeltaTime 남는 문제 방지
	RawDeltaTime = 0.f;
	DeltaTime = 0.f;
	SetGlobalTimeDilation(1.0f);

	bHasBegunPlay = false;
	TickManager.Reset();
	ActiveCamera = nullptr;
	LastLODUpdateCamera = nullptr;
	bHasLastFullLODUpdateCameraPos = false;

	for (ULevel* Level : Levels)
	{
		if (Level) Level->EndPlay();
	}
	AuthorGameMode = nullptr;

	Partition.Reset(FBoundingBox());

	for (ULevel* Level : Levels)
	{
		if (Level) Level->Clear();
	}
	MarkWorldPrimitivePickingBVHDirty();

	ClearLevels();
}
