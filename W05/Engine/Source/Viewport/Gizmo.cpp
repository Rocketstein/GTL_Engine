#include "Gizmo.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Geometry/Intersection.h"
#include "Core/Math/MathUtility.h"
#include <cfloat>
#include <cmath>

UGizmo::UGizmo() {}

// ---------------------------------------------------------------------------
//  Internal helpers
// ---------------------------------------------------------------------------

namespace
{
    constexpr float MT_EPSILON = 1e-8f;

    // Scalar Möller-Trumbore for a single triangle.
    // Ray is expected to be in the same space as the triangle vertices.
    static bool MollerTrumboreSingle(const FVector3 &O, const FVector3 &D,
                                     const Geometry::FTriangle &Tri, float &OutT)
    {
        const FVector3 E1 = Tri.V1 - Tri.V0;
        const FVector3 E2 = Tri.V2 - Tri.V0;
        const FVector3 H  = FVector3::CrossProduct(D, E2);
        const float    A  = FVector3::DotProduct(E1, H);

        if (std::fabs(A) < MT_EPSILON) return false;

        const float    F  = 1.f / A;
        const FVector3 S  = O - Tri.V0;
        const float    U  = F * FVector3::DotProduct(S, H);
        if (U < 0.f || U > 1.f) return false;

        const FVector3 Q = FVector3::CrossProduct(S, E1);
        const float    V = F * FVector3::DotProduct(D, Q);
        if (V < 0.f || U + V > 1.f) return false;

        const float T = F * FVector3::DotProduct(E2, Q);
        if (T < MT_EPSILON) return false;

        OutT = T;
        return true;
    }

} // anonymous namespace

FVector3 UGizmo::GetAxisVector() const 
{ 
    FVector3 AxisVector;
    switch (SelectedAxis)
    {
    case (0):
        AxisVector = GizmoTransform.GetUnitAxis(EAxis::X);
        break;
    case (1):
        AxisVector = GizmoTransform.GetUnitAxis(EAxis::Y);
        break;
    case (2):
        AxisVector = GizmoTransform.GetUnitAxis(EAxis::Z);
        break;
    }
    return AxisVector;
}

void UGizmo::Flush() { 
    Deactivate();
    Target = nullptr;
}

// ---------------------------------------------------------------------------
//  Hover / axis state
// ---------------------------------------------------------------------------
void UGizmo::UpdateHoveredAxis(int Index)
{
    if (!bIsHolding)
        SelectedAxis = Index;
}

void UGizmo::SetPickMesh(int ModeIdx, TArray<FGizmoPickVertex> InVerts, TArray<uint32> InIndices)
{
    if (ModeIdx < 0 || ModeIdx >= 3) return;
    PickVerts[ModeIdx]   = std::move(InVerts);
    PickIndices[ModeIdx] = std::move(InIndices);
}

int UGizmo::PickAxis(const FRay &WorldRay) const
{
    const int ModeIdx = static_cast<int>(GizmoType);
    const TArray<FGizmoPickVertex> &Verts   = PickVerts[ModeIdx];
    const TArray<uint32>           &Indices = PickIndices[ModeIdx];
    if (Verts.empty() || Indices.empty()) return -1;

    const FMatrix  InvWorld     = GetWorldMatrix().GetInverse();
    const FVector3 LocalOrigin  = InvWorld.TransformPosition(WorldRay.Origin);
    const FVector3 LocalDir     = InvWorld.TransformVector(WorldRay.Direction).GetSafeNormal();

    float BestT    = FLT_MAX;
    int   BestAxis = -1;

    const int32 NumTris = static_cast<int32>(Indices.size()) / 3;
    for (int32 i = 0; i < NumTris; ++i)
    {
        const FGizmoPickVertex &V0 = Verts[Indices[i * 3 + 0]];
        const FGizmoPickVertex &V1 = Verts[Indices[i * 3 + 1]];
        const FGizmoPickVertex &V2 = Verts[Indices[i * 3 + 2]];

        float T;
        if (!MollerTrumboreSingle(LocalOrigin, LocalDir,
                                  {V0.Position, V1.Position, V2.Position}, T))
            continue;

        if (T >= BestT) continue;
        BestT    = T;
        BestAxis = V0.AxisIndex;
    }

    return BestAxis;
}

