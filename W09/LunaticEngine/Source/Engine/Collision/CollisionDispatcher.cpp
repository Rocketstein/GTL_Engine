#include "CollisionDispatcher.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include <cmath>
#include <limits>

// ---- Geometry helpers ----

static FVector ClosestPointOnSegment(const FVector& P0, const FVector& P1, const FVector& Q) {
	FVector seg  = P1 - P0;
	float   len2 = seg.Dot(seg);
	if (len2 < 1e-10f) return P0;
	float t = (Q - P0).Dot(seg) / len2;
	t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
	return P0 + seg * t;
}

static FVector ClosestPointOnOBB(
	const FVector& Center, const FVector& Ax0, const FVector& Ax1, const FVector& Ax2,
	const FVector& Ext, const FVector& P)
{
	FVector d  = P - Center;
	float   c0 = d.Dot(Ax0); c0 = c0 < -Ext.X ? -Ext.X : (c0 > Ext.X ? Ext.X : c0);
	float   c1 = d.Dot(Ax1); c1 = c1 < -Ext.Y ? -Ext.Y : (c1 > Ext.Y ? Ext.Y : c1);
	float   c2 = d.Dot(Ax2); c2 = c2 < -Ext.Z ? -Ext.Z : (c2 > Ext.Z ? Ext.Z : c2);
	return Center + Ax0 * c0 + Ax1 * c1 + Ax2 * c2;
}

static void GetCapsuleSegment(const UCapsuleComponent* C, FVector& OutP0, FVector& OutP1) {
	FVector Center = C->GetWorldLocation();
	FVector Up     = C->GetUpVector().Normalized();
	float   SegHH  = C->GetCapsuleHalfHeight() - C->GetCapsuleRadius();
	if (SegHH < 0.f) SegHH = 0.f;
	OutP0 = Center - Up * SegHH;
	OutP1 = Center + Up * SegHH;
}

static FVector GetBoxCollisionExtent(const UBoxComponent* Box) {
	return Box ? Box->GetScaledBoxExtent() : FVector();
}

// Segment-to-segment closest points (Ericson, Real-Time Collision Detection).
// Returns squared distance; OutPA / OutPB are the closest points on each segment.
static float SegmentSegmentClosestPoints(
	const FVector& P0, const FVector& P1,
	const FVector& Q0, const FVector& Q1,
	FVector& OutPA, FVector& OutPB)
{
	FVector d1 = P1 - P0;
	FVector d2 = Q1 - Q0;
	FVector r  = P0 - Q0;
	float   a  = d1.Dot(d1);
	float   e  = d2.Dot(d2);
	float   f  = d2.Dot(r);
	float   s, t;

	if (a < 1e-10f && e < 1e-10f) {
		s = 0.f; t = 0.f;
	} else if (a < 1e-10f) {
		s = 0.f;
		t = f / e;
		t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
	} else {
		float c = d1.Dot(r);
		if (e < 1e-10f) {
			t = 0.f;
			s = -c / a;
			s = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
		} else {
			float b     = d1.Dot(d2);
			float denom = a * e - b * b;
			if (fabsf(denom) > 1e-10f) {
				s = (b * f - c * e) / denom;
				s = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
			} else {
				s = 0.f;
			}
			t = (b * s + f) / e;
			if (t < 0.f) {
				t = 0.f;
				s = -c / a;
				s = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
			} else if (t > 1.f) {
				t = 1.f;
				s = (b - c) / a;
				s = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
			}
		}
	}

	OutPA          = P0 + d1 * s;
	OutPB          = Q0 + d2 * t;
	FVector diff   = OutPA - OutPB;
	return diff.Dot(diff);
}

// ---- Collision functions ----

// ImpactNormal convention throughout: points from B toward A (direction A must move to separate).

static bool BoxBoxCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const UBoxComponent* BoxA = static_cast<const UBoxComponent*>(A);
	const UBoxComponent* BoxB = static_cast<const UBoxComponent*>(B);

	FVector CA = BoxA->GetWorldLocation();
	FVector CB = BoxB->GetWorldLocation();
	FVector d  = CB - CA;

	FVector a0 = BoxA->GetForwardVector().Normalized();
	FVector a1 = BoxA->GetRightVector().Normalized();
	FVector a2 = BoxA->GetUpVector().Normalized();
	FVector eA = GetBoxCollisionExtent(BoxA);

	FVector b0 = BoxB->GetForwardVector().Normalized();
	FVector b1 = BoxB->GetRightVector().Normalized();
	FVector b2 = BoxB->GetUpVector().Normalized();
	FVector eB = GetBoxCollisionExtent(BoxB);

	// 15 SAT axes: 3 face normals each + 9 edge cross products
	FVector axes[15] = {
		a0, a1, a2, b0, b1, b2,
		a0.Cross(b0), a0.Cross(b1), a0.Cross(b2),
		a1.Cross(b0), a1.Cross(b1), a1.Cross(b2),
		a2.Cross(b0), a2.Cross(b1), a2.Cross(b2),
	};

	float   minOverlap = std::numeric_limits<float>::max();
	FVector bestAxis;

	for (const FVector& axis : axes) {
		float lenSq = axis.Dot(axis);
		if (lenSq < 1e-10f) continue;	// degenerate cross product (parallel edges)
		FVector L  = axis * (1.f / sqrtf(lenSq));
		float   ra = fabsf(L.Dot(a0)) * eA.X + fabsf(L.Dot(a1)) * eA.Y + fabsf(L.Dot(a2)) * eA.Z;
		float   rb = fabsf(L.Dot(b0)) * eB.X + fabsf(L.Dot(b1)) * eB.Y + fabsf(L.Dot(b2)) * eB.Z;
		float   sep = fabsf(L.Dot(d));
		if (sep > ra + rb) return false;
		float overlap = (ra + rb) - sep;
		if (overlap < minOverlap) {
			minOverlap = overlap;
			bestAxis   = L;
		}
	}

	// Sign the MTD axis so it points from B toward A
	float sign = (bestAxis.Dot(d) >= 0.f) ? -1.f : 1.f;
	Info.HitResult.ImpactNormal    = bestAxis * sign;
	Info.HitResult.PenetrationDepth = minOverlap;
	return true;
}

