#include "CollisionShapeQuery.h"

#include "Math/Intersection.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace CollisionShapeQuery
{
bool OverlapSphereSphere(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    const float RadiusSum = A.Radius + B.Radius;
    return (A.Center - B.Center).LengthSquared() <= RadiusSum * RadiusSum;
}

bool OverlapBoxSphere(const FCollisionShapeGeometry& Box, const FCollisionShapeGeometry& Sphere)
{
    const FVector LocalSphereCenter = Box.Rotation.Inverse().RotateVector(Sphere.Center - Box.Center);

    const FVector ClosestPoint(
        FMath::Clamp(LocalSphereCenter.X, -Box.BoxExtent.X, Box.BoxExtent.X),
        FMath::Clamp(LocalSphereCenter.Y, -Box.BoxExtent.Y, Box.BoxExtent.Y),
        FMath::Clamp(LocalSphereCenter.Z, -Box.BoxExtent.Z, Box.BoxExtent.Z));

    return (LocalSphereCenter - ClosestPoint).LengthSquared() <= Sphere.Radius * Sphere.Radius;
}

bool OverlapBoxBox(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    const FVector AxesA[3] = {
        A.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized(),
        A.Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)).Normalized(),
        A.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized()
    };
    const FVector AxesB[3] = {
        B.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized(),
        B.Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)).Normalized(),
        B.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized()
    };

    return FMath::IntersectOBBOBB(A.Center, A.BoxExtent, AxesA, B.Center, B.BoxExtent, AxesB);
}

static float SegmentSegmentDistanceSquared(const FVector& P1, const FVector& Q1, const FVector& P2, const FVector& Q2)
{
    const FVector D1 = Q1 - P1;
    const FVector D2 = Q2 - P2;
    const FVector R = P1 - P2;
    const float A = D1.Dot(D1);
    const float E = D2.Dot(D2);
    const float F = D2.Dot(R);

    float S = 0.0f;
    float T = 0.0f;

    if (A <= FMath::Epsilon && E <= FMath::Epsilon)
    {
        return (P1 - P2).LengthSquared();
    }

    if (A <= FMath::Epsilon)
    {
        T = FMath::Clamp(F / E, 0.0f, 1.0f);
    }
    else
    {
        const float C = D1.Dot(R);
        if (E <= FMath::Epsilon)
        {
            S = FMath::Clamp(-C / A, 0.0f, 1.0f);
        }
        else
        {
            const float B = D1.Dot(D2);
            const float Denom = A * E - B * B;

            if (Denom != 0.0f)
            {
                S = FMath::Clamp((B * F - C * E) / Denom, 0.0f, 1.0f);
            }

            T = (B * S + F) / E;
            if (T < 0.0f)
            {
                T = 0.0f;
                S = FMath::Clamp(-C / A, 0.0f, 1.0f);
            }
            else if (T > 1.0f)
            {
                T = 1.0f;
                S = FMath::Clamp((B - C) / A, 0.0f, 1.0f);
            }
        }
    }

    const FVector Closest1 = P1 + D1 * S;
    const FVector Closest2 = P2 + D2 * T;
    return (Closest1 - Closest2).LengthSquared();
}

static float SegmentSegmentClosestPoints(const FVector& P1, const FVector& Q1, const FVector& P2, const FVector& Q2, FVector& OutClosest1, FVector& OutClosest2)
{
    const FVector D1 = Q1 - P1;
    const FVector D2 = Q2 - P2;
    const FVector R = P1 - P2;
    const float A = D1.Dot(D1);
    const float E = D2.Dot(D2);
    const float F = D2.Dot(R);

    float S = 0.0f;
    float T = 0.0f;

    if (A <= FMath::Epsilon && E <= FMath::Epsilon)
    {
        OutClosest1 = P1;
        OutClosest2 = P2;
        return (OutClosest1 - OutClosest2).LengthSquared();
    }

    if (A <= FMath::Epsilon)
    {
        T = FMath::Clamp(F / E, 0.0f, 1.0f);
    }
    else
    {
        const float C = D1.Dot(R);
        if (E <= FMath::Epsilon)
        {
            S = FMath::Clamp(-C / A, 0.0f, 1.0f);
        }
        else
        {
            const float B = D1.Dot(D2);
            const float Denom = A * E - B * B;

            if (Denom != 0.0f)
            {
                S = FMath::Clamp((B * F - C * E) / Denom, 0.0f, 1.0f);
            }

            T = (B * S + F) / E;
            if (T < 0.0f)
            {
                T = 0.0f;
                S = FMath::Clamp(-C / A, 0.0f, 1.0f);
            }
            else if (T > 1.0f)
            {
                T = 1.0f;
                S = FMath::Clamp((B - C) / A, 0.0f, 1.0f);
            }
        }
    }

    OutClosest1 = P1 + D1 * S;
    OutClosest2 = P2 + D2 * T;
    return (OutClosest1 - OutClosest2).LengthSquared();
}

