#include "Editor/Selection/SelectionManager.h"
#include "Object/Object.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Scene/FScene.h"
#include "Object/GarbageCollection.h"

#include <algorithm>

USceneComponent* FSelectionManager::GetSelectedComponent() const
{
    return SelectedComponent.Get();
}

bool FSelectionManager::IsSelected(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    return std::find_if(
        SelectedActors.begin(),
        SelectedActors.end(),
        [Actor](const TWeakObjectPtr<AActor>& SelectedActor)
        {
            return SelectedActor.Get() == Actor;
        }) != SelectedActors.end();
}

AActor* FSelectionManager::GetPrimarySelection() const
{
    for (const TWeakObjectPtr<AActor>& ActorRef : SelectedActors)
    {
        if (AActor* Actor = ActorRef.Get())
        {
            return Actor;
        }
    }

    return nullptr;
}

UGizmoComponent* FSelectionManager::GetGizmo() const
{
    return IsValid(Gizmo) ? Gizmo : nullptr;
}

TArray<AActor*> FSelectionManager::GetSelectedActors() const
{
    TArray<AActor*> ValidActors;
    ValidActors.reserve(SelectedActors.size());

    for (const TWeakObjectPtr<AActor>& ActorRef : SelectedActors)
    {
        if (AActor* Actor = ActorRef.Get())
        {
            ValidActors.push_back(Actor);
        }
    }

    return ValidActors;
}

void FSelectionManager::Init()
{
    Gizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
    if (!Gizmo)
    {
        return;
    }

    Gizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
    Gizmo->Deactivate();
}

void FSelectionManager::Shutdown()
{
    ClearSelection();
    World.Reset();

    if (Gizmo)
    {
        UObjectManager::Get().DestroyObject(Gizmo);
        Gizmo = nullptr;
    }
}

void FSelectionManager::Select(AActor* Actor)
{
    PruneInvalidSelection();

    if (!IsValid(Actor))
    {
        ClearSelection();
        return;
    }

    USceneComponent* RootComponent = Actor->GetRootComponent();
    if (!IsValid(RootComponent))
    {
        ClearSelection();
        return;
    }

    if (SelectedActors.size() == 1 && SelectedActors.front().Get() == Actor && SelectedComponent.Get() == RootComponent)
    {
        return;
    }

    // 기존 선택 해제
    for (const TWeakObjectPtr<AActor>& PrevRef : SelectedActors)
    {
        if (AActor* Prev = PrevRef.Get())
        {
            SetActorProxiesSelected(Prev, false);
        }
    }

    SelectedActors.clear();
    SelectedActors.push_back(Actor);
    SetActorProxiesSelected(Actor, true);
    SelectedComponent = RootComponent;

    SyncGizmo();
}

void FSelectionManager::SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList)
{
    PruneInvalidSelection();

    if (!IsValid(ClickedActor)) return;

    // Find index of clicked actor
    int32 ClickedIdx = -1;
    for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
    {
        if (ActorList[i] == ClickedActor)
        {
            ClickedIdx = i;
            break;
        }
    }
    if (ClickedIdx == -1) return;

    // Find nearest already-selected actor's index in ActorList
    int32 AnchorIdx = ClickedIdx;
    int32 MinDist   = INT_MAX;
    for (const TWeakObjectPtr<AActor>& SelRef : SelectedActors)
    {
        AActor* Sel = SelRef.Get();
        if (!IsValid(Sel))
        {
            continue;
        }

        for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
        {
            if (ActorList[i] == Sel)
            {
                int32 Dist = std::abs(i - ClickedIdx);
                if (Dist < MinDist)
                {
                    MinDist   = Dist;
                    AnchorIdx = i;
                }
                break;
            }
        }
    }

    // Replace selection with range [min, max]
    int32 Lo = std::min(AnchorIdx, ClickedIdx);
    int32 Hi = std::max(AnchorIdx, ClickedIdx);

    // 기존 선택 해제
    for (const TWeakObjectPtr<AActor>& PrevRef : SelectedActors) { if (AActor* Prev = PrevRef.Get()) SetActorProxiesSelected(Prev, false); }

    SelectedActors.clear();
    SelectedComponent.Reset();

    for (int32 i = Lo; i <= Hi; ++i)
    {
        AActor* Actor = ActorList[i];
        if (IsValid(Actor))
        {
            SelectedActors.push_back(Actor);
            SetActorProxiesSelected(Actor, true);
        }
    }

    PruneInvalidSelection();
    SyncGizmo();
}

