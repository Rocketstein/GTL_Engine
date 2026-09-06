#pragma once

#include "../Matrix.h"
#include "../Quat.h"
#include "Rotator.h"

struct FTransform
{
  public:
    static const FTransform Identity;

    FTransform() noexcept = default;

    explicit FTransform(const FQuat &InRotation) noexcept : Rotation(InRotation.GetNormalized()) {}

    explicit FTransform(const FRotator &InRotation) noexcept : Rotation(InRotation.Quaternion()) {}

    FTransform(const FQuat &InRotation, const FVector3 &InTranslation,
               const FVector3 &InScale3D = FVector3::OneVector) noexcept
        : Rotation(InRotation.GetNormalized()), Translation(InTranslation), Scale3D(InScale3D)
    {
    }

    FTransform(const FRotator &InRotation, const FVector3 &InTranslation,
               const FVector3 &InScale3D = FVector3::OneVector) noexcept
        : Rotation(InRotation.Quaternion()), Translation(InTranslation), Scale3D(InScale3D)
    {
    }

    explicit FTransform(const FMatrix &InMatrix) noexcept;

    const FVector3 &GetLocation() const noexcept;
    const FVector3 &GetTranslation() const noexcept;
    const FQuat    &GetRotation() const noexcept;
    const FVector3 &GetScale3D() const noexcept;

    void SetLocation(const FVector3 &InTranslation) noexcept;
    void SetTranslation(const FVector3 &InTranslation) noexcept;
    void SetRotation(const FQuat &InRotation) noexcept;
    void SetRotation(const FRotator &InRotation) noexcept;
    void SetScale3D(const FVector3 &InScale3D) noexcept;
    void SetIdentity() noexcept;

    FRotator Rotator() const noexcept;
    void     NormalizeRotation() noexcept;
    bool     Equals(const FTransform &Other, float Tolerance = 1.e-6f) const noexcept;
    bool     IsIdentity(float Tolerance = 1.e-6f) const noexcept;
    void     AddToTranslation(const FVector3 &DeltaTranslation) noexcept;

    FVector3 TransformPosition(const FVector3 &InPosition) const noexcept;
    FVector3 TransformPositionNoScale(const FVector3 &InPosition) const noexcept;
    FVector3 TransformVector(const FVector3 &InVector) const noexcept;
    FVector3 TransformVectorNoScale(const FVector3 &InVector) const noexcept;

    FVector3 InverseTransformPosition(const FVector3 &InPosition) const noexcept;
    FVector3 InverseTransformPositionNoScale(const FVector3 &InPosition) const noexcept;
    FVector3 InverseTransformVector(const FVector3 &InVector) const noexcept;
    FVector3 InverseTransformVectorNoScale(const FVector3 &InVector) const noexcept;

    FVector3 GetUnitAxis(EAxis Axis) const noexcept;
    FVector3 GetScaledAxis(EAxis Axis) const noexcept;

    FMatrix    ToMatrixNoScale() const noexcept;
    FMatrix    ToMatrixWithScale() const noexcept;
    FMatrix    ToInverseMatrixWithScale() const noexcept;
    FMatrix    ToMatrix() const noexcept;
    FTransform Inverse() const noexcept;

    FTransform  operator*(const FTransform &Other) const noexcept;
    FTransform &operator*=(const FTransform &Other) noexcept;

  private:
    static FVector3 ComponentMultiply(const FVector3 &A, const FVector3 &B) noexcept;
    static FVector3 ComponentDivideSafe(const FVector3 &A, const FVector3 &B,
                                        float Tolerance = 1.e-8f) noexcept;
    static FVector3 GetSafeScaleReciprocal(const FVector3 &InScale,
                                           float           Tolerance = 1.e-8f) noexcept;

  private:
    FQuat    Rotation = FQuat::Identity;
    FVector3 Translation = FVector3::ZeroVector;
    FVector3 Scale3D = FVector3::OneVector;
};
