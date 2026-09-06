// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "PrimitiveComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Core/RayTypes.h"
#include "Collision/RayUtils.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Core/CollisionTypes.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "GameFramework/World.h"

#include <cmath>
#include <cstring>

namespace
{
bool HasSameTransformBasis(const FMatrix& A, const FMatrix& B)
{
    for (int Row = 0; Row < 3; ++Row)
    {
        for (int Col = 0; Col < 3; ++Col)
        {
            if (A.M[Row][Col] != B.M[Row][Col])
            {
                return false;
            }
        }
    }

    return true;
}
} // namespace

IMPLEMENT_CLASS(UPrimitiveComponent, USceneComponent)

namespace
{
	//누군가 이 노가다를 멈춰주세요..
constexpr FEnumPropertyOption GComponentMobilityOptions[] = {
    { "Static", static_cast<int32_t>(EComponentMobility::Static) },
    { "Movable", static_cast<int32_t>(EComponentMobility::Movable) },
};

constexpr FEnumPropertyMeta GComponentMobilityMeta = {
    "EComponentMobility",
    GComponentMobilityOptions,
    static_cast<int32_t>(sizeof(GComponentMobilityOptions) / sizeof(GComponentMobilityOptions[0])),
};

constexpr FEnumPropertyOption GCollisionChannels[] = {
    { "WorldStatic", static_cast<int32_t>(ECollisionChannel::WorldStatic) },
    { "WorldDynamic", static_cast<int32_t>(ECollisionChannel::WorldDynamic) },
    { "Enemy", static_cast<int32_t>(ECollisionChannel::Enemy) },
    { "Player", static_cast<int32_t>(ECollisionChannel::Player) },
    { "Projectile", static_cast<int32_t>(ECollisionChannel::Projectile) },
    { "Pickup", static_cast<int32_t>(ECollisionChannel::Pickup) },
    { "Trigger", static_cast<int32_t>(ECollisionChannel::Trigger) },
    { "FlyingEnemy", static_cast<int32_t>(ECollisionChannel::FlyingEnemy) },
    { "PickupSensor", static_cast<int32_t>(ECollisionChannel::PickupSensor) },
    { "PlayerHitbox", static_cast<int32_t>(ECollisionChannel::PlayerHitbox) }
};

constexpr FEnumPropertyMeta GCollisionChannelMeta = {
    "ECollisionChannel",
    GCollisionChannels,
    static_cast<int32_t>(sizeof(GCollisionChannels) / sizeof(GCollisionChannels[0])),
};
}

UPrimitiveComponent::~UPrimitiveComponent()
{
    DestroyRenderState();
}

void UPrimitiveComponent::MarkProxyDirty(ESceneProxyDirtyFlag Flag) const
{
    if (SceneProxies.empty())
        return;

    for (const auto& Entry : SceneProxies)
    {
        FScene* Scene = Entry.first;
        FPrimitiveProxy* Proxy = Entry.second;
        if (Scene && Proxy)
        {
            Scene->MarkProxyDirty(Proxy, Flag);
        }
    }
}

void UPrimitiveComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << bIsVisible;
    Ar << bVisibleInEditor;
    Ar << bVisibleInGame;
    Ar << bGenerateHitEvents;
    Ar << bGenerateOverlapEvents;
    Ar << bIsEditorHelper;
    Ar << bReceivesDecals;
    Ar << Mobility;
    Ar << CollisionChannel;
}

void UPrimitiveComponent::SetVisibility(bool bNewVisible)
{
    const bool bNeedsChildSync = (bVisibleInEditor != bNewVisible) || (bVisibleInGame != bNewVisible);
    if (bIsVisible == bNewVisible && !bNeedsChildSync)
    {
        return;
    }

    bIsVisible = bNewVisible;
    bVisibleInEditor = bNewVisible;
    bVisibleInGame = bNewVisible;
    MarkRenderVisibilityDirty();
}

void UPrimitiveComponent::SetVisibleInEditor(bool bNewVisible)
{
    if (bVisibleInEditor == bNewVisible)
    {
        return;
    }

    bVisibleInEditor = bNewVisible;
    MarkRenderVisibilityDirty();
}

void UPrimitiveComponent::SetVisibleInGame(bool bNewVisible)
{
    if (bVisibleInGame == bNewVisible)
    {
        return;
    }

    bVisibleInGame = bNewVisible;
    MarkRenderVisibilityDirty();
}

void UPrimitiveComponent::SetEditorHelper(bool bNewHelper)
{
    if (bIsEditorHelper == bNewHelper)
    {
        return;
    }

    bIsEditorHelper = bNewHelper;
    MarkRenderVisibilityDirty();
}

void UPrimitiveComponent::SetReceivesDecals(bool bNewReceivesDecals)
{
    if (bReceivesDecals == bNewReceivesDecals)
    {
        return;
    }

    bReceivesDecals = bNewReceivesDecals;
    MarkRenderStateDirty();
}