static void GetCapsuleSegment(const FCollisionShapeGeometry& Capsule, FVector& OutStart, FVector& OutEnd)
{
    FVector Axis = Capsule.Axis;
    if (Axis.LengthSquared() <= FMath::Epsilon)
    {
        Axis = Capsule.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
    }
    Axis.Normalize();
    const float SegmentHalf = (Capsule.HalfHeight > Capsule.Radius) ? (Capsule.HalfHeight - Capsule.Radius) : 0.0f;

    OutStart = Capsule.Center - Axis * SegmentHalf;
    OutEnd = Capsule.Center + Axis * SegmentHalf;
}

static float PointAABBDistanceSquared(const FVector& Point, const FVector& Extent)
{
    float DistanceSquared = 0.0f;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const float Value = Point.Data[Axis];
        const float Min = -Extent.Data[Axis];
        const float Max = Extent.Data[Axis];

        if (Value < Min)
        {
            const float Delta = Min - Value;
            DistanceSquared += Delta * Delta;
        }
        else if (Value > Max)
        {
            const float Delta = Value - Max;
            DistanceSquared += Delta * Delta;
        }
    }

    return DistanceSquared;
}

static FVector ClosestPointOnAABB(const FVector& Point, const FVector& Extent)
{
    return FVector(
        FMath::Clamp(Point.X, -Extent.X, Extent.X),
        FMath::Clamp(Point.Y, -Extent.Y, Extent.Y),
        FMath::Clamp(Point.Z, -Extent.Z, Extent.Z));
}

static FVector MakeSafeNormal(const FVector& Vector, const FVector& Fallback)
{
    if (Vector.LengthSquared() <= FMath::Epsilon)
    {
        return Fallback.Normalized();
    }

    return Vector.Normalized();
}

static float GetBoundingRadius(const FCollisionShapeGeometry& Geometry)
{
    switch (Geometry.Type)
    {
    case ECollisionShapeType::Sphere:
        return Geometry.Radius;
    case ECollisionShapeType::Capsule:
        return Geometry.HalfHeight;
    case ECollisionShapeType::Box:
        return Geometry.BoxExtent.Length();
    default:
        return 1.0f;
    }
}

static void FlipContact(FCollisionContact& Contact)
{
    Contact.Normal *= -1.0f;
}

static FVector TransformLocalDirectionToWorld(const FCollisionShapeGeometry& Box, const FVector& LocalDirection)
{
    return Box.Rotation.RotateVector(LocalDirection).Normalized();
}

