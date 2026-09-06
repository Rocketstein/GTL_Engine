#include "Physics.h"
#include "Constants.h"
#include "Ball.h"

#include <cmath>
#include <algorithm>

namespace
{
    constexpr float EPSILON = 1e-6f;                // ��� �� ��� ����
    constexpr float POSITION_PERCENT = 0.8f;        // ��ħ ���� �� 1ȸ�� ���� ����
    constexpr float POSITION_SLOP = 0.0001f;        // ��ħ ��� ����
    constexpr float WALL_FRICTION = 0.35f;          // �浹 �� ���� ���� ���
    constexpr float LINEAR_DAMPING = 0.1f;         // ���� ����
    constexpr float BODY_FRICTION = 0.30f;          // ��ü���� ���� ���
    constexpr int SOLVER_ITERATIONS = 4;            // �浹 �ذ��� �ݺ��ϴ� Ƚ��

	inline bool IsDynamic(const IActor* Actor)
	{
		return Actor && Actor->GetBodyType() == EBodyType::Dynamic;
	}

	inline bool IsKinematic(const IActor* Actor)
	{
		return Actor && Actor->GetBodyType() == EBodyType::Kinematic;
	}

	inline bool IsStatic(const IActor* Actor)
	{
		return Actor && Actor->GetBodyType() == EBodyType::Static;
	}

	inline float GetEffectiveInvMass(const IActor* Actor)
	{
		if (!Actor || !IsDynamic(Actor))
			return 0.0f;
		const float Mass = Actor->GetMass();
		return (Mass > EPSILON) ? (1.0f / Mass) : 0.0f;
	}

	inline float GetEffectiveInvInertia(const IActor* Actor)
	{
		if (!Actor || !IsDynamic(Actor) || !Actor->GetApplyRotation())
			return 0.0f;

		const float Mass = Actor->GetMass();
		if (Mass <= EPSILON)
			return 0.0f;

		switch (Actor->GetCollisionShape())
		{
		case ECollisionShape::Circle:
		{
			const float Radius = Actor->GetScale();
			const float Inertia = 0.5f * Mass * Radius * Radius;
			return (Inertia > EPSILON) ? (1.0f / Inertia) : 0.0f;
		}
		case ECollisionShape::AABB:
		{
			const FVector Half = Actor->GetHalfExtent();
			const float W = Half.x * 2.0f;
			const float H = Half.y * 2.0f;
			const float Inertia = (1.0f / 12.0f) * Mass * (W * W + H * H);
			return (Inertia > EPSILON) ? (1.0f / Inertia) : 0.0f;
		}
		default:
			return 0.0f;
		}
	}
}

void UPhysicsSystem::Step(std::vector<IActor*>& Actors, const FWorldBounds& Bounds, float DeltaTime)
{
	Integrate(Actors, DeltaTime);

	UpdateColliderExtraVelocities(Actors, DeltaTime);

	for (int Iter = 0; Iter < SOLVER_ITERATIONS; ++Iter)
	{
		SolveActorCollisions(Actors, Iter == 0);
		SolveWallCollisions(Actors, Bounds);
	}

	CommitColliderLocalOffsets(Actors);
}

void UPhysicsSystem::Integrate(std::vector<IActor*>& Actors, float DeltaTime)
{
	for (IActor* Actor : Actors)
	{
		if (!Actor) continue;

		FVector Velocity = Actor->GetVelocity();
		FVector Position = Actor->GetPosition();
		float Rotation = Actor->GetRotation();

		const bool ApplyGravity = Actor->GetApplyGravity();
		const bool ApplyRotation = Actor->GetApplyRotation() &&
			Actor->GetCollisionShape() != ECollisionShape::AABB;

		switch (Actor->GetBodyType())
		{
		case EBodyType::Static:
			break;

		case EBodyType::Kinematic:
			Position.x += Velocity.x * DeltaTime;
			Position.y += Velocity.y * DeltaTime;
			Position.z = 0.0f;

			if (ApplyRotation) Rotation += Actor->GetAngularVelocity() * DeltaTime;

			Actor->SetPosition(Position);
			if (ApplyRotation) Actor->SetRotation(Rotation);
			break;

		case EBodyType::Dynamic:
			if (ApplyGravity) Velocity.y -= GRAVITY * DeltaTime;

			Position.x += Velocity.x * DeltaTime;
			Position.y += Velocity.y * DeltaTime;
			Position.z = 0.0f;

			if (ApplyRotation) Rotation += Actor->GetAngularVelocity() * DeltaTime;

			Actor->SetVelocity(Velocity);
			Actor->SetPosition(Position);
			if (ApplyRotation) Actor->SetRotation(Rotation);
			break;
		}
	}
}

