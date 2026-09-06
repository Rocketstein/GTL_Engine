#include "Viewport/SkeletalPreviewViewportClient.h"

#include "Component/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Mesh/Skeleton.h"
#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Renderer.h"
#include "Render/Scene/Scene.h"
#include "Viewport/SkeletalMeshViewer.h"

#include <cmath>

namespace
{
float SafeDiv(float Numerator, float Denominator)
{
    return std::abs(Denominator) > FMath::Epsilon ? Numerator / Denominator : Numerator;
}

FQuat ExtractRotationNoScale(const FMatrix& Matrix)
{
    FMatrix RotationMatrix = Matrix;

    for (int32 Row = 0; Row < 3; ++Row)
    {
        FVector Axis(
            RotationMatrix.M[Row][0],
            RotationMatrix.M[Row][1],
            RotationMatrix.M[Row][2]);

        if (Axis.LengthSquared() > FMath::Epsilon * FMath::Epsilon)
        {
            Axis.Normalize();
            RotationMatrix.M[Row][0] = Axis.X;
            RotationMatrix.M[Row][1] = Axis.Y;
            RotationMatrix.M[Row][2] = Axis.Z;
        }
    }

    RotationMatrix.M[0][3] = 0.0f;
    RotationMatrix.M[1][3] = 0.0f;
    RotationMatrix.M[2][3] = 0.0f;
    RotationMatrix.M[3][0] = 0.0f;
    RotationMatrix.M[3][1] = 0.0f;
    RotationMatrix.M[3][2] = 0.0f;
    RotationMatrix.M[3][3] = 1.0f;

    return RotationMatrix.ToQuat();
}

FMatrix GetBoneComponentToDebugWorldMatrix(USkeletalMeshComponent* MeshComp, int32 BoneIndex)
{
    if (!MeshComp || BoneIndex < 0 || BoneIndex >= MeshComp->GetNumBones())
    {
        return FMatrix::Identity;
    }

    const FMatrix& BoneComponentMatrix = MeshComp->GetBoneComponentMatrix(BoneIndex);
    const FMatrix BoneDebugWorldMatrix = MeshComp->GetBoneDebugWorldMatrix(BoneIndex);
    return BoneComponentMatrix.GetInverse() * BoneDebugWorldMatrix;
}

FVector DebugWorldToBoneComponentPosition(
    USkeletalMeshComponent* MeshComp,
    int32 BoneIndex,
    const FVector& DebugWorldPosition)
{
    const FMatrix ComponentToDebugWorld = GetBoneComponentToDebugWorldMatrix(MeshComp, BoneIndex);
    return ComponentToDebugWorld.GetInverse().TransformPositionWithW(DebugWorldPosition);
}
}

FSkeletalPreviewViewportClient::FSkeletalPreviewViewportClient() = default;
FSkeletalPreviewViewportClient::~FSkeletalPreviewViewportClient() = default;

void FSkeletalPreviewViewportClient::Initialize(
    UEditorEngine* InEditorEngine,
    ID3D11Device* InDevice,
    FPreviewSceneContext* InPreviewContext,
    FSkeletalMeshViewer* InOwner)
{
    FPreviewViewportClient::Initialize(InEditorEngine, InDevice, InPreviewContext);
    OwnerViewer = InOwner;
}

void FSkeletalPreviewViewportClient::Release()
{
    OwnerViewer = nullptr;
    bHasLastGizmoComponentRotation = false;
    FPreviewViewportClient::Release();
}

void FSkeletalPreviewViewportClient::Tick(float DeltaTime)
{
    FPreviewViewportClient::Tick(DeltaTime);
    if (OwnerViewer)
    {
        OwnerViewer->HandleInput(DeltaTime);
    }
}

UGizmoComponent* FSkeletalPreviewViewportClient::GetGizmo()
{
    return OwnerViewer ? OwnerViewer->GetBoneGizmo() : nullptr;
}

void FSkeletalPreviewViewportClient::BeginInputFrame()
{
    if (OwnerViewer)
    {
        OwnerViewer->BeginInputFrame();
    }
}