static float SegmentAABBDistanceSquared(const FVector& Start, const FVector& End, const FVector& Extent)
{
    const FVector Direction = End - Start;
    float Candidates[8] = { 0.0f, 1.0f };
    int32 CandidateCount = 2;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const float Delta = Direction.Data[Axis];
        if (std::abs(Delta) <= FMath::Epsilon)
        {
            continue;
        }

        const float MinT = (-Extent.Data[Axis] - Start.Data[Axis]) / Delta;
        const float MaxT = (Extent.Data[Axis] - Start.Data[Axis]) / Delta;

        if (MinT > 0.0f && MinT < 1.0f)
        {
            Candidates[CandidateCount++] = MinT;
        }

        if (MaxT > 0.0f && MaxT < 1.0f)
        {
            Candidates[CandidateCount++] = MaxT;
        }
    }

    for (int32 i = 1; i < CandidateCount; ++i)
    {
        const float Value = Candidates[i];
        int32 j = i - 1;
        while (j >= 0 && Candidates[j] > Value)
        {
            Candidates[j + 1] = Candidates[j];
            --j;
        }
        Candidates[j + 1] = Value;
    }

    float BestDistanceSquared = PointAABBDistanceSquared(Start, Extent);
    const float EndDistanceSquared = PointAABBDistanceSquared(End, Extent);
    if (EndDistanceSquared < BestDistanceSquared)
    {
        BestDistanceSquared = EndDistanceSquared;
    }

    for (int32 i = 0; i < CandidateCount - 1; ++i)
    {
        const float MinT = Candidates[i];
        const float MaxT = Candidates[i + 1];
        if (MaxT - MinT <= FMath::Epsilon)
        {
            continue;
        }

        const float MidT = (MinT + MaxT) * 0.5f;
        const FVector MidPoint = Start + Direction * MidT;
        float Numerator = 0.0f;
        float Denominator = 0.0f;

        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            const float Value = MidPoint.Data[Axis];
            const float Min = -Extent.Data[Axis];
            const float Max = Extent.Data[Axis];
            float Bound = 0.0f;
            bool bOutside = false;

            if (Value < Min)
            {
                Bound = Min;
                bOutside = true;
            }
            else if (Value > Max)
            {
                Bound = Max;
                bOutside = true;
            }

            if (bOutside)
            {
                Numerator += Direction.Data[Axis] * (Start.Data[Axis] - Bound);
                Denominator += Direction.Data[Axis] * Direction.Data[Axis];
            }
        }

        float BestT = MinT;
        if (Denominator > FMath::Epsilon)
        {
            BestT = FMath::Clamp(-Numerator / Denominator, MinT, MaxT);
        }

        const float DistanceSquared = PointAABBDistanceSquared(Start + Direction * BestT, Extent);
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
        }
    }

    return BestDistanceSquared;
}

static float SegmentAABBClosestPoints(const FVector& Start, const FVector& End, const FVector& Extent, FVector& OutSegmentPoint, FVector& OutBoxPoint)
{
    const FVector Direction = End - Start;
    float Candidates[8] = { 0.0f, 1.0f };
    int32 CandidateCount = 2;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const float Delta = Direction.Data[Axis];
        if (std::abs(Delta) <= FMath::Epsilon)
        {
            continue;
        }

        const float MinT = (-Extent.Data[Axis] - Start.Data[Axis]) / Delta;
        const float MaxT = (Extent.Data[Axis] - Start.Data[Axis]) / Delta;

        if (MinT > 0.0f && MinT < 1.0f)
        {
            Candidates[CandidateCount++] = MinT;
        }

        if (MaxT > 0.0f && MaxT < 1.0f)
        {
            Candidates[CandidateCount++] = MaxT;
        }
    }

    float BestDistanceSquared = FLT_MAX;
    for (int32 i = 0; i < CandidateCount; ++i)
    {
        const FVector SegmentPoint = Start + Direction * Candidates[i];
        const FVector BoxPoint = ClosestPointOnAABB(SegmentPoint, Extent);
        const float DistanceSquared = (SegmentPoint - BoxPoint).LengthSquared();
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            OutSegmentPoint = SegmentPoint;
            OutBoxPoint = BoxPoint;
        }
    }

    return BestDistanceSquared;
}

bool OverlapCapsuleSphere(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Sphere)
{
    FVector SegmentStart;
    FVector SegmentEnd;
    GetCapsuleSegment(Capsule, SegmentStart, SegmentEnd);

    const float RadiusSum = Capsule.Radius + Sphere.Radius;
    return SegmentSegmentDistanceSquared(SegmentStart, SegmentEnd, Sphere.Center, Sphere.Center) <= RadiusSum * RadiusSum;
}

bool OverlapCapsuleBox(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Box)
{
    FVector SegmentStart;
    FVector SegmentEnd;
    GetCapsuleSegment(Capsule, SegmentStart, SegmentEnd);

    const FQuat InvBoxRotation = Box.Rotation.Inverse();
    const FVector LocalStart = InvBoxRotation.RotateVector(SegmentStart - Box.Center);
    const FVector LocalEnd = InvBoxRotation.RotateVector(SegmentEnd - Box.Center);

    return SegmentAABBDistanceSquared(LocalStart, LocalEnd, Box.BoxExtent) <= Capsule.Radius * Capsule.Radius;
}