// ---------------------------------------------------------------------------
//  Mode cycling
// ---------------------------------------------------------------------------
void UGizmo::SetNextMode()
{
    switch (GizmoType)
    {
    case EGizmoType::Translation: GizmoType = EGizmoType::Rotation;   break;
    case EGizmoType::Rotation:    GizmoType = EGizmoType::Scale;       break;
    case EGizmoType::Scale:       GizmoType = EGizmoType::Translation; break;
    }
}

// ---------------------------------------------------------------------------
//  Target transform — positions the gizmo to follow the selected object
// ---------------------------------------------------------------------------
void UGizmo::SetTargetLocation(FVector3 NewLocation)
{
    GizmoTransform.SetLocation(NewLocation);
}

void UGizmo::SetTargetRotation(FVector3 NewRotation)
{
    GizmoTransform.SetRotation(FRotator::MakeFromEuler(NewRotation));
}

void UGizmo::SetTargetScale(FVector3 NewScale)
{
    GizmoTransform.SetScale3D(NewScale);
}

void UGizmo::SetTarget(UPrimitiveComponent* NewTarget) 
{
    if (NewTarget)
    {
        Activate();
        Target = NewTarget;
        GizmoTransform.SetLocation(Target->GetRelativeLocation());
        ResetSelectedAxis();
        UE_LOG(Gizmo, ELogLevel::Debug,
               "SetTarget: UUID=%u, type=%s, location=(%.3f,%.3f,%.3f)",
               NewTarget->UUID, NewTarget->GetTypeName(), GizmoTransform.GetLocation().X,
               GizmoTransform.GetLocation().Y, GizmoTransform.GetLocation().Z);
    }
}

// ---------------------------------------------------------------------------
//  Screen-space constant-size scaling
// ---------------------------------------------------------------------------
void UGizmo::ApplyScreenSpaceScaling(const FVector3 &CameraLocation)
{
    constexpr float ScreenSizeConstant = 0.1f;
    constexpr float MinScreenSpaceScale = 0.05f;
    const float     Dist =
        (CameraLocation - GizmoTransform.GetLocation()).Size();
    const float UniformScale = FMath::Clamp(Dist * ScreenSizeConstant, MinScreenSpaceScale, MaxScreenSpaceScale);
    GizmoTransform.SetScale3D(FVector3(UniformScale, UniformScale, UniformScale));
}

void UGizmo::SetHoveringEnabled(bool bEnabled)
{
    bHoveringEnabled = bEnabled;
    if (!bHoveringEnabled)
    {
        ResetSelectedAxis();
    }
}

void UGizmo::SetMaxScreenSpaceScale(float InMaxScale)
{
    MaxScreenSpaceScale = FMath::Max(1.0f, InMaxScale);
}

// ---------------------------------------------------------------------------
//  World / local space toggle
// ---------------------------------------------------------------------------
void UGizmo::SetWorldSpace(bool bInWorldSpace)
{
    bWorldSpace = bInWorldSpace;
    if (bWorldSpace)
        GizmoTransform.SetRotation(FQuat::Identity);
}

// ---------------------------------------------------------------------------
//  Drag
// ---------------------------------------------------------------------------
void UGizmo::DragStart() 
{
    if (bIsActive)
    {
        bPressedOnHandle = true;
        bIsFirstFrameOfDrag = true;
    }
}

