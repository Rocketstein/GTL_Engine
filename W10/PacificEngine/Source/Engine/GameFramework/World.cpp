// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "GameFramework/World.h"
#include "GameFramework/ActorPool.h"
#include "GameFramework/AWorldSettings.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Component/CameraComponent.h"
#include "Render/Visibility/LOD/LODContext.h"
#include <algorithm>
#include "Profiling/Stats.h"

//풀링 테스트
#include "GameFramework/GamejamActor/EnemyBaseActor.h"

IMPLEMENT_CLASS(UWorld, UObject)

UWorld::~UWorld()
{
    if ((ActiveLevel && !ActiveLevel->GetActors().empty()) || (PersistentLevel && !PersistentLevel->GetActors().empty()))
    {
        EndPlay();
    }
}

UObject* UWorld::Duplicate(UObject* NewOuter) const
{
    // UE의 CreatePIEWorldByDuplication 대응 (간소화 버전).
    // 새 UWorld를 만들고, 소스의 Actor들을 하나씩 복제해 NewWorld를 Outer로 삼아 등록한다.
    // AActor::Duplicate 내부에서 Dup->GetTypedOuter<UWorld>() 경유 AddActor가 호출되므로
    // 여기서는 World 단위 상태만 챙기면 된다.
    UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
    if (!NewWorld)
    {
        return nullptr;
    }
    NewWorld->SetOuter(NewOuter);
    NewWorld->InitWorld(); // Partition/VisibleSet 초기화 — 이거 없으면 복제 액터가 렌더링되지 않음
    NewWorld->SetWorldType(GetWorldType());
    NewWorld->SetGameplayPaused(IsGameplayPaused());
    NewWorld->SetEditorActorFolders(EditorActorFolders);

    // Duplicate both levels
    for (AActor* Src : ActiveLevel->GetActors())
    {
        if (!Src)
            continue;
        Src->Duplicate(NewWorld->GetActiveLevel());
    }
    for (AActor* Src : PersistentLevel->GetActors())
    {
        if (!Src || Src->IsA<AWorldSettings>())
            continue;

        Src->Duplicate(NewWorld->GetPersistentLevel());
    }

    NewWorld->PostDuplicate();
    return NewWorld;
}

void UWorld::DestroyActor(AActor* Actor)
{
    // remove and clean up
    if (!Actor)
        return;
    if (ActorPoolManager)
    {
        ActorPoolManager->ForgetActor(Actor);
    }
    Actor->EndPlay();

    // Remove from correct level
    ULevel* ActorLevel = Actor->GetTypedOuter<ULevel>();
    if (ActorLevel)
    {
        ActorLevel->RemoveActor(Actor);
    }

    if (ActorLevel == ActiveLevel)
    {
        MarkEditorPickingAndScenePrimitiveBVHsDirty();
        Partition.RemoveActor(Actor);
    }

    // Mark for garbage collection
    UObjectManager::Get().DestroyObject(Actor);
}

AActor* UWorld::SpawnActorByClass(const FString& ClassName)
{
    UObject* Obj = FObjectFactory::Get().Create(ClassName, ActiveLevel);
    AActor* Actor = Cast<AActor>(Obj);
    if (Actor)
    {
        Actor->InitDefaultComponents();
        AddActor(Actor);
    }
    return Actor;
}

AActor* UWorld::SpawnPersistentActorByClass(const FString& ClassName)
{
    UObject* Obj = FObjectFactory::Get().Create(ClassName, PersistentLevel);
    AActor* Actor = Cast<AActor>(Obj);
    if (Actor)
    {
        Actor->InitDefaultComponents();
        AddActor(Actor);
    }
    return Actor;
}

void UWorld::AddActor(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    ULevel* ActorLevel = Actor->GetTypedOuter<ULevel>();
    if (!ActorLevel)
    {
        // Default to ActiveLevel if no level is assigned
        ActorLevel = ActiveLevel;
    }
    ActorLevel->AddActor(Actor);

    // 액터 생성자 시점에는 Outer가 아직 Level/World에 연결되지 않았을 수 있다.
    // 그 상태에서 만들어진 컴포넌트는 CreateRenderState()가 early-out 되므로
    // 월드 등록 직후 누락된 렌더 스테이트를 한 번 더 보정한다.
    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (!Component)
        {
            continue;
        }

        if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
        {
            if (!Primitive->GetSceneProxy())
            {
                Primitive->CreateRenderState();
            }
            continue;
        }

        Component->CreateRenderState();
    }

    if (ActorLevel == ActiveLevel)
    {
        InsertActorToOctree(Actor);
        MarkEditorPickingAndScenePrimitiveBVHsDirty();
    }

    // PIE 중 Duplicate(Ctrl+D)나 SpawnActor로 들어온 액터에도 BeginPlay를 보장.
    if (bHasBegunPlay && !Actor->HasActorBegunPlay())
    {
        Actor->BeginPlay();
    }
}

void UWorld::MarkEditorPickingAndScenePrimitiveBVHsDirty()
{
    if (DeferredPickingBVHUpdateDepth > 0)
    {
        bDeferredPickingBVHDirty = true;
        return;
    }

    EditorPickingBVH.MarkDirty();
    ScenePrimitiveBVH.MarkDirty();
}