bool OverlapCapsuleCapsule(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    FVector A0;
    FVector A1;
    FVector B0;
    FVector B1;
    GetCapsuleSegment(A, A0, A1);
    GetCapsuleSegment(B, B0, B1);

    const float RadiusSum = A.Radius + B.Radius;
    return SegmentSegmentDistanceSquared(A0, A1, B0, B1) <= RadiusSum * RadiusSum;
}

static bool ComputeSphereSpherePenetration(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B, FCollisionContact& OutContact)
{
    const FVector Delta = B.Center - A.Center;
    const float RadiusSum = A.Radius + B.Radius;
    const float DistanceSquared = Delta.LengthSquared();
    if (DistanceSquared > RadiusSum * RadiusSum)
    {
        return false;
    }

    const float Distance = std::sqrt(std::max(DistanceSquared, 0.0f));
    OutContact.Normal = MakeSafeNormal(Delta, FVector(0.0f, 0.0f, 1.0f));
    OutContact.PenetrationDepth = RadiusSum - Distance;
    return true;
}

static bool ComputeBoxSpherePenetration(const FCollisionShapeGeometry& Box, const FCollisionShapeGeometry& Sphere, FCollisionContact& OutContact)
{
    const FQuat InvBoxRotation = Box.Rotation.Inverse();
    const FVector LocalSphereCenter = InvBoxRotation.RotateVector(Sphere.Center - Box.Center);
    const FVector ClosestPoint = ClosestPointOnAABB(LocalSphereCenter, Box.BoxExtent);
    const FVector LocalDelta = LocalSphereCenter - ClosestPoint;
    const float DistanceSquared = LocalDelta.LengthSquared();

    if (DistanceSquared > Sphere.Radius * Sphere.Radius)
    {
        return false;
    }

    if (DistanceSquared > FMath::Epsilon)
    {
        const float Distance = std::sqrt(DistanceSquared);
        OutContact.Normal = TransformLocalDirectionToWorld(Box, LocalDelta);
        OutContact.PenetrationDepth = Sphere.Radius - Distance;
        return true;
    }

    float BestFaceDistance = FLT_MAX;
    FVector LocalNormal(1.0f, 0.0f, 0.0f);
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const float PositiveDistance = Box.BoxExtent.Data[Axis] - LocalSphereCenter.Data[Axis];
        if (PositiveDistance < BestFaceDistance)
        {
            BestFaceDistance = PositiveDistance;
            LocalNormal = FVector::ZeroVector;
            LocalNormal.Data[Axis] = 1.0f;
        }

        const float NegativeDistance = LocalSphereCenter.Data[Axis] + Box.BoxExtent.Data[Axis];
        if (NegativeDistance < BestFaceDistance)
        {
            BestFaceDistance = NegativeDistance;
            LocalNormal = FVector::ZeroVector;
            LocalNormal.Data[Axis] = -1.0f;
        }
    }

    OutContact.Normal = TransformLocalDirectionToWorld(Box, LocalNormal);
    OutContact.PenetrationDepth = Sphere.Radius + BestFaceDistance;
    return true;
}

static bool ComputeBoxBoxPenetration(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B, FCollisionContact& OutContact)
{
    const FVector AxesA[3] = {
        A.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized(),
        A.Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)).Normalized(),
        A.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized()
    };
    const FVector AxesB[3] = {
        B.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized(),
        B.Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)).Normalized(),
        B.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized()
    };

    const FVector CenterDelta = B.Center - A.Center;
    float BestOverlap = FLT_MAX;
    FVector BestAxis(0.0f, 0.0f, 1.0f);

    auto TestAxis = [&](const FVector& Axis) -> bool
    {
        if (Axis.LengthSquared() <= FMath::Epsilon)
        {
            return true;
        }

        const FVector N = Axis.Normalized();
        const float RadiusA =
            A.BoxExtent.X * std::abs(N.Dot(AxesA[0])) +
            A.BoxExtent.Y * std::abs(N.Dot(AxesA[1])) +
            A.BoxExtent.Z * std::abs(N.Dot(AxesA[2]));
        const float RadiusB =
            B.BoxExtent.X * std::abs(N.Dot(AxesB[0])) +
            B.BoxExtent.Y * std::abs(N.Dot(AxesB[1])) +
            B.BoxExtent.Z * std::abs(N.Dot(AxesB[2]));
        const float Distance = CenterDelta.Dot(N);
        const float Overlap = RadiusA + RadiusB - std::abs(Distance);

        if (Overlap < 0.0f)
        {
            return false;
        }

        if (Overlap < BestOverlap)
        {
            BestOverlap = Overlap;
            BestAxis = (Distance < 0.0f) ? (N * -1.0f) : N;
        }

        return true;
    };

    for (int32 i = 0; i < 3; ++i)
    {
        if (!TestAxis(AxesA[i]) || !TestAxis(AxesB[i]))
        {
            return false;
        }
    }

    for (int32 i = 0; i < 3; ++i)
    {
        for (int32 j = 0; j < 3; ++j)
        {
            if (!TestAxis(AxesA[i].Cross(AxesB[j])))
            {
                return false;
            }
        }
    }

    OutContact.Normal = BestAxis;
    OutContact.PenetrationDepth = BestOverlap;
    return true;
}