static bool SphereSphereCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const USphereComponent* SA = static_cast<const USphereComponent*>(A);
	const USphereComponent* SB = static_cast<const USphereComponent*>(B);

	float   rSum = SA->GetSphereRadius() + SB->GetSphereRadius();
	FVector d    = SB->GetWorldLocation() - SA->GetWorldLocation();	// B - A
	float   distSq = d.Dot(d);
	if (distSq > rSum * rSum) return false;

	float dist = sqrtf(distSq);
	Info.HitResult.PenetrationDepth = rSum - dist;
	// Push A away from B → opposite to d
	Info.HitResult.ImpactNormal = (dist > 1e-6f) ? d * (-1.f / dist) : FVector(0.f, 1.f, 0.f);
	return true;
}

static bool BoxSphereCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const UBoxComponent*    Box;
	const USphereComponent* Sphere;
	bool boxIsA = (dynamic_cast<const UBoxComponent*>(A) != nullptr);
	if (boxIsA) {
		Box    = static_cast<const UBoxComponent*>(A);
		Sphere = static_cast<const USphereComponent*>(B);
	} else {
		Box    = static_cast<const UBoxComponent*>(B);
		Sphere = static_cast<const USphereComponent*>(A);
	}

	FVector Ax0 = Box->GetForwardVector().Normalized();
	FVector Ax1 = Box->GetRightVector().Normalized();
	FVector Ax2 = Box->GetUpVector().Normalized();
	FVector Ext = GetBoxCollisionExtent(Box);
	FVector BC  = Box->GetWorldLocation();
	FVector SC  = Sphere->GetWorldLocation();
	float   SR  = Sphere->GetSphereRadius();

	FVector Closest  = ClosestPointOnOBB(BC, Ax0, Ax1, Ax2, Ext, SC);
	FVector toSphere = SC - Closest;	// from box surface toward sphere center
	float   distSq   = toSphere.Dot(toSphere);
	if (distSq > SR * SR) return false;

	float dist = sqrtf(distSq);
	Info.HitResult.PenetrationDepth = SR - dist;

	FVector normal;
	if (dist > 1e-6f) {
		normal = toSphere * (1.f / dist);
	} else {
		// Sphere center coincides with closest box surface point - fall back to box-to-sphere direction
		FVector fallback = SC - BC;
		float   fbDist   = sqrtf(fallback.Dot(fallback));
		normal = (fbDist > 1e-6f) ? fallback * (1.f / fbDist) : Ax2;
	}

	// toSphere points from Box toward Sphere.
	// If Box==A (Sphere==B): push A away from B -> -normal (opposite to "toward B")
	// If Box==B (Sphere==A): toSphere points from B toward A -> +normal
	Info.HitResult.ImpactNormal = boxIsA ? normal * -1 : normal;
	return true;
}