void UWorld::BuildEditorPickingBVHNow() const
{
    EditorPickingBVH.BuildNow(GetActors());
}

void UWorld::BuildScenePrimitiveBVHNow() const
{
    ScenePrimitiveBVH.BuildNow(GetActors());
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
        BuildEditorPickingBVHNow();
        BuildScenePrimitiveBVHNow();
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
            if (!Primitive || !Primitive->ShouldRenderInCurrentWorld() || !Primitive->IsA<UStaticMeshComponent>())
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

    BuildEditorPickingBVHNow();
    BuildScenePrimitiveBVHNow();
}

bool UWorld::RaycastEditorPicking(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
    if (WorldType == EWorldType::PIE)
    {
        OutActor = nullptr;
        OutHitResult = FHitResult{};
        return false;
    }

    EditorPickingBVH.EnsureBuilt(GetActors());
    return EditorPickingBVH.Raycast(Ray, OutHitResult, OutActor);
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

void UWorld::AddEditorActorFolder(const FString& FolderPath)
{
    if (FolderPath.empty())
    {
        return;
    }

    if (std::find(EditorActorFolders.begin(), EditorActorFolders.end(), FolderPath) == EditorActorFolders.end())
    {
        EditorActorFolders.push_back(FolderPath);
    }
}

void UWorld::RemoveEditorActorFolder(const FString& FolderPath)
{
    auto It = std::remove(EditorActorFolders.begin(), EditorActorFolders.end(), FolderPath);
    EditorActorFolders.erase(It, EditorActorFolders.end());
}

FLODUpdateContext UWorld::PrepareLODContext()
{
    if (!ActiveCamera)
        return {};

    const FVector CameraPos = ActiveCamera->GetWorldLocation();
    const FVector CameraForward = ActiveCamera->GetForwardVector();

    const uint32 LODUpdateFrame = VisibleProxyBuildFrame++;
    const uint32 LODUpdateSlice = LODUpdateFrame & (LOD_UPDATE_SLICE_COUNT - 1);
    const bool bShouldStaggerLOD = Scene.GetPrimitiveProxyCount() >= LOD_STAGGER_MIN_VISIBLE;

    const bool bForceFullLODRefresh =
        !bShouldStaggerLOD || LastLODUpdateCamera != ActiveCamera || !bHasLastFullLODUpdateCameraPos || FVector::DistSquared(CameraPos, LastFullLODUpdateCameraPos) >= LOD_FULL_UPDATE_CAMERA_MOVE_SQ || CameraForward.Dot(LastFullLODUpdateCameraForward) < LOD_FULL_UPDATE_CAMERA_ROTATION_DOT;

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
    bGameplayPaused = false;
    Partition.Reset(FBoundingBox());
    ActiveLevel = UObjectManager::Get().CreateObject<ULevel>(this);
    ActiveLevel->SetWorld(this);
    PersistentLevel = UObjectManager::Get().CreateObject<ULevel>(this);
    PersistentLevel->SetWorld(this);

    WorldSettings = SpawnPersistentActor<AWorldSettings>();
}

void UWorld::BeginPlay()
{
    bHasBegunPlay = true;

    ActorPoolManager = std::make_unique<FActorPoolManager>(this);
    ActorPoolManager->SetWorld(this);

    if (ActiveLevel)
    {
        ActiveLevel->BeginPlay();
    }
    if (PersistentLevel)
    {
        PersistentLevel->BeginPlay();
    }
}

void UWorld::Tick(float DeltaTime, ELevelTick TickType)
{
    {
        SCOPE_STAT_CAT("FlushPrimitive", "1_WorldTick");
        Partition.FlushPrimitive();
    }

    //CollisionManager.Update(*this);
    Collision2DManager.Update(*this);

    Scene.GetDebugPrimitiveQueue().ClearOneFramePrimitives();
    Scene.GetDebugPrimitiveQueue().Tick(DeltaTime);


    TickManager.Tick(this, DeltaTime, TickType);
}

void UWorld::EndPlay()
{
    bHasBegunPlay = false;
    bGameplayPaused = false;
    TickManager.Reset();

    // Clear spatial partition while actors/components are still alive.
    // Otherwise Octree teardown can dereference stale primitive pointers during shutdown.
    Partition.Reset(FBoundingBox());

    // Script EndPlay can release pooled actors through Lua handles, so keep the
    // pool manager alive until all actor/component EndPlay callbacks finish.
    if (ActiveLevel)
    {
        ActiveLevel->EndPlay();
    }
    if (PersistentLevel)
    {
        PersistentLevel->EndPlay();
    }

    // Drop pool delegates before pooled actors are destroyed by the level.
    ActorPoolManager.reset();

    if (ActiveLevel)
    {
        ActiveLevel->Clear();
        UObjectManager::Get().DestroyObject(ActiveLevel);
        ActiveLevel = nullptr;
    }
    if (PersistentLevel)
    {
        PersistentLevel->Clear();
        UObjectManager::Get().DestroyObject(PersistentLevel);
        PersistentLevel = nullptr;
    }

    MarkEditorPickingAndScenePrimitiveBVHsDirty();
}