static bool ComputeCapsuleSpherePenetration(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Sphere, FCollisionContact& OutContact)
{
    FVector SegmentStart;
    FVector SegmentEnd;
    GetCapsuleSegment(Capsule, SegmentStart, SegmentEnd);

    FVector CapsulePoint;
    FVector SpherePoint;
    const float DistanceSquared = SegmentSegmentClosestPoints(SegmentStart, SegmentEnd, Sphere.Center, Sphere.Center, CapsulePoint, SpherePoint);
    const float RadiusSum = Capsule.Radius + Sphere.Radius;
    if (DistanceSquared > RadiusSum * RadiusSum)
    {
        return false;
    }

    const float Distance = std::sqrt(std::max(DistanceSquared, 0.0f));
    OutContact.Normal = MakeSafeNormal(Sphere.Center - CapsulePoint, Sphere.Center - Capsule.Center);
    OutContact.PenetrationDepth = RadiusSum - Distance;
    return true;
}

static bool ComputeCapsuleCapsulePenetration(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B, FCollisionContact& OutContact)
{
    FVector A0;
    FVector A1;
    FVector B0;
    FVector B1;
    GetCapsuleSegment(A, A0, A1);
    GetCapsuleSegment(B, B0, B1);

    FVector ClosestA;
    FVector ClosestB;
    const float DistanceSquared = SegmentSegmentClosestPoints(A0, A1, B0, B1, ClosestA, ClosestB);
    const float RadiusSum = A.Radius + B.Radius;
    if (DistanceSquared > RadiusSum * RadiusSum)
    {
        return false;
    }

    const float Distance = std::sqrt(std::max(DistanceSquared, 0.0f));
    OutContact.Normal = MakeSafeNormal(ClosestB - ClosestA, B.Center - A.Center);
    OutContact.PenetrationDepth = RadiusSum - Distance;
    return true;
}

static bool ComputeCapsuleBoxPenetration(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Box, FCollisionContact& OutContact)
{
    FVector SegmentStart;
    FVector SegmentEnd;
    GetCapsuleSegment(Capsule, SegmentStart, SegmentEnd);

    const FQuat InvBoxRotation = Box.Rotation.Inverse();
    const FVector LocalStart = InvBoxRotation.RotateVector(SegmentStart - Box.Center);
    const FVector LocalEnd = InvBoxRotation.RotateVector(SegmentEnd - Box.Center);

    FVector LocalSegmentPoint;
    FVector LocalBoxPoint;
    const float DistanceSquared = SegmentAABBClosestPoints(LocalStart, LocalEnd, Box.BoxExtent, LocalSegmentPoint, LocalBoxPoint);
    if (DistanceSquared > Capsule.Radius * Capsule.Radius)
    {
        return false;
    }

    if (DistanceSquared > FMath::Epsilon)
    {
        const FVector LocalNormal = MakeSafeNormal(LocalBoxPoint - LocalSegmentPoint, FVector(0.0f, 0.0f, 1.0f));
        OutContact.Normal = TransformLocalDirectionToWorld(Box, LocalNormal);
        OutContact.PenetrationDepth = Capsule.Radius - std::sqrt(DistanceSquared);
        return true;
    }

    float BestFaceDistance = FLT_MAX;
    FVector LocalNormal(1.0f, 0.0f, 0.0f);
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const float PositiveDistance = Box.BoxExtent.Data[Axis] - LocalSegmentPoint.Data[Axis];
        if (PositiveDistance < BestFaceDistance)
        {
            BestFaceDistance = PositiveDistance;
            LocalNormal = FVector::ZeroVector;
            LocalNormal.Data[Axis] = 1.0f;
        }

        const float NegativeDistance = LocalSegmentPoint.Data[Axis] + Box.BoxExtent.Data[Axis];
        if (NegativeDistance < BestFaceDistance)
        {
            BestFaceDistance = NegativeDistance;
            LocalNormal = FVector::ZeroVector;
            LocalNormal.Data[Axis] = -1.0f;
        }
    }

    OutContact.Normal = TransformLocalDirectionToWorld(Box, LocalNormal);
    OutContact.PenetrationDepth = Capsule.Radius + BestFaceDistance;
    return true;
}