static bool BoxCapsuleCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const UBoxComponent*     Box;
	const UCapsuleComponent* Capsule;
	bool boxIsA = (dynamic_cast<const UBoxComponent*>(A) != nullptr);
	if (boxIsA) {
		Box     = static_cast<const UBoxComponent*>(A);
		Capsule = static_cast<const UCapsuleComponent*>(B);
	} else {
		Box     = static_cast<const UBoxComponent*>(B);
		Capsule = static_cast<const UCapsuleComponent*>(A);
	}

	FVector Ax0 = Box->GetForwardVector().Normalized();
	FVector Ax1 = Box->GetRightVector().Normalized();
	FVector Ax2 = Box->GetUpVector().Normalized();
	FVector Ext = GetBoxCollisionExtent(Box);
	FVector BC  = Box->GetWorldLocation();
	float   CR  = Capsule->GetCapsuleRadius();

	FVector P0, P1;
	GetCapsuleSegment(Capsule, P0, P1);

	// Test three representative capsule axis points; pick the shallowest overlap
	// for the MTV (largest dist-to-box = smallest penetration depth).
	FVector candidates[3] = { P0, P1, ClosestPointOnSegment(P0, P1, BC) };

	bool    hit        = false;
	float   maxDistSq  = -1.f;
	FVector bestP, bestClosestOnBox;

	for (const FVector& P : candidates) {
		FVector Closest = ClosestPointOnOBB(BC, Ax0, Ax1, Ax2, Ext, P);
		float   dSq     = FVector::DistSquared(P, Closest);
		if (dSq <= CR * CR) {
			hit = true;
			if (dSq > maxDistSq) {
				maxDistSq        = dSq;
				bestP            = P;
				bestClosestOnBox = Closest;
			}
		}
	}

	if (!hit) return false;

	float   dist    = sqrtf(maxDistSq);
	FVector toSeg   = bestP - bestClosestOnBox;	// from box surface toward capsule axis
	Info.HitResult.PenetrationDepth = CR - dist;

	FVector normal;
	if (dist > 1e-6f) {
		normal = toSeg * (1.f / dist);
	} else {
		// Capsule axis point inside box — fall back to box-to-capsule-center direction
		FVector fallback = Capsule->GetWorldLocation() - BC;
		float   fbDist   = sqrtf(fallback.Dot(fallback));
		normal = (fbDist > 1e-6f) ? fallback * (1.f / fbDist) : Ax2;
	}

	// toSeg points from Box toward capsule axis.
	// If Box==A: push A away from B -> -normal
	// If Box==B: toSeg points from B toward A -> +normal
	Info.HitResult.ImpactNormal = boxIsA ? normal * -1 : normal;
	return true;
}

static bool SphereCapsuleCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const USphereComponent*  Sphere;
	const UCapsuleComponent* Capsule;
	bool sphereIsA = (dynamic_cast<const USphereComponent*>(A) != nullptr);
	if (sphereIsA) {
		Sphere  = static_cast<const USphereComponent*>(A);
		Capsule = static_cast<const UCapsuleComponent*>(B);
	} else {
		Sphere  = static_cast<const USphereComponent*>(B);
		Capsule = static_cast<const UCapsuleComponent*>(A);
	}

	FVector P0, P1;
	GetCapsuleSegment(Capsule, P0, P1);
	FVector SC       = Sphere->GetWorldLocation();
	FVector Closest  = ClosestPointOnSegment(P0, P1, SC);
	float   rSum     = Sphere->GetSphereRadius() + Capsule->GetCapsuleRadius();
	FVector toSphere = SC - Closest;	// from capsule axis toward sphere center
	float   distSq   = toSphere.Dot(toSphere);
	if (distSq > rSum * rSum) return false;

	float dist = sqrtf(distSq);
	Info.HitResult.PenetrationDepth = rSum - dist;

	FVector normal = (dist > 1e-6f) ? toSphere * (1.f / dist) : FVector(0.f, 1.f, 0.f);

	// toSphere points from Capsule toward Sphere.
	// If Sphere==A (Capsule==B): points from B toward A -> +normal
	// If Sphere==B (Capsule==A): points from A toward B -> -normal
	Info.HitResult.ImpactNormal = sphereIsA ? normal : normal * -1;
	return true;
}

static bool CapsuleCapsuleCollision(const UShapeComponent* A, const UShapeComponent* B, FOverlapInfo& Info) {
	const UCapsuleComponent* CA = static_cast<const UCapsuleComponent*>(A);
	const UCapsuleComponent* CB = static_cast<const UCapsuleComponent*>(B);

	FVector P0, P1, Q0, Q1;
	GetCapsuleSegment(CA, P0, P1);
	GetCapsuleSegment(CB, Q0, Q1);

	float   rSum = CA->GetCapsuleRadius() + CB->GetCapsuleRadius();
	FVector ClosestA, ClosestB;
	float   distSq = SegmentSegmentClosestPoints(P0, P1, Q0, Q1, ClosestA, ClosestB);
	if (distSq > rSum * rSum) return false;

	float   dist = sqrtf(distSq);
	FVector toA  = ClosestA - ClosestB;	// from CB axis toward CA axis = from B toward A

	Info.HitResult.PenetrationDepth = rSum - dist;
	Info.HitResult.ImpactNormal     = (dist > 1e-6f) ? toA * (1.f / dist) : FVector(0.f, 1.f, 0.f);
	return true;
}

void FCollisionDispatcher::Init() {
	Register("UBoxComponent",     "UBoxComponent",     BoxBoxCollision);
	Register("UBoxComponent",     "USphereComponent",  BoxSphereCollision);
	Register("UBoxComponent",     "UCapsuleComponent", BoxCapsuleCollision);
	Register("USphereComponent",  "USphereComponent",  SphereSphereCollision);
	Register("USphereComponent",  "UCapsuleComponent", SphereCapsuleCollision);
	Register("UCapsuleComponent", "UCapsuleComponent", CapsuleCapsuleCollision);
}