void FSelectionManager::ToggleSelect(AActor* Actor)
{
    PruneInvalidSelection();

    if (!IsValid(Actor)) return;

    auto It = std::find_if(SelectedActors.begin(), SelectedActors.end(), [Actor](const TWeakObjectPtr<AActor>& Ref) { return Ref.Get() == Actor; });
    if (It != SelectedActors.end())
    {
        SetActorProxiesSelected(Actor, false);
        SelectedActors.erase(It);
        if (USceneComponent* AliveComponent = SelectedComponent.GetAlive())
        {
            if (AliveComponent->GetOwner() == Actor)
            {
                SelectedComponent.Reset();
                PruneInvalidSelection();
            }
        }
    }
    else
    {
        SelectedActors.push_back(Actor);
        SetActorProxiesSelected(Actor, true);
        if (SelectedActors.size() == 1)
        {
            USceneComponent* RootComponent = Actor->GetRootComponent();
            SelectedComponent              = IsValid(RootComponent) ? RootComponent : nullptr;
        }
    }
    SyncGizmo();
}

void FSelectionManager::Deselect(AActor* Actor)
{
    PruneInvalidSelection();

    auto It = std::find_if(SelectedActors.begin(), SelectedActors.end(), [Actor](const TWeakObjectPtr<AActor>& Ref) { return Ref.Get() == Actor; });
    if (It != SelectedActors.end())
    {
        SetActorProxiesSelected(Actor, false);
        SelectedActors.erase(It);
        if (USceneComponent* AliveComponent = SelectedComponent.GetAlive())
        {
            if (AliveComponent->GetOwner() == Actor)
            {
                SelectedComponent.Reset();
                PruneInvalidSelection();
            }
        }
    }
    SyncGizmo();
}

void FSelectionManager::ClearSelection()
{
    PruneInvalidSelection();

    if (SelectedActors.empty() && SelectedComponent.Get() == nullptr)
    {
        return;
    }

    for (const TWeakObjectPtr<AActor>& ActorRef : SelectedActors)
    {
        if (AActor* Actor = ActorRef.Get())
        {
            SetActorProxiesSelected(Actor, false);
        }
    }

    SelectedActors.clear();
    SelectedComponent.Reset();
    SyncGizmo();
}

int32 FSelectionManager::DeleteSelectedActors()
{
    PruneInvalidSelection();

    if (!IsValid(World.Get()) || SelectedActors.empty())
    {
        return 0;
    }

    TArray<AActor*> ActorsToDelete = GetSelectedActors();
    const int32     DeletedCount   = static_cast<int32>(ActorsToDelete.size());

    // 파괴 전에 선택/기즈모 참조를 먼저 끊어 dangling target을 방지한다.
    ClearSelection();

    World->BeginDeferredPickingBVHUpdate();
    for (AActor* Actor : ActorsToDelete)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        World->DestroyActor(Actor);
    }
    World->EndDeferredPickingBVHUpdate();

    return DeletedCount;
}

void FSelectionManager::Tick()
{
    PruneInvalidSelection();

    if (!IsValid(Gizmo) || !bGizmoEnabled)
    {
        return;
    }

    USceneComponent* Primary = SelectedComponent.Get();
    if (!IsValid(Primary))
    {
        return;
    }

    if (Gizmo->GetTargetComponent() != Primary)
    {
        SyncGizmo();
        return;
    }

    Gizmo->UpdateGizmoTransform();
}

void FSelectionManager::SelectComponent(USceneComponent* Component)
{
    PruneInvalidSelection();

    if (!IsValid(Component))
    {
        return;
    }

    // [버그 수정] 에디터 전용 컴포넌트(광원 아이콘 등)는 개별 조작 대상이 아니므로,
    // 부모 컴포넌트로 리다이렉트하여 함께 움직이도록 합니다.
    USceneComponent* Target = Component;
    if (Component->IsEditorOnlyComponent())
    {
        if (IsValid(Component->GetParent()))
        {
            Target = Component->GetParent();
        }
        else
        {
            AActor* ComponentOwner = Component->GetOwner();
            if (IsValid(ComponentOwner))
            {
                Target = ComponentOwner->GetRootComponent();
            }
        }
    }

    if (!IsValid(Target))
    {
        return;
    }

    if (SelectedComponent.Get() == Target)
    {
        return;
    }

    AActor* Owner = Target->GetOwner();
    if (!IsValid(Owner))
    {
        return;
    }

    if (!IsSelected(Owner))
    {
        Select(Owner);
    }

    // Select(Owner)는 actor root를 선택 대상으로 잡기 때문에, owner 선택 보장 후
    // 실제 component 선택 대상을 다시 설정합니다.
    SelectedComponent = Target;

    SyncGizmo();
}

