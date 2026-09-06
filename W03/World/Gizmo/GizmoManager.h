#pragma once

#include "GizmoComponent.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include <vector>

class FGizmoManager
{
public:
	enum EGizmoMode { Translate, Rotate, Scale, End };

	void Initialize(UGizmoComponent* InComponent);

	// Target
	void SetTarget(USceneComponent* NewTarget);
	void AddTarget(USceneComponent* NewTarget);

	void Deactivate();
	bool HasTarget()             const { return TargetComponent != nullptr; }
	bool IsSelected(USceneComponent* Comp) const;
	USceneComponent* GetTarget() const { return TargetComponent; }

	const std::vector<USceneComponent*>& GetSelectedTargets() const { return SelectedComponents; }

	// Mode
	void SetNextMode();
	void UpdateGizmoMode(EGizmoMode NewMode);
	void SetTranslateMode() { UpdateGizmoMode(Translate); }
	void SetRotateMode()    { UpdateGizmoMode(Rotate); }
	void SetScaleMode()     { UpdateGizmoMode(Scale); }

	// Drag
	void UpdateDrag(const FRay& Ray);
	void DragEnd();

	// Target transform (driven from UI)
	void SetTargetLocation(FVector NewLocation);
	void SetTargetRotation(FVector NewRotation);
	void SetTargetScale(FVector NewScale);

	// Screen-space
	void ApplyScreenSpaceScaling(const FVector& CameraLocation);
	void SetWorldSpace(bool bWorldSpace);
	void SetVisibility(bool bVisible);

	// Hold / press state
	void SetHolding(bool bHold);
	bool IsHolding()          const { return bIsHolding; }
	void SetPressedOnHandle(bool bPressed) { bPressedOnHandle = bPressed; }
	bool IsPressedOnHandle()  const { return bPressedOnHandle; }

	// Component access (for rendering / raycasting)
	UGizmoComponent* GetComponent() const { return Component; }

private:
	UGizmoComponent* Component = nullptr;

	USceneComponent*              TargetComponent    = nullptr;
	std::vector<USceneComponent*> SelectedComponents;

	EGizmoMode       CurMode              = Translate;
	FVector          LastIntersectionLocation;

	const float axisLength        = 1.0f;
	const float Radius            = 0.1f;
	const float ScaleSensitivity  = 1.0f;

	bool bIsFirstFrameOfDrag = true;
	bool bIsHolding          = false;
	bool bIsWorldSpace       = true;
	bool bPressedOnHandle    = false;

	FVector GetVectorForAxis(int32 Axis);
	bool    IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float& OutRayT);

	void HandleDrag(float DragAmount);
	void TranslateTarget(float DragAmount);
	void RotateTarget(float DragAmount);
	void ScaleTarget(float DragAmount);
	void UpdateLinearDrag(const FRay& Ray);
	void UpdateAngularDrag(const FRay& Ray);
	void UpdateGizmoTransform();
};
