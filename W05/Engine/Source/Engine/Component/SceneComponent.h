#pragma once

#include "Core/Containers/Array.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Math/Transform/Transform.h"
#include "Core/Math/Vector3.h"
#include "CoreUObject/Object.h"

class FComponentPropertyBuilder;

class USceneComponent : public UObject
{
    DECLARE_RTTI(USceneComponent, UObject)

  public:
    USceneComponent() = default;
    virtual ~USceneComponent() override;

  public:
    FVector3   GetRelativeLocation() const { return WorldTransform.GetLocation(); }
    FRotator   GetRelativeRotation() const { return WorldTransform.Rotator(); }
    FVector3   GetRelativeScale3D() const { return WorldTransform.GetScale3D(); }
    FQuat      GetRelativeQuaternion() const { return WorldTransform.GetRotation(); }
    FTransform GetRelativeTransform() const { return WorldTransform; }

    virtual void SetRelativeLocation(const FVector3 &NewLocation);
    virtual void SetRelativeRotation(const FQuat &NewRotation);
    virtual void SetRelativeRotation(const FRotator &NewRotation);
    virtual void SetRelativeScale3D(const FVector3 &NewScale);
    virtual void SetRelativeTransform(const FVector3 &NewTransform);

    virtual void Update(float DeltaTime);
    virtual bool ShouldSerializeInScene() const { return true; }
    virtual bool ShouldShowInDetailsTree() const { return true; }

    FMatrix GetRelativeMatrix() const;
    FMatrix GetRelativeMatrixNoScale() const;

    bool IsSelected() const;
    void SetSelected(bool bInSelected);

    virtual bool IsShowBounds() const { return false; };

    virtual void SetShowBounds(bool bInShowBounds) {
        // Do nothing
    };

  protected:
    virtual void OnTransformChanged() {}

  protected:
    bool bIsSelected = false;

  protected:
    FTransform WorldTransform;
};
