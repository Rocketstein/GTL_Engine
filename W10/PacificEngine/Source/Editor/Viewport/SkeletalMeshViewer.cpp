#include "SkeletalMeshViewer.h"

#include "Component/GizmoComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Input/SkelViewerViewportInputController.h"

#include "Mesh/SkeletalMesh.h"
#include "Mesh/Skeleton.h"

#include <algorithm>
#include <cfloat>

namespace
{
float DistanceRaySegmentSq(
    const FVector& RayOrigin,
    const FVector& RayDir,
    const FVector& SegmentStart,
    const FVector& SegmentEnd,
    float& OutRayT,
    float& OutSegmentT)
{
    const FVector Segment = SegmentEnd - SegmentStart;
    const FVector Diff = RayOrigin - SegmentStart;

    const float SegmentLenSq = Segment.LengthSquared();
    if (SegmentLenSq <= 1e-6f)
    {
        OutSegmentT = 0.0f;
        const float PointRayT = -(Diff.Dot(RayDir));
        OutRayT = PointRayT > 0.0f ? PointRayT : 0.0f;
        const FVector ClosestRayPoint = RayOrigin + RayDir * OutRayT;
        return FVector::DistSquared(ClosestRayPoint, SegmentStart);
    }

    const float RaySegmentDot = RayDir.Dot(Segment);
    const float RayDiffDot = RayDir.Dot(Diff);
    const float SegmentDiffDot = Segment.Dot(Diff);
    const float Denominator = SegmentLenSq - RaySegmentDot * RaySegmentDot;

    if (Denominator > 1e-6f)
    {
        OutSegmentT = std::clamp(
            (SegmentDiffDot - RaySegmentDot * RayDiffDot) / Denominator, // +s_correct
            0.0f, 1.0f);
    }
    else
    {
        OutSegmentT = std::clamp(-SegmentDiffDot / SegmentLenSq, 0.0f, 1.0f);
    }

    const float ClosestRayT = OutSegmentT * RaySegmentDot - RayDiffDot;
    OutRayT = ClosestRayT > 0.0f ? ClosestRayT : 0.0f;

    const FVector ClosestRayPoint = RayOrigin + RayDir * OutRayT;
    const FVector ClosestSegmentPoint = SegmentStart + Segment * OutSegmentT;
    return FVector::DistSquared(ClosestRayPoint, ClosestSegmentPoint);
}
}

FSkeletalMeshViewer::FSkeletalMeshViewer() = default;

FSkeletalMeshViewer::~FSkeletalMeshViewer() = default;

void FSkeletalMeshViewer::Initialize(uint32 InEditorId, UEditorEngine* InEditorEngine, ID3D11Device* InDevice, USkeletalMesh* InSkeletalMesh)
{
    bOpen = true;
	ViewerID = InEditorId;
    ViewerState.reset();

    ViewerScene.Initialize(
        InEditorEngine,
        FName("SkeletalMeshPreview_" + std::to_string(InEditorId)));
    PreviewMeshComponent = ViewerScene.CreateSkeletalMeshPreview();
    BoneGizmo = ViewerScene.CreateGizmo(BoneGizmoTargetActor);

    ViewerPanel.Initialize(InEditorEngine, this);
    SetSkeletalMesh(InSkeletalMesh);
	
    ViewportClient.Initialize(InEditorEngine, InDevice, &ViewerScene, this);
    FocusCameraOnPreviewMesh();

    InputController = std::make_unique<FSkelViewerViewportInputController>(
        this,
        &ViewportClient);
}

void FSkeletalMeshViewer::Release() 
{
    bOpen = false;
    InputController.reset();
    ViewerPanel.Release();
	ViewportClient.Release();
	ViewerScene.Release();

    PreviewMeshComponent = nullptr;
    BoneGizmoTargetActor = nullptr;
    BoneGizmo = nullptr;
}
void FSkeletalMeshViewer::Tick(float DeltaTime) 
{
    ViewerScene.Tick(DeltaTime);
}

void FSkeletalMeshViewer::Render(float DeltaTime)
{
    ViewerPanel.Render(DeltaTime);
}

bool FSkeletalMeshViewer::HandleInput(float DeltaTime)
{
    return InputController ? InputController->HandleInput(DeltaTime) : false;
}

USkeletalMesh* FSkeletalMeshViewer::GetSkeletalMesh()
{
    return ViewerState.ActiveMesh ? ViewerState.ActiveMesh : nullptr;
}

void FSkeletalMeshViewer::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    ViewerState.ActiveMesh = InSkeletalMesh;
    ViewerState.SelectedBoneIndex = -1;

    if (PreviewMeshComponent)
    {
        PreviewMeshComponent->SetSkeletalMesh(InSkeletalMesh);
        PreviewMeshComponent->ResetToReferencePose();
    }

    BuildBoneHierarchy();
    ViewerPanel.SetSkelMesh();
    FocusCameraOnPreviewMesh();
}