void UPhysicsSystem::SolveWallCollisions(std::vector<IActor*>& Actors, const FWorldBounds& Bounds)
{
	for (IActor* Actor : Actors)
	{
		if (!Actor) continue;

		const int colliderCount = Actor->GetColliderPartCount();
		for (int i = 0; i < colliderCount; ++i)
		{
			const FColliderPart& part = Actor->GetColliderPart(i);
			if (!part.bEnabled)
				continue;

			switch (part.Shape)
			{
			case ECollisionShape::Circle:
				SolveCircleWall(Actor, i, Bounds);
				break;
			case ECollisionShape::AABB:
				SolveAABBWall(Actor, i, Bounds);
				break;
			default:
				break;
			}
		}
	}
}

void UPhysicsSystem::SolveActorCollisions(std::vector<IActor*>& Actors, bool bFireTriggerEvents)
{
	std::vector<FContact> contacts;
	const int Count = static_cast<int>(Actors.size());

	for (int i = 0; i < Count; ++i)
	{
		IActor* A = Actors[i];
		if (!A) continue;

		for (int j = i + 1; j < Count; ++j)
		{
			IActor* B = Actors[j];
			if (!B) continue;

			for (int colliderA = 0; colliderA < A->GetColliderPartCount(); ++colliderA)
			{
				for (int colliderB = 0; colliderB < B->GetColliderPartCount(); ++colliderB)
				{
					if (!ShouldCollide(A, colliderA, B, colliderB))
					{
						continue;
					}

					FContact Contact{};
					if (!DetectContact(A, colliderA, B, colliderB, Contact))
					{
						continue;
					}

					if (Contact.bTrigger)
					{
						if (bFireTriggerEvents)
						{
							NotifyTrigger(A, B);
						}
						continue;
					}

					ResolveContact(Contact);
				}
			}
		}
	}
}

