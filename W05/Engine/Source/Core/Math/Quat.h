#pragma once

#include "Vector3.h"
#include <cstdint>


struct FMatrix;
struct FRotator;

struct FQuat
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float W = 1.0f;

    static const FQuat Identity;

    constexpr FQuat() noexcept = default;

    constexpr FQuat(float InX, float InY, float InZ, float InW) noexcept
        : X(InX), Y(InY), Z(InZ), W(InW)
    {
    }

    explicit FQuat(const FRotator &InRotator) noexcept;
    explicit FQuat(const FMatrix &InMatrix) noexcept;
    FQuat(const FVector3 &Axis, float AngleRad) noexcept;

    static FQuat MakeFromEuler(const FVector3 &InEulerDegrees) noexcept;
    static float DotProduct(const FQuat &A, const FQuat &B) noexcept;
    static FQuat Slerp(const FQuat &A, const FQuat &B, float Alpha) noexcept;

    bool     operator==(const FQuat &Other) const noexcept;
    bool     operator!=(const FQuat &Other) const noexcept;
    FQuat    operator-() const noexcept;
    FQuat    operator+(const FQuat &Other) const noexcept;
    FQuat    operator-(const FQuat &Other) const noexcept;
    FQuat    operator*(float Scale) const noexcept;
    FQuat    operator/(float Scale) const noexcept;
    FQuat    operator*(const FQuat &Other) const noexcept;
    FVector3 operator*(const FVector3 &InVector) const noexcept;

    FQuat &operator+=(const FQuat &Other) noexcept;
    FQuat &operator-=(const FQuat &Other) noexcept;
    FQuat &operator*=(float Scale) noexcept;
    FQuat &operator/=(float Scale) noexcept;
    FQuat &operator*=(const FQuat &Other) noexcept;

    float operator|(const FQuat &Other) const noexcept;

  public:
    bool     Equals(const FQuat &Other, float Tolerance = 1.e-6f) const noexcept;
    bool     IsIdentity(float Tolerance = 1.e-6f) const noexcept;
    bool     ContainsNaN() const noexcept;
    float    SizeSquared() const noexcept;
    float    Size() const noexcept;
    bool     IsNormalized(float Tolerance = 1.e-4f) const noexcept;
    void     Normalize(float Tolerance = 1.e-8f) noexcept;
    FQuat    GetNormalized(float Tolerance = 1.e-8f) const noexcept;
    FQuat    Conjugate() const noexcept;
    FQuat    Inverse() const noexcept;
    FVector3 RotateVector(const FVector3 &InVector) const noexcept;
    FVector3 UnrotateVector(const FVector3 &InVector) const noexcept;
    float    GetAngle() const noexcept;
    FVector3 GetRotationAxis(float Tolerance = 1.e-8f) const noexcept;
    FVector3 Euler() const noexcept;
    FVector3 GetAxisX() const noexcept;
    FVector3 GetAxisY() const noexcept;
    FVector3 GetAxisZ() const noexcept;
    FVector3 GetForwardVector() const noexcept;
    FVector3 GetRightVector() const noexcept;
    FVector3 GetUpVector() const noexcept;
    float    AngularDistance(const FQuat &Other) const noexcept;
    void     EnforceShortestArcWith(const FQuat &Other) noexcept;
    FMatrix  ToMatrix() const noexcept;

    FRotator Rotator() const noexcept;
};

inline FQuat operator*(float Scale, const FQuat &Quat) noexcept { return Quat * Scale; }
