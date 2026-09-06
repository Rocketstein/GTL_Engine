#pragma once
#include "Core/CoreMinimal.h"
#include "Core/Math/Transform/Transform.h"
#include "Engine/Source/CoreUObject/Object.h"

using namespace Geometry;

class UStaticMeshComponent;
class UPrimitiveComponent;

// Minimal per-vertex data needed for axis picking
struct FGizmoPickVertex
{
    FVector3 Position;
    int      AxisIndex; // 0=X, 1=Y, 2=Z
};

enum class EGizmoType
{
    Translation,
    Rotation,
    Scale,
};

class UGizmo : public UObject
{
    DECLARE_RTTI(UGizmo, UObject)
  private:
    EGizmoType GizmoType            = EGizmoType::Translation;
    int32      SelectedAxis         = -1;
    bool       bIsHolding           = false;
    bool       bIsActive            = false;
    bool       bPressedOnHandle     = false;
    bool       bIsFirstFrameOfDrag  = true;
    FVector3   LastIntersectionLocation;

    UPrimitiveComponent* Target = nullptr;

    UStaticMeshComponent *MeshComponent = nullptr;
    FTransform            GizmoTransform;
    bool                  bWorldSpace = true;
    bool                  bHoveringEnabled = true;
    float                 MaxScreenSpaceScale = 500.0f;

    // CPU pick mesh — one entry per gizmo mode (Translation=0, Rotation=1, Scale=2)
    TArray<FGizmoPickVertex> PickVerts[3];
    TArray<uint32>           PickIndices[3];

  public:
    UGizmo();
    ~UGizmo() = default;
    void Flush();

    // Rendering;
    void       Activate() { bIsActive = true; }
    void       Deactivate() { bIsActive = false; GizmoType = EGizmoType::Translation;}
    bool       IsActive() const { return bIsActive; }
    void       SetGizmoType(EGizmoType Type) { GizmoType = Type; }
    EGizmoType GetPrimitiveType() const { return GizmoType; }
    FMatrix    GetWorldMatrix() const { return GizmoTransform.ToMatrixWithScale(); }

    // Hover / axis picking
    void         UpdateHoveredAxis(int Index);
    void         ResetSelectedAxis() { SelectedAxis = -1; }
    void         SetHoldingState(bool bHolding) { bIsHolding = bHolding; }
    inline int32 GetSelectedAxis() const { return SelectedAxis; }
    inline bool  IsHovered() const { return SelectedAxis != -1; }

    void SetPickMesh(int ModeIdx, TArray<FGizmoPickVertex> InVerts, TArray<uint32> InIndices);
    int  PickAxis(const FRay &WorldRay) const;
    FVector3 GetAxisVector() const;

    // Target
    void                 SetTarget(UPrimitiveComponent *NewTarget);
    bool                 HasTarget() const { return Target != nullptr; }
    UPrimitiveComponent* GetTarget() const { return Target; }

    // Mode
    void SetNextMode();
    void UpdateGizmoMode(EGizmoType NewType) { GizmoType = NewType; }
    void SetTranslateMode() { UpdateGizmoMode(EGizmoType::Translation); }
    void SetRotateMode()    { UpdateGizmoMode(EGizmoType::Rotation); }
    void SetScaleMode()     { UpdateGizmoMode(EGizmoType::Scale); }

    // Drag
    void DragStart();
    void UpdateDrag(const FRay &Ray);
    void DragEnd();
    void HandleDrag(float DragAmount);

    // Target transform (driven from UI)
    void SetTargetLocation(FVector3 NewLocation);
    void SetTargetRotation(FVector3 NewRotation);
    void SetTargetScale(FVector3 NewScale);

    // Screen-space
    void ApplyScreenSpaceScaling(const FVector3 &CameraLocation);
    void SetHoveringEnabled(bool bEnabled);
    bool IsHoveringEnabled() const { return bHoveringEnabled; }
    void SetMaxScreenSpaceScale(float InMaxScale);
    float GetMaxScreenSpaceScale() const { return MaxScreenSpaceScale; }
    void SetWorldSpace(bool bWorldSpace);

    // Hold / press state
    void SetHolding(bool bHold) { bIsHolding = bHold; }
    bool IsHolding() const { return bIsHolding; }
    void SetPressedOnHandle(bool bPressed) { bPressedOnHandle = bPressed; }
    bool IsPressedOnHandle() const { return bPressedOnHandle; }
};