void UPhysicsSystem::SolveCircleWall(IActor* Actor, int ColliderIndex, const FWorldBounds& Bounds)
{
	if (!Actor)
		return;

	FWorldBounds EffectiveBounds = Bounds;

	if (dynamic_cast<Ball*>(Actor))
	{
		EffectiveBounds.top += 0.03f; // 공만 바닥을 0.03만큼 위로
	}

	const FColliderPart& part = Actor->GetColliderPart(ColliderIndex);
	if (part.Shape != ECollisionShape::Circle || !part.bEnabled)
	{
		return;
	}

	const EBodyType Type = Actor->GetBodyType();
	if (Type == EBodyType::Static)
	{
		return;
	}

	FVector Position = Actor->GetPosition();
	FVector Velocity = Actor->GetVelocity();

	const bool ApplyRotation = Actor->GetApplyRotation();
	float AngularVelocity = ApplyRotation ? Actor->GetAngularVelocity() : 0.0f;

	const FVector Center = Actor->GetColliderWorldCenter(ColliderIndex);
	const float Radius = part.Radius;
	const float InvMass = GetEffectiveInvMass(Actor);
	const float InvInertia = ApplyRotation ? GetEffectiveInvInertia(Actor) : 0.0f;

	auto ResolveWall = [&](const FVector& Normal, const FVector& ContactPoint, float Penetration)
		{
			Position.x += Normal.x * Penetration;
			Position.y += Normal.y * Penetration;
			Position.z = 0.0f;

			const bool bFloorOrCeil = std::fabs(Normal.y) > 0.5f;

			if (Type == EBodyType::Kinematic)
			{
				const float Vn = Dot2(Velocity, Normal);
				if (Vn < 0.0f)
				{
					Velocity.x -= Normal.x * Vn;
					Velocity.y -= Normal.y * Vn;
					Velocity.z = 0.0f;
				}

				const FVector Tangent(-Normal.y, Normal.x, 0.0f);
				const float Vt = Dot2(Velocity, Tangent);

				Velocity.x -= Tangent.x * (Vt * WALL_FRICTION);
				Velocity.y -= Tangent.y * (Vt * WALL_FRICTION);
				Velocity.z = 0.0f;

				if (bFloorOrCeil)
				{
					const FVector Tangent2(-Normal.y, Normal.x, 0.0f);
					const float Vt2 = Dot2(Velocity, Tangent2);

					Velocity.x -= Tangent2.x * (Vt2 * WALL_FRICTION);
					Velocity.y -= Tangent2.y * (Vt2 * WALL_FRICTION);
					Velocity.z = 0.0f;
				}
				return;
			}

			if (InvMass <= EPSILON)
			{
				return;
			}

			const float VelAlongNormal = Dot2(Velocity, Normal);
			if (VelAlongNormal >= 0.0f)
			{
				return;
			}

			const float Jn = -(1.0f + Actor->GetRestitution()) * VelAlongNormal / InvMass;

			Velocity.x += Normal.x * Jn * InvMass;
			Velocity.y += Normal.y * Jn * InvMass;
			Velocity.z = 0.0f;

			if (bFloorOrCeil)
			{
				const FVector r(ContactPoint.x - Position.x, ContactPoint.y - Position.y, 0.0f);

				FVector ContactVelocity = Velocity;
				ContactVelocity.x += -AngularVelocity * r.y;
				ContactVelocity.y += AngularVelocity * r.x;

				const FVector Tangent(-Normal.y, Normal.x, 0.0f);
				const float Vt = Dot2(ContactVelocity, Tangent);

				const float rxt = CrossVV(r, Tangent);
				const float KT = InvMass + (rxt * rxt) * InvInertia;
				if (KT > EPSILON)
				{
					float Jt = -Vt / KT;
					const float MaxFriction = WALL_FRICTION * std::fabs(Jn);
					Jt = (std::max)(-MaxFriction, (std::min)(Jt, MaxFriction));

					Velocity.x += Tangent.x * Jt * InvMass;
					Velocity.y += Tangent.y * Jt * InvMass;
					Velocity.z = 0.0f;

					const FVector frictionImpulse(Tangent.x * Jt, Tangent.y * Jt, 0.0f);
					AngularVelocity += CrossVV(r, frictionImpulse) * InvInertia;
				}
			}
		};

	if (Center.x - Radius < EffectiveBounds.left)
	{
		const float Penetration = EffectiveBounds.left - (Center.x - Radius);
		ResolveWall(FVector(1.0f, 0.0f, 0.0f), FVector(EffectiveBounds.left, Center.y, 0.0f), Penetration);
		Actor->OnHitWall();
	}

	if (Center.x + Radius > EffectiveBounds.right)
	{
		const float Penetration = (Center.x + Radius) - EffectiveBounds.right;
		ResolveWall(FVector(-1.0f, 0.0f, 0.0f), FVector(EffectiveBounds.right, Center.y, 0.0f), Penetration);
		Actor->OnHitWall();
	}

	if (Center.y - Radius < EffectiveBounds.top)
	{
		const float Penetration = EffectiveBounds.top - (Center.y - Radius);
		ResolveWall(FVector(0.0f, 1.0f, 0.0f), FVector(Center.x, EffectiveBounds.top, 0.0f), Penetration);
		Velocity.x *= (1.0f - LINEAR_DAMPING);
		Actor->OnHitWall();
	}

	if (Center.y + Radius > EffectiveBounds.bottom)
	{
		const float Penetration = (Center.y + Radius) - EffectiveBounds.bottom;
		ResolveWall(FVector(0.0f, -1.0f, 0.0f), FVector(Center.x, EffectiveBounds.bottom, 0.0f), Penetration);
		Velocity.x *= (1.0f - LINEAR_DAMPING);
		Actor->OnHitWall();
	}

	Actor->SetPosition(Position);

	if (Type != EBodyType::Static)
	{
		Actor->SetVelocity(Velocity);
	}

	if (Type == EBodyType::Dynamic)
	{
		if (ApplyRotation)
			Actor->SetAngularVelocity(AngularVelocity);
		else
			Actor->SetAngularVelocity(0.0f);
	}
}