void UGizmo::UpdateDrag(const FRay& Ray)
{
    // TODO: implement axis-constrained drag
    if (SelectedAxis < 0 /*|| !IsActive || !bIsHolding*/) return;

    if (GizmoType == EGizmoType::Rotation)
    {
        // Angular Drag
        FVector3 planeNormal = GetAxisVector();
        float    Denom = FVector3::DotProduct(Ray.Direction, planeNormal);
        if (std::abs(Denom) < 1e-6f)
            return;

        FVector3 Current =
            Ray.Origin + (Ray.Direction *
                          (FVector3::DotProduct((GizmoTransform.GetLocation() - Ray.Origin), (planeNormal) / Denom)));

        if (bIsFirstFrameOfDrag)
        {
            LastIntersectionLocation = Current;
            bIsFirstFrameOfDrag = false;
            return;
        }

        FVector3 CenterToLast =
            (LastIntersectionLocation - GizmoTransform.GetLocation()).GetSafeNormal();
        FVector3 CenterToCurrent = (Current - GizmoTransform.GetLocation()).GetSafeNormal();

        float Dot   = FMath::Clamp(FVector3::DotProduct(CenterToLast, CenterToCurrent), -1.0f, 1.0f);
        float Angle = std::acos(Dot);
        float Sign  = FVector3::DotProduct(FVector3::CrossProduct(CenterToLast, CenterToCurrent), planeNormal) >= 0.0f ? 1.0f : -1.0f;

        HandleDrag(Sign * Angle);
        LastIntersectionLocation = Current;
    }
    else
    {
        // Linear Drag
        FVector3 AxisVector = GetAxisVector();

        FVector3 PlaneNormal = FVector3::CrossProduct(AxisVector,Ray.Direction);
        FVector3 ProjectDir  = FVector3::CrossProduct(PlaneNormal, AxisVector);

        float Denom = FVector3::DotProduct(Ray.Direction, ProjectDir);
        if (std::abs(Denom) < 1e-6f)
            return;

        FVector3 Current =
            Ray.Origin + (Ray.Direction *
                          (FVector3::DotProduct((GizmoTransform.GetLocation() - Ray.Origin), ProjectDir) / Denom));

        if (bIsFirstFrameOfDrag)
        {
            LastIntersectionLocation = Current;
            bIsFirstFrameOfDrag = false;
            return;
        }

        float DragAmount = FVector3::DotProduct((Current - LastIntersectionLocation), AxisVector);
        HandleDrag(DragAmount);
        LastIntersectionLocation = Current;
    }
}

void UGizmo::DragEnd()
{
    bPressedOnHandle = false;
}

void UGizmo::HandleDrag(float DragAmount) {
    FVector3 Delta = GetAxisVector() * DragAmount;
    if (!Target) return; // Debugging
   
    // Gizmo should NOT rotate or scale with its target
    switch (GizmoType)
    {
    case EGizmoType::Translation:
    {
        GizmoTransform.AddToTranslation(Delta);
        /*Target->GetRelativeTransform().AddToTranslation(Delta);*/
        Target->SetRelativeLocation(Target->GetRelativeLocation() + Delta);
        break;
    }
    case EGizmoType::Rotation:
    {
        FMatrix DeltaMat;
        switch (SelectedAxis)
        {
        case 0:
            DeltaMat = FMatrix::MakeRotationX(DragAmount);
            break;
        case 1:
            DeltaMat = FMatrix::MakeRotationY(DragAmount);
            break;
        case 2:
            DeltaMat = FMatrix::MakeRotationZ(DragAmount);
            break;
        }
        FMatrix CurMat = Target->GetRelativeQuaternion().ToMatrix();
        Target->SetRelativeRotation(FQuat(CurMat * DeltaMat));
        break;
    }
    case EGizmoType::Scale:
    {
        FVector3 NewScale = Target->GetRelativeScale3D();
        switch (SelectedAxis)
        {
        case 0:
            NewScale.X += DragAmount;
            break;
        case 1:
            NewScale.Y += DragAmount;
            break;
        case 2:
            NewScale.Z += DragAmount;
            break;
        }
        Target->SetRelativeScale3D(NewScale);
    }
    }
    UStaticMeshComponent
        *Comp = static_cast<UStaticMeshComponent*>(Target);
    Comp->MarkRaycastTransformDirty();

}