static bool ComputeFallbackPenetration(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B, FCollisionContact& OutContact)
{
    if (!OverlapShapeGeometry(A, B))
    {
        return false;
    }

    const FVector Delta = B.Center - A.Center;
    const float RadiusSum = GetBoundingRadius(A) + GetBoundingRadius(B);
    const float Distance = std::sqrt(std::max(Delta.LengthSquared(), 0.0f));
    OutContact.Normal = MakeSafeNormal(Delta, FVector(0.0f, 0.0f, 1.0f));
    OutContact.PenetrationDepth = std::max(RadiusSum - Distance, 0.1f);
    return true;
}

bool OverlapShapeGeometry(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    switch (A.Type)
    {
    case ECollisionShapeType::Sphere:
        switch (B.Type)
        {
        case ECollisionShapeType::Sphere:
            return OverlapSphereSphere(A, B);
        case ECollisionShapeType::Box:
            return OverlapBoxSphere(B, A);
        case ECollisionShapeType::Capsule:
            return OverlapCapsuleSphere(B, A);
        default:
            break;
        }
        break;

    case ECollisionShapeType::Box:
        switch (B.Type)
        {
        case ECollisionShapeType::Box:
            return OverlapBoxBox(A, B);
        case ECollisionShapeType::Sphere:
            return OverlapBoxSphere(A, B);
        case ECollisionShapeType::Capsule:
            return OverlapCapsuleBox(B, A);
        default:
            break;
        }
        break;

    case ECollisionShapeType::Capsule:
        switch (B.Type)
        {
        case ECollisionShapeType::Sphere:
            return OverlapCapsuleSphere(A, B);
        case ECollisionShapeType::Box:
            return OverlapCapsuleBox(A, B);
        case ECollisionShapeType::Capsule:
            return OverlapCapsuleCapsule(A, B);
        default:
            break;
        }
        break;
    }

    return false;
}

bool ComputePenetration(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B, FCollisionContact& OutContact)
{
    switch (A.Type)
    {
    case ECollisionShapeType::Sphere:
        switch (B.Type)
        {
        case ECollisionShapeType::Sphere:
            return ComputeSphereSpherePenetration(A, B, OutContact);
        case ECollisionShapeType::Box:
            if (ComputeBoxSpherePenetration(B, A, OutContact))
            {
                FlipContact(OutContact);
                return true;
            }
            return false;
        case ECollisionShapeType::Capsule:
            if (ComputeCapsuleSpherePenetration(B, A, OutContact))
            {
                FlipContact(OutContact);
                return true;
            }
            return false;
        default:
            break;
        }
        break;

    case ECollisionShapeType::Box:
        switch (B.Type)
        {
        case ECollisionShapeType::Box:
            return ComputeBoxBoxPenetration(A, B, OutContact);
        case ECollisionShapeType::Sphere:
            return ComputeBoxSpherePenetration(A, B, OutContact);
        case ECollisionShapeType::Capsule:
            if (ComputeCapsuleBoxPenetration(B, A, OutContact))
            {
                FlipContact(OutContact);
                return true;
            }
            return false;
        default:
            break;
        }
        break;

    case ECollisionShapeType::Capsule:
        switch (B.Type)
        {
        case ECollisionShapeType::Sphere:
            return ComputeCapsuleSpherePenetration(A, B, OutContact);
        case ECollisionShapeType::Box:
            return ComputeCapsuleBoxPenetration(A, B, OutContact);
        case ECollisionShapeType::Capsule:
            return ComputeCapsuleCapsulePenetration(A, B, OutContact);
        default:
            break;
        }
        break;
    }

    return ComputeFallbackPenetration(A, B, OutContact);
}
} // namespace CollisionShapeQuery