void UPhysicsSystem::SolveAABBWall(IActor* Actor, int ColliderIndex, const FWorldBounds& Bounds)
{
	if (!Actor)
		return;

	const FColliderPart& part = Actor->GetColliderPart(ColliderIndex);
	if (part.Shape != ECollisionShape::AABB || !part.bEnabled)
		return;

	const EBodyType Type = Actor->GetBodyType();
	if (Type == EBodyType::Static)
	{
		return;
	}

	FVector Position = Actor->GetPosition();
	FVector Velocity = Actor->GetVelocity();

	const FVector Center = Actor->GetColliderWorldCenter(ColliderIndex);
	const FVector HW = part.HalfExtent;

	auto ResolveWall = [&](const FVector& Normal, float Penetration)
		{
			Position.x += Normal.x * Penetration;
			Position.y += Normal.y * Penetration;
			Position.z = 0.0f;

			const float Vn = Dot2(Velocity, Normal);
			if (Vn < 0.0f)
			{
				if (Type == EBodyType::Kinematic)
				{
					Velocity.x -= Normal.x * Vn;
					Velocity.y -= Normal.y * Vn;
					Velocity.z = 0.0f;
				}
				else
				{
					const float Jn = -(1.0f + Actor->GetRestitution()) * Vn;
					Velocity.x += Normal.x * Jn;
					Velocity.y += Normal.y * Jn;
					Velocity.z = 0.0f;
				}
			}

			const bool bFloorOrCeil = std::fabs(Normal.y) > 0.5f;
			if (bFloorOrCeil)
			{
				const FVector Tangent(-Normal.y, Normal.x, 0.0f);
				const float Vt = Dot2(Velocity, Tangent);

				Velocity.x -= Tangent.x * (Vt * WALL_FRICTION);
				Velocity.y -= Tangent.y * (Vt * WALL_FRICTION);
				Velocity.z = 0.0f;
			}
		};

	if (Center.x - HW.x < Bounds.left)
	{
		const float Penetration = Bounds.left - (Center.x - HW.x);
		ResolveWall(FVector(1.0f, 0.0f, 0.0f), Penetration);
	}

	if (Center.x + HW.x > Bounds.right)
	{
		const float Penetration = (Center.x + HW.x) - Bounds.right;
		ResolveWall(FVector(-1.0f, 0.0f, 0.0f), Penetration);
	}

	if (Center.y - HW.y < Bounds.top)
	{
		const float Penetration = Bounds.top - (Center.y - HW.y);
		ResolveWall(FVector(0.0f, 1.0f, 0.0f), Penetration);
		Velocity.x *= (1.0f - LINEAR_DAMPING);
	}

	if (Center.y + HW.y > Bounds.bottom)
	{
		const float Penetration = (Center.y + HW.y) - Bounds.bottom;
		ResolveWall(FVector(0.0f, -1.0f, 0.0f), Penetration);
		Velocity.x *= (1.0f - LINEAR_DAMPING);
	}

	Actor->SetPosition(Position);

	if (Type != EBodyType::Static)
	{
		Actor->SetVelocity(Velocity);
	}
}

bool UPhysicsSystem::DetectContact(IActor* A, int ColliderA, IActor* B, int ColliderB, FContact& OutContact)
{
	if (!A || !B) return false;

	const EPairInteraction Interaction = GetPairInteraction(A, ColliderA, B, ColliderB);
	if (Interaction == EPairInteraction::None)
	{
		return false;
	}

	bool bHit = false;

	const ECollisionShape ShapeA = A->GetColliderPart(ColliderA).Shape;
	const ECollisionShape ShapeB = B->GetColliderPart(ColliderB).Shape;

	if (ShapeA == ECollisionShape::Circle && ShapeB == ECollisionShape::Circle)
	{
		bHit = DetectCircleCircle(A, ColliderA, B, ColliderB, OutContact);
	}
	else if (ShapeA == ECollisionShape::Circle && ShapeB == ECollisionShape::AABB)
	{
		bHit = DetectCircleAABB(A, ColliderA, B, ColliderB, OutContact);
	}
	else if (ShapeA == ECollisionShape::AABB && ShapeB == ECollisionShape::Circle)
	{
		bHit = DetectCircleAABB(B, ColliderB, A, ColliderA, OutContact);
		if (bHit)
		{
			std::swap(OutContact.A, OutContact.B);
			std::swap(OutContact.ColliderIndexA, OutContact.ColliderIndexB);
			OutContact.Normal.x *= -1.0f;
			OutContact.Normal.y *= -1.0f;
		}
	}
	else if (ShapeA == ECollisionShape::AABB && ShapeB == ECollisionShape::AABB)
	{
		bHit = DetectAABBAABB(A, ColliderA, B, ColliderB, OutContact);
	}

	if (!bHit) return false;

	OutContact.bTrigger = (Interaction == EPairInteraction::Trigger);
	OutContact.bValid = true;
	return true;
}

