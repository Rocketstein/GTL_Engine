#pragma once

#include "Math/Quat.h"

enum class ECollisionShapeType
{
    Box,
    Sphere,
    Capsule
};

struct FCollisionShapeGeometry
{
    ECollisionShapeType Type = ECollisionShapeType::Box;
    FVector Center = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    FVector BoxExtent = FVector::ZeroVector;
    FVector Axis = FVector(0.0f, 0.0f, 1.0f);
    float Radius = 0.0f;
    float HalfHeight = 0.0f;
};