bool UPrimitiveComponent::ShouldRenderInWorld(EWorldType WorldType) const
{
    if (!bIsVisible)
    {
        return false;
    }

    switch (WorldType)
    {
    case EWorldType::Editor:
        return bVisibleInEditor;
    case EWorldType::PIE:
    case EWorldType::Game:
        return bVisibleInGame;
    default:
        return bVisibleInGame;
    }
}

bool UPrimitiveComponent::ShouldRenderInCurrentWorld() const
{
    AActor* OwnerActor = GetOwner();
    UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
    return ShouldRenderInWorld(World ? World->GetWorldType() : EWorldType::Game);
}

const TArray<FOverlapInfo>& UPrimitiveComponent::GetOverlapInfos() const
{
    return OverlapInfos;
}

void UPrimitiveComponent::AddOverlapInfo(UPrimitiveComponent* OtherPrimitive)
{
    OverlapInfos.emplace_back(FOverlapInfo(OtherPrimitive));
}

void UPrimitiveComponent::ClearOverlapInfos()
{
    OverlapInfos.clear();
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* OtherActor) const
{
    if (!OtherActor)
    {
        return false;
    }

    for (const FOverlapInfo& Info : OverlapInfos)
    {
        const UPrimitiveComponent* OtherComponent = Info.OverlappedComponent;
        if (!OtherComponent)
        {
            continue;
        }

        if (OtherComponent->GetOwner() == OtherActor)
        {
            return true;
        }
    }

    return false;
}

bool UPrimitiveComponent::IsOverlappingComponent(const UPrimitiveComponent* OtherPrimitive) const
{
    if (!OtherPrimitive)
    {
        return false;
    }

    for (const FOverlapInfo& Info : OverlapInfos)
    {
        if (Info.OverlappedComponent == OtherPrimitive)
        {
            return true;
        }
    }

    return false;
}

void UPrimitiveComponent::SetCollisionChannel(ECollisionChannel NewChannel)
{
    CollisionChannel = NewChannel;
}

// ============================================================
// MarkRenderTransformDirty / MarkRenderVisibilityDirty
// ============================================================
void UPrimitiveComponent::MarkRenderTransformDirty()
{
    MarkProxyDirty(ESceneProxyDirtyFlag::Transform);

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
        return;
    UWorld* World = OwnerActor->GetWorld();
    if (!World)
        return;

    World->UpdateActorInOctree(OwnerActor);
    World->MarkEditorPickingAndScenePrimitiveBVHsDirty();
}

void UPrimitiveComponent::MarkRenderVisibilityDirty()
{
    MarkProxyDirty(ESceneProxyDirtyFlag::Visibility);

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
        return;
    UWorld* World = OwnerActor->GetWorld();
    if (!World)
        return;

    World->UpdateActorInOctree(OwnerActor);
    World->MarkEditorPickingAndScenePrimitiveBVHsDirty();
}

void UPrimitiveComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
    OutProps.push_back({ "Visible In Editor", EPropertyType::Bool, &bVisibleInEditor });
    OutProps.push_back({ "Visible In Game", EPropertyType::Bool, &bVisibleInGame });
    OutProps.push_back({ "Is Editor Helper", EPropertyType::Bool, &bIsEditorHelper });
    OutProps.push_back({ "Receives Decals", EPropertyType::Bool, &bReceivesDecals });
    OutProps.push_back({ "Generate Overlap Events", EPropertyType::Bool, &bGenerateOverlapEvents });
    OutProps.push_back({ "Generate Hit Events", EPropertyType::Bool, &bGenerateHitEvents });
    OutProps.push_back({ "Mobility", EPropertyType::Enum, &Mobility, 0.0f, 0.0f, 0.1f, &GComponentMobilityMeta });
    OutProps.push_back({ "CollisionChannel", EPropertyType::Enum, &CollisionChannel, 0.0f, 0.0f, 0.1f, &GCollisionChannelMeta });
}

void UPrimitiveComponent::PostEditProperty(const char* PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Visible") == 0)
    {
        bVisibleInEditor = bIsVisible;
        bVisibleInGame = bIsVisible;
        MarkRenderVisibilityDirty();
    }
    else if (strcmp(PropertyName, "Visible In Editor") == 0 || strcmp(PropertyName, "Visible In Game") == 0 || strcmp(PropertyName, "Is Editor Helper") == 0)
    {
        MarkRenderVisibilityDirty();
    }
    else if (strcmp(PropertyName, "Receives Decals") == 0)
    {
        MarkRenderStateDirty();
    }
}

void UPrimitiveComponent::Activate()
{
    Super::Activate();
    SetVisibility(true);
}

void UPrimitiveComponent::Deactivate()
{
    Super::Deactivate();
    SetVisibility(false);
}

FBoundingBox UPrimitiveComponent::GetWorldBoundingBox() const
{
    EnsureWorldAABBUpdated();
    return FBoundingBox(WorldAABBMinLocation, WorldAABBMaxLocation);
}

void UPrimitiveComponent::MarkWorldBoundsDirty()
{
    bWorldAABBDirty = true;
    bHasValidWorldAABB = false;
    MarkRenderTransformDirty();
}

