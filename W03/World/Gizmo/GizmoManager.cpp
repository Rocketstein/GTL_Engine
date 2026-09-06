#include "GizmoManager.h"
#include "Classes/AActor.h"
#include <cmath>
#include <algorithm>

void FGizmoManager::Initialize(UGizmoComponent* InComponent)
{
	Component = InComponent;
}

void FGizmoManager::SetHolding(bool bHold)
{
	bIsHolding = bHold;
	if (Component)
		Component->SetHoldingState(bHold);
}

void FGizmoManager::SetVisibility(bool bVisible)
{
	if (Component)
		Component->SetVisibility(bVisible);
}

void FGizmoManager::SetTarget(USceneComponent* NewTarget)
{
	if (!NewTarget) return;
	if (!Component) return;

	SelectedComponents.clear();
	SelectedComponents.push_back(NewTarget);
	TargetComponent = NewTarget;
	Component->SetWorldLocation(TargetComponent->GetWorldLocation());
	UpdateGizmoTransform();
	Component->SetVisibility(true);
}

void FGizmoManager::AddTarget(USceneComponent* NewTarget)
{
	if (!NewTarget) return;

	auto it = std::find(SelectedComponents.begin(), SelectedComponents.end(), NewTarget);
	if (it != SelectedComponents.end())
	{
		// 이미 선택된 경우는 비활성화
		SelectedComponents.erase(it);
		if (SelectedComponents.empty())
		{
			Deactivate();
			return;
		}
		TargetComponent = SelectedComponents.back();
		UpdateGizmoTransform();
		return;
	}

	SelectedComponents.push_back(NewTarget);
	TargetComponent = NewTarget;

	if (!Component) return;
	Component->SetWorldLocation(TargetComponent->GetWorldLocation());
	UpdateGizmoTransform();
	Component->SetVisibility(true);
}


bool FGizmoManager::IsSelected(USceneComponent* Comp) const
{
	return std::find(SelectedComponents.begin(), SelectedComponents.end(), Comp) != SelectedComponents.end();
}

void FGizmoManager::Deactivate()
{
	SelectedComponents.clear();
	TargetComponent = nullptr;
	if (Component)
		Component->Deactivate();
}

void FGizmoManager::SetNextMode()
{
	UpdateGizmoMode(static_cast<EGizmoMode>((static_cast<int>(CurMode) + 1) % EGizmoMode::End));
}

void FGizmoManager::UpdateGizmoMode(EGizmoMode NewMode)
{
	CurMode = NewMode;
	UpdateGizmoTransform();
}

void FGizmoManager::UpdateGizmoTransform()
{
	if (!Component || !TargetComponent) return;

	Component->SetWorldLocation(TargetComponent->GetWorldLocation());
	Component->UpdateMeshForMode(static_cast<int>(CurMode));

	switch (CurMode)
	{
	case Scale:
		Component->SetRelativeRotation(TargetComponent->RelativeRotation);
		break;
	case Rotate:
	case Translate:
		Component->SetRelativeRotation(bIsWorldSpace ? FVector() : TargetComponent->RelativeRotation);
		break;
	}
}

void FGizmoManager::SetTargetLocation(FVector NewLocation)
{
	if (!TargetComponent) return;
	TargetComponent->SetRelativeLocation(NewLocation);
	UpdateGizmoTransform();
}

void FGizmoManager::SetTargetRotation(FVector NewRotation)
{
	if (!TargetComponent) return;
	TargetComponent->SetRelativeRotation(NewRotation);
	UpdateGizmoTransform();
}

void FGizmoManager::SetTargetScale(FVector NewScale)
{
	if (!TargetComponent) return;
	FVector Safe = NewScale;
	if (Safe.X < 0.001f) Safe.X = 0.001f;
	if (Safe.Y < 0.001f) Safe.Y = 0.001f;
	if (Safe.Z < 0.001f) Safe.Z = 0.001f;
	TargetComponent->SetRelativeScale(Safe);
}

void FGizmoManager::ApplyScreenSpaceScaling(const FVector& CameraLocation)
{
	if (!Component) return;
	float Distance = FVector::Distance(CameraLocation, Component->GetWorldLocation());
	float NewScale = Distance * 0.1f;
	if (NewScale < 0.01f) NewScale = 0.01f;
	Component->SetRelativeScale(FVector(NewScale, NewScale, NewScale));
}

void FGizmoManager::SetWorldSpace(bool bWorldSpace)
{
	bIsWorldSpace = bWorldSpace;
	UpdateGizmoTransform();
}

FVector FGizmoManager::GetVectorForAxis(int32 Axis)
{
	if (!Component) return FVector(0, 0, 0);
	switch (Axis)
	{
	case 0: return Component->GetForwardVector();
	case 1: return Component->GetRightVector();
	case 2: return Component->GetUpVector();
	default: return FVector(0, 0, 0);
	}
}

