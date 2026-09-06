#include "Transform.h"
#include "../Matrix.h"
#include "../Quat.h"
#include "Rotator.h"


const FTransform FTransform::Identity(FQuat::Identity, FVector3::ZeroVector, FVector3::OneVector);

FTransform::FTransform(const FMatrix &InMatrix) noexcept
    : Rotation(FQuat::Identity), Translation(FVector3::ZeroVector), Scale3D(FVector3::OneVector)
{
    FMatrix RotationMatrix = FMatrix::Identity;
    if (InMatrix.Decompose(Translation, RotationMatrix, Scale3D))
    {
        Rotation = FQuat(RotationMatrix).GetNormalized();
    }
    else
    {
        Translation = InMatrix.GetTranslation();
        Rotation = FQuat::Identity;
        Scale3D = FVector3::OneVector;
    }
}

const FVector3 &FTransform::GetLocation() const noexcept { return Translation; }

const FVector3 &FTransform::GetTranslation() const noexcept { return Translation; }

const FQuat &FTransform::GetRotation() const noexcept { return Rotation; }

const FVector3 &FTransform::GetScale3D() const noexcept { return Scale3D; }

void FTransform::SetLocation(const FVector3 &InTranslation) noexcept
{
    Translation = InTranslation;
}

void FTransform::SetTranslation(const FVector3 &InTranslation) noexcept
{
    Translation = InTranslation;
}

void FTransform::SetRotation(const FQuat &InRotation) noexcept
{
    Rotation = InRotation.GetNormalized();
}

void FTransform::SetRotation(const FRotator &InRotation) noexcept
{
    Rotation = InRotation.Quaternion();
}

void FTransform::SetScale3D(const FVector3 &InScale3D) noexcept { Scale3D = InScale3D; }

void FTransform::SetIdentity() noexcept
{
    Rotation = FQuat::Identity;
    Translation = FVector3::ZeroVector;
    Scale3D = FVector3::OneVector;
}

FRotator FTransform::Rotator() const noexcept { return Rotation.Rotator(); }

void FTransform::NormalizeRotation() noexcept { Rotation.Normalize(); }

bool FTransform::Equals(const FTransform &Other, float Tolerance) const noexcept
{
    return Translation.Equals(Other.Translation, Tolerance) &&
           Rotation.Equals(Other.Rotation, Tolerance) && Scale3D.Equals(Other.Scale3D, Tolerance);
}

bool FTransform::IsIdentity(float Tolerance) const noexcept { return Equals(Identity, Tolerance); }

void FTransform::AddToTranslation(const FVector3 &DeltaTranslation) noexcept
{
    Translation += DeltaTranslation;
}

FVector3 FTransform::TransformPosition(const FVector3 &InPosition) const noexcept
{
    return Rotation.RotateVector(ComponentMultiply(InPosition, Scale3D)) + Translation;
}

FVector3 FTransform::TransformPositionNoScale(const FVector3 &InPosition) const noexcept
{
    return Rotation.RotateVector(InPosition) + Translation;
}

FVector3 FTransform::TransformVector(const FVector3 &InVector) const noexcept
{
    return Rotation.RotateVector(ComponentMultiply(InVector, Scale3D));
}

FVector3 FTransform::TransformVectorNoScale(const FVector3 &InVector) const noexcept
{
    return Rotation.RotateVector(InVector);
}

FVector3 FTransform::InverseTransformPosition(const FVector3 &InPosition) const noexcept
{
    const FVector3 Untranslated = InPosition - Translation;
    const FVector3 Unrotated = Rotation.UnrotateVector(Untranslated);
    return ComponentDivideSafe(Unrotated, Scale3D);
}

FVector3 FTransform::InverseTransformPositionNoScale(const FVector3 &InPosition) const noexcept
{
    return Rotation.UnrotateVector(InPosition - Translation);
}

