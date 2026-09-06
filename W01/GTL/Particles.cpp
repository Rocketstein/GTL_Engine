#include "Particles.h"

ParticleObject::ParticleObject(FVector position, FVector velocity, float radius, float lifespan, FVector4 color) {
	bApplyGravity = false;
	Position = position;
	Velocity = velocity;
	Scale = radius;
	lifespan_ = lifespan;
	rgba = color;
}

void ParticleObject::Render(URenderer* renderer, ID3D11Buffer* vertexBuffer) {
	if (!renderer) return;

	float tmp = 0; // Actual garbage variable. No need of angle for particles
	renderer->UpdateConstant(FVector4(Position.x, Position.y, Position.z, Scale), tmp, rgba);
	renderer->RenderPrimitive(vertexBuffer, 2400); // sizeof(sphere_vertices)
}

void ParticleObject::Update(float dt) {
	if (lifespan_ <= 0.0f) {
		// Expired
		return;
	}

	Position.x += Velocity.x * dt;
	Position.y += Velocity.y * dt;

	// Dim out as each particle age
	rgba.w -= (float)(1.0 / lifespan_);
}

//_______________________________________________________

ParticleCollection::ParticleCollection(FVector center, size_t numParticles, float spawnRadius, float maxLifespan, ParticleType pType, float startDelay, FVector4 useSingleColor) {
	center_ = center;
	numParticles_ = numParticles;
	spawnRadius_ = spawnRadius;
	maxLifespan_ = maxLifespan;
	life = maxLifespan;
	UseSingleColor = useSingleColor;
	type = pType;
	startDelay_ = startDelay;
	spawnRandomParticles();
}

FVector ParticleCollection::generateRandomFVectorSphere(FVector center, float radius) {
	FVector result;
	float x, y;

	do {
		x = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
		y = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
	} while (x * x + y * y > 1.0f);

	result.x = center.x + x * radius;
	result.y = center.y + y * radius;
	result.z = 0.0f; // Unused for now

	return result;
}

FVector ParticleCollection::generateRandomFVectorCube(FVector base, float radius) {
	FVector result;
	result.x = base.x + radius * (((float)rand() / RAND_MAX) * 2.0f - 1.0f);
	result.y = base.y + radius * (((float)rand() / RAND_MAX) * 2.0f - 1.0f);
	result.z = 0.0f; // Unused for now
	return result;
}

void ParticleCollection::spawnRandomParticles() {
	for (int i = 0; i < numParticles_; i++) {
		auto randPos = generateRandomFVectorSphere(center_, spawnRadius_);

		// Enforce outward velocity
		// Direction = from center to spawn position, normalized
        float dx = randPos.x - center_.x;
        float dy = randPos.y - center_.y;
        float len = sqrt(dx * dx + dy * dy);
        
        FVector randVel;
        if (len > 0.0001f) {
            // Normalize and scale by launch speed
			float speedVariance = particleLaunchSpeed * ((float)rand() / RAND_MAX);
			randVel = FVector(
				(dx / len) * (particleLaunchSpeed + speedVariance),
				(dy / len) * (particleLaunchSpeed + speedVariance),
				0.0f
			);

			if (type == Inward) {
				randVel.x *= -1;
				randVel.y *= -1;
				randVel.z *= -1;
			}
        } else {
            // Particle spawned exactly at center, skip this particle
			continue;
        }
		auto randRad = particleMaxRadius * ((float)rand() / RAND_MAX);
		float randLife = maxLifespan_ * ((float)rand() / RAND_MAX);
		
		FVector randRGB(0, 0, 0); 
		if (UseSingleColor.w == 1) {
			// Use pre-defined color
			randRGB.x = UseSingleColor.x;
			randRGB.y = UseSingleColor.y;
			randRGB.z = UseSingleColor.z;
		}
		else {
			// Randomize particle colors
			// Ensure bright colors for particles
			randRGB = generateRandomFVectorCube(FVector(0, 0, 0), 1);
			randRGB.x += particleBrightening;
			randRGB.y += particleBrightening;
			randRGB.z += particleBrightening;
		}
		particleObjects.push_back(make_unique<ParticleObject>(randPos, randVel, randRad, randLife, FVector4(randRGB.x, randRGB.y, randRGB.z, 1.0f)));
	}
}

void ParticleCollection::updateParticles(float dt) {
	if (startDelay_ > 0) {
		startDelay_ -= dt;
		return;
	}

	life -= dt;
	if (life <= 0) {
		// Collection expired
		return;
	}

	for (int i = particleObjects.size() - 1; i >= 0; i--) {
		particleObjects[i]->lifespan_ -= dt;
		
		// Remove dead particles
		if (particleObjects[i]->lifespan_ <= 0.0f) {
			particleObjects.erase(particleObjects.begin() + i);
			continue;
		}
		particleObjects[i]->Update(dt);
	}
}

void ParticleCollection::renderParticles(URenderer* renderer, ID3D11Buffer* vertexBuffer) {
	if (startDelay_ > 0) {
		return;
	}

	for (auto& p : particleObjects) {
		if (type == Inward && FVecCloseEnough(p->GetPosition(), center_)) {
			p->lifespan_ = 0;
		}
		p->Render(renderer, vertexBuffer);
	}
}

bool ParticleCollection::expired() const {
	return life <= 0;
}

bool ParticleCollection::FVecCloseEnough(const FVector& v1, const FVector& v2) const {
	float dx = v1.x - v2.x;
	float dy = v1.y - v2.y;
	float distSq = dx * dx + dy * dy;
	float th = particleMaxRadius * 2.0f; // roughly particle diameter
	return distSq < th * th;
}

//_________________________________________________

void ParticleEmitter::UpdateParticleCollections(float dt)
{
	for (int i = particleCollection.size() - 1; i >= 0; i--)
	{
		particleCollection[i].updateParticles(dt);
		if (particleCollection[i].expired())
		{
			particleCollection.erase(particleCollection.begin() + i);
		}
	}
}

void ParticleEmitter::RenderParticleCollections(URenderer* renderer, ID3D11Buffer* vertexBuffer)
{
	for (int i = particleCollection.size() - 1; i >= 0; i--)
	{
		particleCollection[i].renderParticles(renderer, vertexBuffer);
	}
}


//_________________________________________________

void ParticleSequence::GoalEffect(const FVector& center, vector<ParticleCollection>& collection) {
	ParticleCollection p(center, 250, 0.8f, 0.5f, Outward, 0.0f, FVector4(0, 0, 0, 0));
	collection.push_back(std::move(p));
}

void ParticleSequence::SpecialMove(const FVector& center, vector<ParticleCollection>& collection) {
	for (int i = 0; i < 4; i++) {
		ParticleCollection p(center, 250, 0.5f, 0.5f, Inward, 0.5f * i, FVector4(1, 1, 0, 1));
		collection.push_back(std::move(p));
	}
}

void ParticleSequence::Stun(const FVector& center, vector<ParticleCollection>& collection) {
	ParticleCollection p(center, 20, 0.3f, 0.2f, Outward, 0.0f, FVector4(1, 1, 1, 1));
	collection.push_back(std::move(p));
}