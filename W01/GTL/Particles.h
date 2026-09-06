#pragma once
#include "IActor.h"
#include <vector>
#include <cstdlib>
#include <memory>

// Note: This class currently depends of plain color shader pipeline

using std::vector;
using std::unique_ptr;
using std::make_unique;

class ParticleObject : public IActor {
public:
	ParticleObject(FVector position, FVector velocity, float radius, float lifespan, FVector4 color);
	void Render(URenderer* renderer, ID3D11Buffer* vertexBuffer);
	void Update(float dt);

	float lifespan_ = 0;
private:
	FVector4 rgba = {0, 0, 0, 0};
};

class ParticleCollection {
public:
	ParticleCollection(FVector center, size_t numParticles, float spawnRadius, float maxLifespan, ParticleType pType, float startDelay, FVector4 UseSingleColor);

	// Update all particles per frame
	void updateParticles(float dt);

	// Draw all particles beloning to this colleciton
	void renderParticles(URenderer* renderer, ID3D11Buffer* vertexBuffer);

	// Check the remaining lifespan of this collection
	bool expired() const;
private:
	ParticleType type		= Outward;
	size_t numParticles_	= 0;
	float maxLifespan_		= 0;
	float life				= 0;
	float spawnRadius_		= 0;
	FVector center_			= { 0, 0, 0 };
	FVector4 UseSingleColor = { 0, 0, 0, 0 };
	float startDelay_		= 0;		// Do not start particle effect until the delay has been elapsed
	vector<unique_ptr<ParticleObject>> particleObjects;

	// Spawn random particles using given fields
	void spawnRandomParticles();

	// Uses rejection sampling to pick a point in a circular range
	FVector generateRandomFVectorSphere(FVector center, float radius);

	// Picks a point in a cubical range
	FVector generateRandomFVectorCube(FVector center, float radius);

	// FVector Comparison using epsilon shielding
	bool FVecCloseEnough(const FVector& a, const FVector& b) const;
};

// Cover methods of particle effects
class ParticleSequence {
public:
	// Dragon CRASH!!
	static void SpecialMove(const FVector& center, vector<ParticleCollection>& collection);

	// Don't forget to couple this function with a knockback. Perhaps a pause too.
	static void Stun(const FVector& center, vector<ParticleCollection>& collection);

	// A proper ceremony
	static void GoalEffect(const FVector& center, vector<ParticleCollection>& collection);
};

// Emitter interface for actors to spawn particles
class ParticleEmitter {
public:
	void UpdateParticleCollections(float dt);

	void RenderParticleCollections(URenderer* renderer, ID3D11Buffer* vertexBuffer);

	vector<ParticleCollection>& GetParticleCollections() { return particleCollection; }

protected:
	vector<ParticleCollection> particleCollection;
};