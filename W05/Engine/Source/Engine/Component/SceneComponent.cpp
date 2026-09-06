#include "SceneComponent.h"
#include "Core/Geometry/Primitives/AABBUtility.h"
#include "Core/Math/Vector3.h"
#include <algorithm>

USceneComponent::~USceneComponent() {}

void USceneComponent::SetRelativeLocation(const FVector3 &NewLocation)
{
    if (WorldTransform.GetLocation() == NewLocation)
    {
        return;
    }

    WorldTransform.SetLocation(NewLocation);
    OnTransformChanged();
}

void USceneComponent::SetRelativeRotation(const FQuat &NewRotation)
{
    if (WorldTransform.GetRotation() == NewRotation)
    {
        return;
    }

    WorldTransform.SetRotation(NewRotation);
    OnTransformChanged();
}

void USceneComponent::SetRelativeRotation(const FRotator &NewRotation)
{
    SetRelativeRotation(NewRotation.Quaternion());
}

void USceneComponent::SetRelativeScale3D(const FVector3 &NewScale)
{
    if (WorldTransform.GetScale3D() == NewScale)
    {
        return;
    }
    FVector3 AbsScale;
    AbsScale.X = std::max(NewScale.X, 0.0001f);
    AbsScale.Y = std::max(NewScale.Y, 0.0001f);
    AbsScale.Z = std::max(NewScale.Z, 0.0001f);
    WorldTransform.SetScale3D(AbsScale);

    OnTransformChanged();
}

void USceneComponent::SetRelativeTransform(const FVector3 &NewTransform)
{
    WorldTransform.SetScale3D(NewTransform);
    OnTransformChanged();
}

void USceneComponent::Update(float DeltaTime) {}

FMatrix USceneComponent::GetRelativeMatrix() const { return WorldTransform.ToMatrix(); }

FMatrix USceneComponent::GetRelativeMatrixNoScale() const
{
    return WorldTransform.ToMatrixNoScale();
}

bool USceneComponent::IsSelected() const { return bIsSelected; }
void USceneComponent::SetSelected(bool bInSelected) { bIsSelected = bInSelected; }