FVector3 FTransform::InverseTransformVector(const FVector3 &InVector) const noexcept
{
    return ComponentDivideSafe(Rotation.UnrotateVector(InVector), Scale3D);
}

FVector3 FTransform::InverseTransformVectorNoScale(const FVector3 &InVector) const noexcept
{
    return Rotation.UnrotateVector(InVector);
}

FVector3 FTransform::GetUnitAxis(EAxis Axis) const noexcept
{
    return GetScaledAxis(Axis).GetSafeNormal();
}

FVector3 FTransform::GetScaledAxis(EAxis Axis) const noexcept
{
    switch (Axis)
    {
    case EAxis::X:
        return Rotation.RotateVector(FVector3(Scale3D.X, 0.0f, 0.0f));
    case EAxis::Y:
        return Rotation.RotateVector(FVector3(0.0f, Scale3D.Y, 0.0f));
    case EAxis::Z:
        return Rotation.RotateVector(FVector3(0.0f, 0.0f, Scale3D.Z));
    default:
        return FVector3::ZeroVector;
    }
}

FMatrix FTransform::ToMatrixNoScale() const noexcept
{
    return FMatrix::MakeWorld(Translation, Rotation.ToMatrix(), FVector3::OneVector);
}

FMatrix FTransform::ToMatrixWithScale() const noexcept
{
    return FMatrix::MakeWorld(Translation, Rotation.ToMatrix(), Scale3D);
}

FMatrix FTransform::ToInverseMatrixWithScale() const noexcept
{
    return ToMatrixWithScale().GetInverse();
}

FMatrix FTransform::ToMatrix() const noexcept { return ToMatrixWithScale(); }

FTransform FTransform::Inverse() const noexcept
{
    const FVector3 InverseScale3D = GetSafeScaleReciprocal(Scale3D);
    const FQuat    InverseRotation = Rotation.Inverse();
    const FVector3 InverseTranslation =
        InverseRotation.RotateVector(ComponentMultiply(-Translation, InverseScale3D));

    return FTransform(InverseRotation, InverseTranslation, InverseScale3D);
}

FTransform FTransform::operator*(const FTransform &Other) const noexcept
{
    const FVector3 ResultScale3D = ComponentMultiply(Scale3D, Other.Scale3D);
    const FQuat    ResultRotation = Rotation * Other.Rotation;
    const FVector3 ResultTranslation = Other.TransformPosition(Translation);

    return FTransform(ResultRotation, ResultTranslation, ResultScale3D);
}

FTransform &FTransform::operator*=(const FTransform &Other) noexcept
{
    *this = *this * Other;
    return *this;
}

FVector3 FTransform::ComponentMultiply(const FVector3 &A, const FVector3 &B) noexcept
{
    FVector3 Result;
    FDXMathBridge::StoreVector(Result, DirectX::XMVectorMultiply(A.ToXMVector(), B.ToXMVector()));
    return Result;
}

FVector3 FTransform::ComponentDivideSafe(const FVector3 &A, const FVector3 &B,
                                         float Tolerance) noexcept
{
    const DirectX::XMVECTOR Numerator = A.ToXMVector();
    const DirectX::XMVECTOR Denominator = B.ToXMVector();
    const DirectX::XMVECTOR SafeMask = DirectX::XMVectorGreater(
        DirectX::XMVectorAbs(Denominator), DirectX::XMVectorReplicate(Tolerance));
    const DirectX::XMVECTOR Quotient = DirectX::XMVectorDivide(Numerator, Denominator);
    FVector3                Result;
    FDXMathBridge::StoreVector(
        Result, DirectX::XMVectorSelect(DirectX::XMVectorZero(), Quotient, SafeMask));
    return Result;
}

FVector3 FTransform::GetSafeScaleReciprocal(const FVector3 &InScale, float Tolerance) noexcept
{
    return ComponentDivideSafe(FVector3::OneVector, InScale, Tolerance);
}