void UPrimitiveComponent::UpdateWorldAABB() const
{
    FVector LExt = LocalExtents;

    FMatrix worldMatrix = GetWorldMatrix();

    float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
    float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
    float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

    FVector WorldCenter = GetWorldLocation();
    WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
    WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}

/*
    현재 지원하지 않는 기본 머티리얼 폴백 지점입니다.
*/
bool UPrimitiveComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
    FMeshDataView View = GetMeshDataView();
    if (!View.IsValid())
        return false;

    bool bHit = FRayUtils::RaycastTriangles(
        Ray, GetWorldMatrix(),
        GetWorldInverseMatrix(),
        View.VertexData,
        View.Stride,
        View.IndexData,
        View.IndexCount,
        OutHitResult);

    if (bHit)
    {
        OutHitResult.HitComponent = this;
    }
    return bHit;
}

void UPrimitiveComponent::UpdateWorldMatrix() const
{
    const FMatrix PreviousWorldMatrix = CachedWorldMatrix;
    const FVector PreviousWorldAABBMin = WorldAABBMinLocation;
    const FVector PreviousWorldAABBMax = WorldAABBMaxLocation;
    const bool bHadValidWorldAABB = bHasValidWorldAABB;

    USceneComponent::UpdateWorldMatrix();

    if (bWorldAABBDirty)
    {
        if (bHadValidWorldAABB && HasSameTransformBasis(PreviousWorldMatrix, CachedWorldMatrix))
        {
            const FVector TranslationDelta = CachedWorldMatrix.GetLocation() - PreviousWorldMatrix.GetLocation();
            WorldAABBMinLocation = PreviousWorldAABBMin + TranslationDelta;
            WorldAABBMaxLocation = PreviousWorldAABBMax + TranslationDelta;
            bWorldAABBDirty = false;
            bHasValidWorldAABB = true;
        }
        else
        {
            UpdateWorldAABB();
        }
    }

    MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
}

FPrimitiveProxy* UPrimitiveComponent::CreateSceneProxy()
{
    return new FPrimitiveProxy(this);
}

FPrimitiveProxy* UPrimitiveComponent::GetSceneProxy() const
{
    if (Owner && Owner->GetWorld())
    {
        return GetSceneProxy(Owner->GetWorld()->GetScene());
    }

    if (SceneProxies.size() == 1)
    {
        return SceneProxies.begin()->second;
    }

    return nullptr;
}

FPrimitiveProxy* UPrimitiveComponent::GetSceneProxy(FScene& Scene) const
{
    auto It = SceneProxies.find(&Scene);
    if (It != SceneProxies.end())
    {
        return It->second;
	}
    return nullptr;
}

void UPrimitiveComponent::CreateRenderState()
{
    if (!Owner || !Owner->GetWorld())
        return;

    UWorld* World = Owner->GetWorld();
    FScene& Scene = World->GetScene();

    auto It = SceneProxies.find(&Scene);
    if (It != SceneProxies.end())
    {
        return;
    }

    auto* Proxy = Scene.AddPrimitive(this);
    if (!Proxy)
    {
        return;
    }

    SceneProxies.emplace(&Scene, Proxy);

    World->GetPartition().AddSinglePrimitive(this);
    World->MarkEditorPickingAndScenePrimitiveBVHsDirty();
}

void UPrimitiveComponent::DestroyRenderState()
{
    for (const auto& Entry : SceneProxies)
    {
        FScene* Scene = Entry.first;
        FPrimitiveProxy* Proxy = Entry.second;
        if (Scene && Proxy)
        {
            Scene->RemovePrimitive(Proxy);
        }
    }

    SceneProxies.clear();

    if (Owner && Owner->GetWorld())
    {
        UWorld* World = Owner->GetWorld();
        World->GetPartition().RemoveSinglePrimitive(this);
        World->MarkEditorPickingAndScenePrimitiveBVHsDirty();
    }
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
    DestroyRenderState();
    CreateRenderState();
}

void UPrimitiveComponent::SetGenerateOverlapEvents(bool bNewGenerateOverlap)
{
    bGenerateOverlapEvents = bNewGenerateOverlap;
}

void UPrimitiveComponent::SetGenerateHitEvents(bool bNewGenerate)
{
    bGenerateHitEvents = bNewGenerate;
}

void UPrimitiveComponent::SetMobility(EComponentMobility NewMobility)
{
    Mobility = NewMobility;
}

bool UPrimitiveComponent::IsMovable() const
{
    return Mobility == EComponentMobility::Movable;
}

bool UPrimitiveComponent::IsStatic() const
{
    return Mobility == EComponentMobility::Static;
}

void UPrimitiveComponent::OnTransformDirty()
{
    bWorldAABBDirty = true;
    MarkRenderTransformDirty();
}

void UPrimitiveComponent::EnsureWorldAABBUpdated() const
{
    GetWorldMatrix();
    if (bWorldAABBDirty)
    {
        UpdateWorldAABB();
    }
}