bool FSkeletalPreviewViewportClient::InputKey(const FViewportKeyEvent& Event)
{
    return OwnerViewer ? OwnerViewer->InputKey(Event) : false;
}

bool FSkeletalPreviewViewportClient::InputAxis(const FViewportAxisEvent& Event)
{
    return OwnerViewer ? OwnerViewer->InputAxis(Event) : false;
}

bool FSkeletalPreviewViewportClient::InputPointer(const FViewportPointerEvent& Event)
{
    return OwnerViewer ? OwnerViewer->InputPointer(Event) : false;
}

void FSkeletalPreviewViewportClient::ResetInputState()
{
    if (OwnerViewer)
    {
        OwnerViewer->ResetInputState();
    }
}

void FSkeletalPreviewViewportClient::ResetKeyboardInputState()
{
    if (OwnerViewer)
    {
        OwnerViewer->ResetKeyboardInputState();
    }
}

EViewMode FSkeletalPreviewViewportClient::GetViewMode() const
{
    return OwnerViewer ? OwnerViewer->GetState().ViewMode : EViewMode::Lit_Phong;
}

void FSkeletalPreviewViewportClient::BeforeCollect(FRenderCollectContext& CollectContext, FScene& Scene)
{
    (void)CollectContext;
    (void)Scene;
    SyncBoneGizmoToSelection();
}

void FSkeletalPreviewViewportClient::CollectPreviewWorld(
    FRenderer& Renderer,
    UWorld* World,
    FRenderCollectContext& CollectContext)
{
    if (Options.bShowMesh)
    {
        Renderer.CollectWorld(World, CollectContext);
    }
}

void FSkeletalPreviewViewportClient::CollectPreviewOverlays(FRenderer& Renderer, FScene& Scene)
{
    if (Options.bShowSkeleton)
    {
        Renderer.CollectSkeletalDebug(Scene);
    }
}

void FSkeletalPreviewViewportClient::SyncBoneGizmoToSelection()
{
    UGizmoComponent* Gizmo = OwnerViewer ? OwnerViewer->GetBoneGizmo() : nullptr;
    AActor* TargetActor = OwnerViewer ? OwnerViewer->GetBoneGizmoTargetActor() : nullptr;
    USkeletalMeshComponent* MeshComp = OwnerViewer ? OwnerViewer->GetPreviewMeshComponent() : nullptr;
    if (!Gizmo || !TargetActor || !MeshComp)
    {
        return;
    }

    if (Gizmo->IsHolding())
    {
        ApplyBoneGizmoToSelection();
        return;
    }

    const int32 BoneIndex = Options.SelectedBoneIndex;
    if (BoneIndex < 0 || BoneIndex >= MeshComp->GetNumBones())
    {
        Gizmo->SetVisibility(false);
        bHasLastGizmoComponentRotation = false;
        return;
    }

    if (!Gizmo->HasTarget())
    {
        Gizmo->SetTarget(TargetActor);
    }

    const FMatrix BoneWorldMatrix = MeshComp->GetBoneDebugWorldMatrix(BoneIndex);
    const bool bWorldSpace = Gizmo->IsWorldSpace();
    if (USceneComponent* Root = TargetActor->GetRootComponent())
    {
        Root->SetRelativeLocation(BoneWorldMatrix.GetLocation());
        if (bWorldSpace)
        {
            Root->SetRelativeRotationWithEulerHint(FQuat::Identity, FRotator());
        }
        else
        {
            const FQuat BoneWorldRotation = ExtractRotationNoScale(BoneWorldMatrix);
            Root->SetRelativeRotationWithEulerHint(BoneWorldRotation, BoneWorldRotation.ToRotator());
        }
        Root->SetRelativeScale(OwnerViewer
                                   ? OwnerViewer->GetCachedBoneComponentScale(BoneIndex)
                                   : BoneWorldMatrix.GetScale());

        LastGizmoComponentRotation = Root->GetRelativeRotation().ToQuaternion();
        LastGizmoComponentRotation.Normalize();
        bHasLastGizmoComponentRotation = true;
    }

    Gizmo->SetVisibility(true);
    Gizmo->UpdateGizmoTransform();
}