bool UPhysicsSystem::DetectCircleCircle(IActor* A, int ColliderA, IActor* B, int ColliderB, FContact& OutContact)
{
	const FVector PA = A->GetColliderWorldCenter(ColliderA);
	const FVector PB = B->GetColliderWorldCenter(ColliderB);

	const float RA = A->GetColliderPart(ColliderA).Radius;
	const float RB = B->GetColliderPart(ColliderB).Radius;

	FVector Delta;
	Delta.x = PB.x - PA.x;
	Delta.y = PB.y - PA.y;
	Delta.z = 0.0f;

	const float DistSq = LengthSq2(Delta);
	const float RadiusSum = RA + RB;

	if (DistSq > RadiusSum * RadiusSum)
	{
		return false;
	}

	const float Dist = Length2(Delta);
	const FVector Normal = (Dist > EPSILON) ? Normalize2(Delta) : FVector(1.0f, 0.0f, 0.0f);
	const float Penetration = RadiusSum - Dist;

    OutContact.A = A;
    OutContact.B = B;
    OutContact.Normal = Normal;
    OutContact.Penetration = Penetration;
    OutContact.Point = FVector(
        PA.x + Normal.x * (RA - 0.5f * Penetration),
        PA.y + Normal.y * (RA - 0.5f * Penetration),
        0.0f
    );
	OutContact.ColliderIndexA = ColliderA;
	OutContact.ColliderIndexB = ColliderB;

	Player* player1 = dynamic_cast<Player*>(A);
	Ball* ball1 = dynamic_cast<Ball*>(B);

    if (player1 && ball1) {
		player1->OnHitBall(ball1);
    }

	Player* player2 = dynamic_cast<Player*>(B);
	Ball* ball2 = dynamic_cast<Ball*>(A);

	if (player2 && ball2) {
		player2->OnHitBall(ball2);
	}

    return true;
}

bool UPhysicsSystem::DetectCircleAABB(IActor* Circle, int CircleIndex, IActor* Box, int BoxIndex, FContact& OutContact)
{
	if (!Circle || !Box) return false;

	const FVector PC = Circle->GetColliderWorldCenter(CircleIndex);
	const FVector PB = Box->GetColliderWorldCenter(BoxIndex);

	const float CRadius = Circle->GetColliderPart(CircleIndex).Radius;
	const FVector BHalfExtent = Box->GetColliderPart(BoxIndex).HalfExtent;

	const float MinX = PB.x - BHalfExtent.x;
	const float MaxX = PB.x + BHalfExtent.x;
	const float MinY = PB.y - BHalfExtent.y;
	const float MaxY = PB.y + BHalfExtent.y;

	const float ClosestX = (std::max)(MinX, (std::min)(PC.x, MaxX));
	const float ClosestY = (std::max)(MinY, (std::min)(PC.y, MaxY));

	const FVector Closest(ClosestX, ClosestY, 0.0f);
	FVector Delta(PC.x - Closest.x, PC.y - Closest.y, 0.0f);

	const float DistSq = LengthSq2(Delta);
	if (DistSq > CRadius * CRadius)
	{
		return false;
	}

	float Dist = Length2(Delta);
	FVector Normal;
	float Penetration = 0.0f;
	FVector ContactPoint = Closest;

	if (Dist > EPSILON)
	{
		Normal = Normalize2(Delta);
		Penetration = CRadius - Dist;
	}
	else
	{
		const float Left = PC.x - MinX;
		const float Right = MaxX - PC.x;
		const float Top = PC.y - MinY;
		const float Bottom = MaxY - PC.y;

		const float MinPen = (std::min)((std::min)(Left, Right), (std::min)(Top, Bottom));

		if (MinPen == Left) Normal = FVector(-1.0f, 0.0f, 0.0f);
		else if (MinPen == Right) Normal = FVector(1.0f, 0.0f, 0.0f);
		else if (MinPen == Top) Normal = FVector(0.0f, -1.0f, 0.0f);
		else Normal = FVector(0.0f, 1.0f, 0.0f);

		Penetration = CRadius + MinPen;
		ContactPoint = FVector(
			PC.x - Normal.x * CRadius,
			PC.y - Normal.y * CRadius,
			0.0f
		);
	}

	OutContact.A = Circle;
	OutContact.B = Box;
	OutContact.ColliderIndexA = CircleIndex;
	OutContact.ColliderIndexB = BoxIndex;
	OutContact.Normal = FVector(-Normal.x, -Normal.y, 0.0f);
	OutContact.Penetration = Penetration;
	OutContact.Point = ContactPoint;
	OutContact.bValid = true;
	return true;
}