void FSkeletalMeshViewer::FocusCameraOnPreviewMesh()
{
    if (!PreviewMeshComponent || !PreviewMeshComponent->GetSkeletalMesh())
    {
        return;
    }

    // Preview 공간에서는 asset 원본 vertex offset보다 화면 기준 중심 정렬이 더 중요하다.
    // 먼저 원점 배치로 bounds를 구한 뒤, bounds center가 원점에 오도록 preview mesh만 보정한다.
    //PreviewMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    PreviewMeshComponent->UpdateWorldMatrix();

    const FBoundingBox InitialBounds = PreviewMeshComponent->GetWorldBoundingBox();
    if (!InitialBounds.IsValid())
    {
        return;
    }

    PreviewMeshComponent->SetRelativeLocation(InitialBounds.GetCenter() * -1.0f);
    PreviewMeshComponent->UpdateWorldMatrix();

    ViewportClient.FocusOnBounds(PreviewMeshComponent->GetWorldBoundingBox(), 25.0f);
}

void FSkeletalMeshViewer::BuildBoneHierarchy()
{

    ViewerState.BoneHierarchy.clear();

    if (!ViewerState.ActiveMesh || !ViewerState.ActiveMesh->GetSkeleton())
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = ViewerState.ActiveMesh->GetSkeleton()->GetBones();
    ViewerState.BoneHierarchy.resize(Bones.size());

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        if (ParentIndex < 0)
        {
            continue;
        }

        if (ParentIndex < static_cast<int32>(Bones.size()))
        {
            ViewerState.BoneHierarchy[ParentIndex].push_back(BoneIndex);
        }
    }
}

bool FSkeletalMeshViewer::GetCachedBoneLocalTransform(int32 BoneIndex, FTransform& OutTransform)
{
    return ViewerPanel.GetCachedBoneLocalTransform(BoneIndex, OutTransform);
}

bool FSkeletalMeshViewer::SetCachedBoneLocalTransform(
    int32 BoneIndex,
    const FTransform& NewTransform,
    bool bApplyToComponent)
{
    return ViewerPanel.SetCachedBoneLocalTransform(BoneIndex, NewTransform, bApplyToComponent);
}

FQuat FSkeletalMeshViewer::GetCachedBoneComponentRotation(int32 BoneIndex)
{
    return ViewerPanel.GetCachedBoneComponentRotation(BoneIndex);
}

FVector FSkeletalMeshViewer::GetCachedBoneComponentScale(int32 BoneIndex)
{
    return ViewerPanel.GetCachedBoneComponentScale(BoneIndex);
}

void FSkeletalMeshViewer::ClearBoneSelection()
{
    ViewerState.SelectedBoneIndex = -1;
}

void FSkeletalMeshViewer::SelectBone(int32 BoneIndex)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(ViewerState.BoneHierarchy.size()))
    {
        ClearBoneSelection();
        return;
    }

    if (ViewerState.SelectedBoneIndex == BoneIndex)
    {
        return;
    }

    ViewerState.SelectedBoneIndex = BoneIndex;
}

bool FSkeletalMeshViewer::RaycastBonePicking(const FRay& Ray, int32& BestBoneIndex)
{
    BestBoneIndex = -1;

    USkeletalMeshComponent* MeshComp = PreviewMeshComponent;
    if (!MeshComp)
        return false;

    const int32 BoneCount = MeshComp->GetNumBones();
    if (BoneCount <= 0)
        return false;

    if (Ray.Direction.LengthSquared() <= EPSILON)
        return false;

    constexpr float PickRadius = 0.08f;
    constexpr float PickRadiusSq = PickRadius * PickRadius;

    const FVector RayOrigin = Ray.Origin;
    const FVector RayDir = Ray.Direction.Normalized();

    float BestDistance = FLT_MAX;
    bool bHit = false;

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FBoneInfo* BoneInfo = MeshComp->GetBoneInfo(BoneIndex);
        if (!BoneInfo)
            continue;

        const int32 ParentIndex = BoneInfo->ParentIndex;

        // Root는 parent-child bone segment가 없으므로 제외
        if (ParentIndex < 0 || ParentIndex >= BoneCount)
            continue;

        const FVector ParentPos =
            MeshComp->GetBoneDebugWorldMatrix(ParentIndex).GetLocation();

        const FVector BonePos =
            MeshComp->GetBoneDebugWorldMatrix(BoneIndex).GetLocation();

        float RayT = 0.0f;
        float SegmentT = 0.0f;

        const float DistSq = DistanceRaySegmentSq(
            RayOrigin,
            RayDir,
            ParentPos,
            BonePos,
            RayT,
            SegmentT);

        if (RayT >= 0.0f && DistSq <= PickRadiusSq)
        {
            if (RayT < BestDistance)
            {
                BestDistance = RayT;
                BestBoneIndex = ParentIndex;
                bHit = true;
            }
        }
    }

    return bHit;
}

void FSkeletalMeshViewer::BeginInputFrame()
{
    if (InputController)
    {
        InputController->BeginInputFrame();
    }
}

bool FSkeletalMeshViewer::InputKey(const FViewportKeyEvent& Event)
{
    return InputController ? InputController->InputKey(Event) : false;
}

bool FSkeletalMeshViewer::InputAxis(const FViewportAxisEvent& Event)
{
    return InputController ? InputController->InputAxis(Event) : false;
}

bool FSkeletalMeshViewer::InputPointer(const FViewportPointerEvent& Event)
{
    return InputController ? InputController->InputPointer(Event) : false;
}

void FSkeletalMeshViewer::ResetInputState()
{
    if (InputController)
    {
        InputController->ResetInputState();
    }
}

void FSkeletalMeshViewer::ResetKeyboardInputState()
{
    if (InputController)
    {
        InputController->ResetKeyboardInputState();
    }
}