void FSkeletalPreviewViewportClient::ApplyBoneGizmoToSelection()
{
    if (!OwnerViewer)
    {
        return;
    }

    AActor* TargetActor = OwnerViewer->GetBoneGizmoTargetActor();
    USkeletalMeshComponent* MeshComp = OwnerViewer->GetPreviewMeshComponent();
    if (!TargetActor || !MeshComp)
    {
        return;
    }

    USceneComponent* TargetRoot = TargetActor->GetRootComponent();
    if (!TargetRoot)
    {
        return;
    }

    const int32 BoneIndex = Options.SelectedBoneIndex;
    if (BoneIndex < 0 || BoneIndex >= MeshComp->GetNumBones())
    {
        return;
    }

    FTransform CachedTransform;
    if (!OwnerViewer->GetCachedBoneLocalTransform(BoneIndex, CachedTransform))
    {
        return;
    }

    const FBoneInfo* BoneInfo = MeshComp->GetBoneInfo(BoneIndex);
    const int32 ParentIndex = BoneInfo ? BoneInfo->ParentIndex : -1;

    UGizmoComponent* Gizmo = OwnerViewer->GetBoneGizmo();
    const EGizmoMode GizmoMode = Gizmo ? Gizmo->GetMode() : EGizmoMode::Translate;

    if (GizmoMode == EGizmoMode::Translate)
    {
        const FVector DebugWorldPosition = TargetRoot->GetWorldLocation();
        const FVector BoneComponentPosition =
            DebugWorldToBoneComponentPosition(
                MeshComp,
                BoneIndex,
                DebugWorldPosition);

        if (ParentIndex >= 0 && ParentIndex < MeshComp->GetNumBones())
        {
            const FMatrix ParentComponentInverse =
                MeshComp->GetBoneComponentMatrix(ParentIndex).GetInverse();

            CachedTransform.Location =
                ParentComponentInverse.TransformPositionWithW(BoneComponentPosition);
        }
        else
        {
            CachedTransform.Location = BoneComponentPosition;
        }
    }
    else if (GizmoMode == EGizmoMode::Rotate)
    {
        if (!Gizmo || Gizmo->GetSelectedAxis() < 0)
        {
            return;
        }

        FQuat NewComponentRotation = TargetRoot->GetRelativeRotation().ToQuaternion();
        NewComponentRotation.Normalize();

        if (!bHasLastGizmoComponentRotation)
        {
            LastGizmoComponentRotation = NewComponentRotation;
            bHasLastGizmoComponentRotation = true;
            return;
        }

        FQuat DeltaComponentRotation =
            NewComponentRotation * LastGizmoComponentRotation.Inverse();
        DeltaComponentRotation.Normalize();

        FQuat ParentComponentRotation = FQuat::Identity;
        if (ParentIndex >= 0 && ParentIndex < MeshComp->GetNumBones())
        {
            ParentComponentRotation =
                ExtractRotationNoScale(MeshComp->GetBoneComponentMatrix(ParentIndex));
            ParentComponentRotation.Normalize();
        }

        FQuat DeltaLocalRotation =
            ParentComponentRotation.Inverse() * DeltaComponentRotation * ParentComponentRotation;
        DeltaLocalRotation.Normalize();

        CachedTransform.Rotation = DeltaLocalRotation * CachedTransform.Rotation;
        CachedTransform.Rotation.Normalize();

        LastGizmoComponentRotation = NewComponentRotation;
    }
    else if (GizmoMode == EGizmoMode::Scale)
    {
        CachedTransform.Scale = TargetRoot->GetRelativeScale();

        if (ParentIndex >= 0 && ParentIndex < MeshComp->GetNumBones())
        {
            const FVector ParentScale = OwnerViewer->GetCachedBoneComponentScale(ParentIndex);
            const FVector ComponentScale = TargetRoot->GetRelativeScale();
            CachedTransform.Scale = FVector(
                SafeDiv(ComponentScale.X, ParentScale.X),
                SafeDiv(ComponentScale.Y, ParentScale.Y),
                SafeDiv(ComponentScale.Z, ParentScale.Z));
        }
    }

    OwnerViewer->SetCachedBoneLocalTransform(BoneIndex, CachedTransform, true);
}