bool UPhysicsSystem::DetectAABBAABB(IActor* A, int ColliderA, IActor* B, int ColliderB, FContact& OutContact)
{
	if (!A || !B) return false;

	const FVector PA = A->GetColliderWorldCenter(ColliderA);
	const FVector PB = B->GetColliderWorldCenter(ColliderB);

	const FVector AHE = A->GetColliderPart(ColliderA).HalfExtent;
	const FVector BHE = B->GetColliderPart(ColliderB).HalfExtent;

	const float dx = PB.x - PA.x;
	const float dy = PB.y - PA.y;
	const float px = (AHE.x + BHE.x) - std::fabs(dx);
	const float py = (AHE.y + BHE.y) - std::fabs(dy);

	if (px <= 0.0f || py <= 0.0f) return false;

	FVector Normal;
	float Penetration;

	if (px < py)
	{
		Normal = (dx >= 0.0f) ? FVector(1.0f, 0.0f, 0.0f) : FVector(-1.0f, 0.0f, 0.0f);
		Penetration = px;
	}
	else
	{
		Normal = (dy >= 0.0f) ? FVector(0.0f, 1.0f, 0.0f) : FVector(0.0f, -1.0f, 0.0f);
		Penetration = py;
	}

	OutContact.A = A;
	OutContact.B = B;
	OutContact.ColliderIndexA = ColliderA;
	OutContact.ColliderIndexB = ColliderB;
	OutContact.Normal = Normal;
	OutContact.Penetration = Penetration;
	OutContact.Point = FVector(
		(PA.x + PB.x) * 0.5f,
		(PA.y + PB.y) * 0.5f,
		0.0f
	);
	OutContact.bValid = true;
	return true;
}

