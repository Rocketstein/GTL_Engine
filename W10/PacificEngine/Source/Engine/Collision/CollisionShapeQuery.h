#pragma once

#include "CollisionShapeGeometry.h"

struct FCollisionContact
{
    FVector Normal = FVector(0.0f, 0.0f, 1.0f);
    float PenetrationDepth = 0.0f;
};

namespace CollisionShapeQuery
{
bool OverlapSphereSphere(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B);
bool OverlapBoxSphere(const FCollisionShapeGeometry& Box, const FCollisionShapeGeometry& Sphere);
bool OverlapBoxBox(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B);
bool OverlapCapsuleSphere(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Sphere);
bool OverlapCapsuleBox(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Box);
bool OverlapCapsuleCapsule(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B);
bool OverlapShapeGeometry(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B);
bool ComputePenetration(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B, FCollisionContact& OutContact);
}