bool FGizmoManager::IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float& OutRayT)
{
	if (!Component) return false;

	FVector axisStart  = Component->GetWorldLocation();
	FVector axisVector = AxisEnd - axisStart;
	FVector diffOrigin = Ray.Origin - axisStart;

	float RayDirDotRayDir = Ray.Direction.Dot(Ray.Direction);
	float RayDirDotAxis   = Ray.Direction.Dot(axisVector);
	float AxisDotAxis     = axisVector.Dot(axisVector);
	float RayDirDotDiff   = Ray.Direction.Dot(diffOrigin);
	float AxisDotDiff     = axisVector.Dot(diffOrigin);

	float Denominator = (RayDirDotRayDir * AxisDotAxis) - (RayDirDotAxis * RayDirDotAxis);
	float rayT, axisS;

	if (Denominator < 1e-6f)
	{
		rayT  = 0.0f;
		axisS = (AxisDotAxis > 0.0f) ? (AxisDotDiff / AxisDotAxis) : 0.0f;
	}
	else
	{
		rayT  = (RayDirDotAxis * AxisDotDiff - AxisDotAxis * RayDirDotDiff) / Denominator;
		axisS = (RayDirDotRayDir * AxisDotDiff - RayDirDotAxis * RayDirDotDiff) / Denominator;
	}

	if (rayT  < 0.0f) rayT  = 0.0f;
	if (axisS < 0.0f) axisS = 0.0f;
	else if (axisS > 1.0f) axisS = 1.0f;

	FVector closest  = Ray.Origin + (Ray.Direction * rayT);
	FVector onAxis   = axisStart  + (axisVector    * axisS);
	FVector diff     = closest - onAxis;
	float   distSq   = diff.Dot(diff);

	if (distSq < Radius * Radius)
	{
		OutRayT = rayT;
		return true;
	}
	return false;
}

void FGizmoManager::HandleDrag(float DragAmount)
{
	switch (CurMode)
	{
	case Translate: TranslateTarget(DragAmount); break;
	case Rotate:    RotateTarget(DragAmount);    break;
	case Scale:     ScaleTarget(DragAmount);     break;
	}
	UpdateGizmoTransform();
}

void FGizmoManager::TranslateTarget(float DragAmount)
{
	if (!Component || SelectedComponents.empty()) return;
	FVector delta = GetVectorForAxis(Component->GetSelectedAxis()) * DragAmount;
	Component->AddWorldOffset(delta);
	for (auto* target : SelectedComponents)
	{
		if (target)
		{
			target->AddWorldOffset(delta);
			target->GetOwningActor()->Transformed();
		}
	}
}

void FGizmoManager::RotateTarget(float DragAmount)
{
	if (!Component || SelectedComponents.empty()) return;
	FVector rotAxis     = GetVectorForAxis(Component->GetSelectedAxis());
	FMatrix deltaMatrix = FMatrix::MakeRotationAxis(rotAxis, DragAmount);
	for (auto* target : SelectedComponents)
	{
		if (target)
		{
			FMatrix curMatrix = FMatrix::MakeRotationEuler(target->RelativeRotation);
			target->SetRelativeRotation((curMatrix * deltaMatrix).GetEuler());
			target->GetOwningActor()->Transformed();
		}
	}
}

void FGizmoManager::ScaleTarget(float DragAmount)
{
	if (!Component || SelectedComponents.empty()) return;
	float scaleDelta = DragAmount * ScaleSensitivity;
	int axis = Component->GetSelectedAxis();
	for (auto* target : SelectedComponents)
	{
		if (!target) continue;
		FVector NewScale = target->RelativeScale3D;
		switch (axis)
		{
		case 0: NewScale.X += scaleDelta; break;
		case 1: NewScale.Y += scaleDelta; break;
		case 2: NewScale.Z += scaleDelta; break;
		}
		target->SetRelativeScale(NewScale);
		target->GetOwningActor()->Transformed();
	}
}

void FGizmoManager::UpdateLinearDrag(const FRay& Ray)
{
	if (!Component) return;

	FVector axisVector = GetVectorForAxis(Component->GetSelectedAxis());
	FVector planeNormal = axisVector.Cross(Ray.Direction);
	FVector projectDir  = planeNormal.Cross(axisVector);

	float Denom = Ray.Direction.Dot(projectDir);
	if (std::abs(Denom) < 1e-6f) return;

	FVector Current = Ray.Origin + (Ray.Direction * ((Component->GetWorldLocation() - Ray.Origin).Dot(projectDir) / Denom));

	if (bIsFirstFrameOfDrag) { LastIntersectionLocation = Current; bIsFirstFrameOfDrag = false; return; }

	float DragAmount = (Current - LastIntersectionLocation).Dot(axisVector);
	HandleDrag(DragAmount);
	LastIntersectionLocation = Current;
}

void FGizmoManager::UpdateAngularDrag(const FRay& Ray)
{
	if (!Component) return;

	FVector planeNormal = GetVectorForAxis(Component->GetSelectedAxis());
	float Denom = Ray.Direction.Dot(planeNormal);
	if (std::abs(Denom) < 1e-6f) return;

	FVector Current = Ray.Origin + (Ray.Direction * ((Component->GetWorldLocation() - Ray.Origin).Dot(planeNormal) / Denom));

	if (bIsFirstFrameOfDrag) { LastIntersectionLocation = Current; bIsFirstFrameOfDrag = false; return; }

	FVector CenterToLast    = (LastIntersectionLocation - Component->GetWorldLocation()).Normalized();
	FVector CenterToCurrent = (Current                  - Component->GetWorldLocation()).Normalized();

	float Dot   = Clamp(CenterToLast.Dot(CenterToCurrent), -1.0f, 1.0f);
	float Angle = std::acos(Dot);
	float Sign  = (CenterToLast.Cross(CenterToCurrent).Dot(planeNormal) >= 0.0f) ? 1.0f : -1.0f;

	HandleDrag(Sign * Angle);
	LastIntersectionLocation = Current;
}

void FGizmoManager::UpdateDrag(const FRay& Ray)
{
	if (!bIsHolding || !Component || !Component->IsActive()) return;
	if (Component->GetSelectedAxis() == -1 || SelectedComponents.empty()) return;

	if (CurMode == Rotate)
		UpdateAngularDrag(Ray);
	else
		UpdateLinearDrag(Ray);
}

void FGizmoManager::DragEnd()
{
	bIsFirstFrameOfDrag = true;
	SetHolding(false);
}