void UPhysicsSystem::ResolveContact(const FContact& Contact)
{
	if (!Contact.bValid || Contact.bTrigger)
	{
		return;
	}

	IActor* A = Contact.A;
	IActor* B = Contact.B;

	if (!A || !B)
	{
		return;
	}

	FVector PA = A->GetPosition();
	FVector PB = B->GetPosition();

	const FVector Normal = Contact.Normal;
	const float Penetration = Contact.Penetration;
	const FVector ContactPoint = Contact.Point;

	const float InvMassA = GetEffectiveInvMass(A);
	const float InvMassB = GetEffectiveInvMass(B);
	const float InvMassSum = InvMassA + InvMassB;

	if (InvMassSum <= EPSILON)
	{
		return;
	}

	const float CorrectionMag =
		POSITION_PERCENT * (std::max)(Penetration - POSITION_SLOP, 0.0f) / InvMassSum;

	const FVector Correction(Normal.x * CorrectionMag, Normal.y * CorrectionMag, 0.0f);

	PA.x -= Correction.x * InvMassA;
	PA.y -= Correction.y * InvMassA;
	PA.z = 0.0f;
	A->SetPosition(PA);

	PB.x += Correction.x * InvMassB;
	PB.y += Correction.y * InvMassB;
	PB.z = 0.0f;
	B->SetPosition(PB);

	FVector VA = A->GetVelocity();
	FVector VB = B->GetVelocity();

	const bool RotateA = A->GetApplyRotation() && A->GetCollisionShape() != ECollisionShape::AABB;
	const bool RotateB = B->GetApplyRotation() && B->GetCollisionShape() != ECollisionShape::AABB;

	float WA = RotateA ? A->GetAngularVelocity() : 0.0f;
	float WB = RotateB ? B->GetAngularVelocity() : 0.0f;

	const float InvIA = RotateA ? GetEffectiveInvInertia(A) : 0.0f;
	const float InvIB = RotateB ? GetEffectiveInvInertia(B) : 0.0f;

	const FVector rA(ContactPoint.x - PA.x, ContactPoint.y - PA.y, 0.0f);
	const FVector rB(ContactPoint.x - PB.x, ContactPoint.y - PB.y, 0.0f);

	const FVector ExtraVA = A->GetColliderPart(Contact.ColliderIndexA).ExtraVelocity;
	const FVector ExtraVB = B->GetColliderPart(Contact.ColliderIndexB).ExtraVelocity;

	FVector ContactVA = VA;
	ContactVA.x += -WA * rA.y + ExtraVA.x;
	ContactVA.y += WA * rA.x + ExtraVA.y;
	ContactVA.z = 0.0f;

	FVector ContactVB = VB;
	ContactVB.x += -WB * rB.y + ExtraVB.x;
	ContactVB.y += WB * rB.x + ExtraVB.y;
	ContactVB.z = 0.0f;

	FVector RV(ContactVB.x - ContactVA.x, ContactVB.y - ContactVA.y, 0.0f);

	const float VelAlongNormal = Dot2(RV, Normal);
	if (VelAlongNormal > 0.0f)
	{
		return;
	}

	const float rAxN = CrossVV(rA, Normal);
	const float rBxN = CrossVV(rB, Normal);

	const float KN = InvMassA + InvMassB + (rAxN * rAxN) * InvIA + (rBxN * rBxN) * InvIB;
	if (KN <= EPSILON)
	{
		return;
	}

	const float Restitution = 0.5f * (A->GetRestitution() + B->GetRestitution());
	const float Jn = -(1.0f + Restitution) * VelAlongNormal / KN;
	const FVector NormalImpulse(Normal.x * Jn, Normal.y * Jn, 0.0f);

	VA.x -= NormalImpulse.x * InvMassA;
	VA.y -= NormalImpulse.y * InvMassA;
	VA.z = 0.0f;
	WA -= CrossVV(rA, NormalImpulse) * InvIA;

	VB.x += NormalImpulse.x * InvMassB;
	VB.y += NormalImpulse.y * InvMassB;
	VB.z = 0.0f;
	WB += CrossVV(rB, NormalImpulse) * InvIB;

	ContactVA = VA;
	ContactVA.x += -WA * rA.y + ExtraVA.x;
	ContactVA.y += WA * rA.x + ExtraVA.y;
	ContactVA.z = 0.0f;

	ContactVB = VB;
	ContactVB.x += -WB * rB.y + ExtraVB.x;
	ContactVB.y += WB * rB.x + ExtraVB.y;
	ContactVB.z = 0.0f;

	RV.x = ContactVB.x - ContactVA.x;
	RV.y = ContactVB.y - ContactVA.y;
	RV.z = 0.0f;

	FVector Tangent(
		RV.x - Normal.x * Dot2(RV, Normal),
		RV.y - Normal.y * Dot2(RV, Normal),
		0.0f
	);

	const float TangentLen = Length2(Tangent);
	if (TangentLen > EPSILON)
	{
		Tangent = Normalize2(Tangent);

		const float rAxT = CrossVV(rA, Tangent);
		const float rBxT = CrossVV(rB, Tangent);

		const float KT = InvMassA + InvMassB + (rAxT * rAxT) * InvIA + (rBxT * rBxT) * InvIB;
		if (KT > EPSILON)
		{
			float Jt = -Dot2(RV, Tangent) / KT;
			const float MaxFriction = BODY_FRICTION * std::fabs(Jn);
			Jt = (std::max)(-MaxFriction, (std::min)(Jt, MaxFriction));

			const FVector TangentImpulse(Tangent.x * Jt, Tangent.y * Jt, 0.0f);

			VA.x -= TangentImpulse.x * InvMassA;
			VA.y -= TangentImpulse.y * InvMassA;
			VA.z = 0.0f;
			WA -= CrossVV(rA, TangentImpulse) * InvIA;

			VB.x += TangentImpulse.x * InvMassB;
			VB.y += TangentImpulse.y * InvMassB;
			VB.z = 0.0f;
			WB += CrossVV(rB, TangentImpulse) * InvIB;
		}
	}

	if (IsDynamic(A))
	{
		A->SetVelocity(VA);
		A->SetAngularVelocity(RotateA ? WA : 0.0f);
	}

	if (IsDynamic(B))
	{
		B->SetVelocity(VB);
		B->SetAngularVelocity(RotateB ? WB : 0.0f);
	}
}