void FSelectionManager::SetGizmoEnabled(bool bEnabled)
{
    if (bGizmoEnabled == bEnabled)
    {
        return;
    }

    bGizmoEnabled = bEnabled;
    SyncGizmo();
}

void FSelectionManager::SetWorld(UWorld* InWorld)
{
    PruneInvalidSelection();

    // 기존 Scene에서 Gizmo 프록시 해제
    if (Gizmo && IsValid(World.Get()))
        Gizmo->DestroyRenderState();

    World = IsValid(InWorld) ? InWorld : nullptr;

    // 새 Scene에 Gizmo 프록시 등록
    if (IsValid(Gizmo) && IsValid(World.Get()))
    {
        Gizmo->SetScene(&World.Get()->GetScene());
        Gizmo->CreateRenderState();
    }

    SyncGizmo();
}

void FSelectionManager::AddReferencedObjects(FReferenceCollector& Collector)
{
    // Selection targets/world are weak references. The editor-owned gizmo is the only UObject
    // whose lifetime is owned by the selection manager.
    Collector.AddReferencedObject(Gizmo);
}

void FSelectionManager::PruneInvalidSelection()
{
    bool bSelectionChanged = false;

    const size_t OldActorCount = SelectedActors.size();
    SelectedActors.erase(
        std::remove_if(
            SelectedActors.begin(),
            SelectedActors.end(),
            [](const TWeakObjectPtr<AActor>& ActorRef)
            {
                return ActorRef.Get() == nullptr;
            }
        ),
        SelectedActors.end()
    );
    bSelectionChanged = bSelectionChanged || OldActorCount != SelectedActors.size();

    if (SelectedComponent.GetAlive())
    {
        AActor* Owner = SelectedComponent.GetAlive()->GetOwner();
        if (!IsValid(SelectedComponent.Get()) || !IsValid(Owner))
        {
            SelectedComponent.Reset();
            bSelectionChanged = true;
        }
    }

    if (SelectedComponent.Get())
    {
        AActor* Owner = SelectedComponent.Get()->GetOwner();
        if (!IsValid(Owner) || !IsSelected(Owner))
        {
            SelectedComponent.Reset();
            bSelectionChanged = true;
        }
    }

    if (!SelectedComponent.Get() && !SelectedActors.empty())
    {
        for (const TWeakObjectPtr<AActor>& ActorRef : SelectedActors)
        {
            AActor* Actor = ActorRef.Get();
            if (!IsValid(Actor))
            {
                continue;
            }

            USceneComponent* Root = Actor->GetRootComponent();
            if (IsValid(Root))
            {
                SelectedComponent = Root;
                break;
            }
        }
    }

    if (bSelectionChanged && IsValid(Gizmo))
    {
        RefreshSelectedActorCache();
        Gizmo->SetSelectedActors(SelectedActorCache.empty() ? nullptr : &SelectedActorCache);
    }
}

void FSelectionManager::RefreshSelectedActorCache()
{
    SelectedActorCache.clear();
    SelectedActorCache.reserve(SelectedActors.size());
    for (const TWeakObjectPtr<AActor>& ActorRef : SelectedActors)
    {
        if (AActor* Actor = ActorRef.Get())
        {
            SelectedActorCache.push_back(Actor);
        }
    }
}

void FSelectionManager::SyncGizmo()
{
    PruneInvalidSelection();

    if (!IsValid(Gizmo)) return;

    if (!bGizmoEnabled)
    {
        Gizmo->Deactivate();
        return;
    }

    USceneComponent* Primary = SelectedComponent.Get();
    if (IsValid(Primary))
    {
        RefreshSelectedActorCache();
        Gizmo->SetSelectedActors(SelectedActorCache.empty() ? nullptr : &SelectedActorCache);
        Gizmo->SetTarget(Primary);
    }
    else
    {
        Gizmo->SetSelectedActors(nullptr);
        Gizmo->Deactivate();
    }
}

void FSelectionManager::SetActorProxiesSelected(AActor* Actor, bool bSelected)
{
    if (!IsValid(Actor) || !IsValid(World.Get())) return;

    FScene& Scene = World->GetScene();
    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!IsValid(Prim))
        {
            continue;
        }

        if (FPrimitiveSceneProxy* Proxy = Prim->GetSceneProxy())
        {
            if (Proxy->HasValidOwner())
            {
                Scene.SetProxySelected(Proxy, bSelected);
            }
        }
    }
}

