#pragma once
#include <vector>
#include "Structs.h"
#include "IActor.h"
#include "Player.h"
#include "Ball.h"

struct FContact
{
	IActor* A = nullptr;
	IActor* B = nullptr;

	int ColliderIndexA = -1;
	int ColliderIndexB = -1;

	FVector Normal{ 0.0f, 0.0f, 0.0f };
	FVector Point{ 0.0f, 0.0f, 0.0f };
	float Penetration = 0.0f;

	bool bTrigger = false;
	bool bValid = false;
};

class UPhysicsSystem
{
public:
	void Step(std::vector<IActor*>& Actors, const FWorldBounds& Bounds, float DeltaTime);

private:
	void Integrate(std::vector<IActor*>& Actors, float DeltaTime);
	void SolveWallCollisions(std::vector<IActor*>& Actors, const FWorldBounds& Bounds);
	void SolveActorCollisions(std::vector<IActor*>& Actors, bool bFireTriggerEvents);

	bool DetectContact(IActor* A, int ColliderA, IActor* B, int ColliderB, FContact& Outcontact);
	bool DetectCircleCircle(IActor* A, int ColliderA, IActor* B, int ColliderB, FContact& OutContact);
	bool DetectCircleAABB(IActor* Circle, int CircleIndex, IActor* Box, int BoxIndex, FContact& OutContact);
	bool DetectAABBAABB(IActor* A, int ColliderA, IActor* B, int ColliderB, FContact& OutContact);

	void SolveCircleWall(IActor* Actor, int ColliderIndex, const FWorldBounds& Bounds);
	void SolveAABBWall(IActor* Actor, int ColliderIndex, const FWorldBounds& Bounds);
	void ResolveContact(const FContact& Contact);

	bool ShouldCollide(IActor* A, int ColliderA, IActor* B, int ColliderB) const;

	void UpdateColliderExtraVelocities(std::vector<IActor*>& Actors, float DeltaTime);
	void CommitColliderLocalOffsets(std::vector<IActor*>& Actors);

	EPairInteraction GetPairInteraction(IActor* A, int ColliderA, IActor* B, int ColliderB) const;
	void NotifyTrigger(IActor* A, IActor* B);

	float Dot2(const FVector& A, const FVector& B) const;
	float LengthSq2(const FVector& V) const;
	float Length2(const FVector& V) const;
	FVector Normalize2(const FVector& V) const;
	float CrossVV(const FVector& A, const FVector& B) const;
};