EPairInteraction UPhysicsSystem::GetPairInteraction(IActor* A, int ColliderA, IActor* B, int ColliderB) const
{
	if (!A || !B)
	{
		return EPairInteraction::None;
	}

	const ECollisionMode ModeA = A->GetColliderPart(ColliderA).Mode;
	const ECollisionMode ModeB = B->GetColliderPart(ColliderB).Mode;

	if (ModeA == ECollisionMode::Ignore || ModeB == ECollisionMode::Ignore)
	{
		return EPairInteraction::None;
	}

	if (ModeA == ECollisionMode::Block && ModeB == ECollisionMode::Block)
	{
		return EPairInteraction::Block;
	}

	return EPairInteraction::Trigger;
}

void UPhysicsSystem::NotifyTrigger(IActor* A, IActor* B)
{
	if (!A || !B)
	{
		return;
	}

	A->OnTrigger(B);
	B->OnTrigger(A);
}

bool UPhysicsSystem::ShouldCollide(IActor* A, int ColliderA, IActor* B, int ColliderB) const
{
	if (!A || !B || A == B)
	{
		return false;
	}

	const EPairInteraction Interaction = GetPairInteraction(A, ColliderA, B, ColliderB);

	if (Interaction == EPairInteraction::None)
	{
		return false;
	}

	if (Interaction == EPairInteraction::Trigger)
	{
		return true;
	}

	return IsDynamic(A) || IsDynamic(B);
}

void UPhysicsSystem::UpdateColliderExtraVelocities(std::vector<IActor*>& Actors, float DeltaTime)
{
	if (DeltaTime <= EPSILON)
	{
		return;
	}

	for (IActor* Actor : Actors)
	{
		if (!Actor) continue;

		for (int i = 0; i < Actor->GetColliderPartCount(); ++i)
		{
			FColliderPart& Part = Actor->GetColliderPart(i);
			if (!Part.bEnabled)
				continue;

			if (!Part.bHasPrevLocalOffset)
			{
				Part.PrevLocalOffset = Part.LocalOffset;
				Part.ExtraVelocity = FVector(0.0f, 0.0f, 0.0f);
				Part.bHasPrevLocalOffset = true;
				continue;
			}

			Part.ExtraVelocity.x = (Part.LocalOffset.x - Part.PrevLocalOffset.x) / DeltaTime;
			Part.ExtraVelocity.y = (Part.LocalOffset.y - Part.PrevLocalOffset.y) / DeltaTime;
			Part.ExtraVelocity.z = 0.0f;
		}
	}
}

void UPhysicsSystem::CommitColliderLocalOffsets(std::vector<IActor*>& Actors)
{
	for (IActor* Actor : Actors)
	{
		if (!Actor) continue;

		for (int i = 0; i < Actor->GetColliderPartCount(); ++i)
		{
			FColliderPart& Part = Actor->GetColliderPart(i);
			if (!Part.bEnabled)
				continue;

			Part.PrevLocalOffset = Part.LocalOffset;
			Part.bHasPrevLocalOffset = true;
		}
	}
}

float UPhysicsSystem::Dot2(const FVector& A, const FVector& B) const
{
	return A.x * B.x + A.y * B.y;
}

float UPhysicsSystem::LengthSq2(const FVector& V) const
{
	return Dot2(V, V);
}

float UPhysicsSystem::Length2(const FVector& V) const
{
	return std::sqrt(LengthSq2(V));
}

FVector UPhysicsSystem::Normalize2(const FVector& V) const
{
	const float Len = Length2(V);
	if (Len <= EPSILON)
	{
		return FVector(1.0f, 0.0f, 0.0f);
	}

	return FVector(V.x / Len, V.y / Len, 0.0f);
}

float UPhysicsSystem::CrossVV(const FVector& A, const FVector& B) const
{
	return A.x * B.y - A.y * B.x;
}