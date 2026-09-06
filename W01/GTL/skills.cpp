#include "skills.h"
#include "Particles.h"
#include "Player.h"
#include "AudioManager.h"

// SKill A_______________________________________________________________________
void SkillA::Activate(SkillContext& ctx) {
	// Activate the skill, e.g., spawn particles, apply effects, etc.

	using namespace SkillAConstants;
	activated = true;
	recovering = false;
	standByElapsed = 0;	// Reset the standby timer
	recoveryTimer = 0;	// Reset the recovery timer
	context = ctx;
	auto owner = ctx.owner;
	auto target = ctx.target;

	// Spawn a particle effect at the player's position
	owner->SetVelocity(FVector(0, 0, 0));	// Stop the player when the skill is activated
	owner->SetApplyGravity(false);			// Disable gravity for the player during the skill effect
	target->SetVelocity(FVector(0, 0, 0));	// Stop the ball when the skill is activated
	target->SetApplyGravity(false);			// Disable gravity for the ball during the skill effect
	target->SetAngularVelocity(0);			// Stop the ball's rotation when the skill is activated

	owner->SetControllable(false);			// Disable player control while the skill A effect is active
	context.target->SetCollisionMode(ECollisionMode::Ignore);	// Disable ball collision while the skill A effect is active
	switch (owner->GetPlayerID()) {
	case 1:
		// Player 1
		owner->SetPosition(FVector(player1NewPositionX, player1NewPositionY, 0));
		target->SetPosition(FVector(ball1NewPositionX, ball1NewPositionY, 0));
		break;
	case 2:
		// Player 2
		owner->SetPosition(FVector(player2NewPositionX, player2NewPositionY, 0));
		target->SetPosition(FVector(ball2NewPositionX, ball2NewPositionY, 0));
		break;
	default:
		break;
	}

	auto &collection = owner->GetParticleCollections();
	ParticleSequence::SpecialMove(owner->GetPosition(), collection);
	AudioManager::getInstance()->PlaySFX("skill_common");
}

void SkillA::Update(float dt) {
	// Update the skill's state, e.g., reduce cooldown, manage active effects, etc.
	if (activated && !recovering) {
		// During the standby phase before the skill effect is launched
		context.target->SetVelocity(FVector(0, 0, 0));	// Keep the ball stationary during the standby phase

		if (standByElapsed < skillStandByDuration) {
			standByElapsed += dt;
		}

		if (standByElapsed >= skillStandByDuration) {
			Launch();
		}
	}
	else if (recovering) {
		// During the recovery phase after the skill effect ends
		if (recoveryTimer < SkillAConstants::recoveryDuration) {
			recoveryTimer += dt;
		}
		else {
			Recover();
			recovering = false;	// End the recovery phase after it's done
		}
	}
}

void SkillA::Launch() {
	using namespace SkillAConstants;
	// Launch the skill's effect, e.g., apply damage, modify player state, etc.
	if (IsReadyToLaunch()) {
		activated = false;
		recovering = true;
		standByElapsed = 0;	// Reset the standby timer for potential reuse

		// Re-enable ball collision right when the skill is launched
		context.target->SetCollisionMode(ECollisionMode::Block);
		context.target->SetApplyGravity(true);	// Re-enable gravity for the ball right when the skill is launched

		auto owner = context.owner;
		auto target = context.target;
		switch (owner->GetPlayerID()) {
		case 1:
			// Player 1
			target->SetVelocity(FVector(ball1LaunchVelocityX, ball1LaunchVelocityY, 0.0f));
			break;
		case 2:
			// Player 2
			target->SetVelocity(FVector(ball2LaunchVelocityX, ball2LaunchVelocityY, 0.0f));
			break;
		default:
			break;
		}
		AudioManager::getInstance()->PlaySFX("skillA_launch", 1.5f);
		std::cout << "[SKillA] Launched\n";
		// Implement the actual effect of the skill here
	}
}

void SkillA::Recover() {
	// Handle the recovery phase after the skill effect ends, e.g., reset player state, start cooldown, etc.
	recovering = false;
	recoveryTimer = 0;	// Reset the recovery timer for potential reuse

	auto owner = context.owner;
	owner->SetApplyGravity(true);	// Re-enable gravity for the player after the skill effect ends
	owner->SetControllable(true);	// Re-enable player control after the skill effect ends
}

// SKill B_______________________________________________________________________
void SkillB::Activate(SkillContext& ctx) {
	// Activate the skill, e.g., spawn particles, apply effects, etc.

	using namespace SkillBConstants;
	activated = true;
	recovering = false;
	standByElapsed = 0;	// Reset the standby timer
	recoveryTimer = 0;	// Reset the recovery timer
	context = ctx;
	auto owner = ctx.owner;
	auto target = ctx.target;

	// Spawn a particle effect at the player's position
	owner->SetVelocity(FVector(0, 0, 0));	// Stop the player when the skill is activated
	owner->SetApplyGravity(false);			// Disable gravity for the player during the skill effect
	target->SetVelocity(FVector(0, 0, 0));	// Stop the ball when the skill is activated
	target->SetApplyGravity(false);			// Disable gravity for the ball during the skill effect
	target->SetAngularVelocity(0);			// Stop the ball's rotation when the skill is activated

	owner->SetControllable(false);			// Disable player control while the skill A effect is active
	context.target->SetCollisionMode(ECollisionMode::Ignore);	// Disable ball collision while the skill A effect is active
	switch (owner->GetPlayerID()) {
	case 1:
		// Player 1
		owner->SetPosition(FVector(player1NewPositionX, player1NewPositionY, 0));
		target->SetPosition(FVector(ball1NewPositionX, ball1NewPositionY, 0));
		break;
	case 2:
		// Player 2
		owner->SetPosition(FVector(player2NewPositionX, player2NewPositionY, 0));
		target->SetPosition(FVector(ball2NewPositionX, ball2NewPositionY, 0));
		break;
	default:
		break;
	}

	auto& collection = owner->GetParticleCollections();
	ParticleSequence::SpecialMove(owner->GetPosition(), collection);
	AudioManager::getInstance()->PlaySFX("skill_common");
}

void SkillB::Update(float dt) {
	// Update the skill's state, e.g., reduce cooldown, manage active effects, etc.
	if (activated && !recovering) {
		// During the standby phase before the skill effect is launched
		context.target->SetVelocity(FVector(0, 0, 0));	// Keep the ball stationary during the standby phase

		if (standByElapsed < skillStandByDuration) {
			standByElapsed += dt;
		}

		if (standByElapsed >= skillStandByDuration) {
			Launch();
		}
	}
	else if (recovering) {
		// During the recovery phase after the skill effect ends
		if (recoveryTimer < SkillBConstants::recoveryDuration) {
			recoveryTimer += dt;
		}
		else {
			Recover();
			recovering = false;	// End the recovery phase after it's done
		}
	}
}

void SkillB::Recover() {
	// Handle the recovery phase after the skill effect ends, e.g., reset player state, start cooldown, etc.
	recovering = false;
	recoveryTimer = 0;	// Reset the recovery timer for potential reuse

	auto owner = context.owner;
	owner->SetApplyGravity(true);	// Re-enable gravity for the player after the skill effect ends
	owner->SetControllable(true);	// Re-enable player control after the skill effect ends
}

void SkillB::Launch() {
	using namespace SkillBConstants;
	// Launch the skill's effect, e.g., apply damage, modify player state, etc.
	if (IsReadyToLaunch()) {
		activated = false;
		recovering = true;
		standByElapsed = 0;	// Reset the standby timer for potential reuse

		// Re-enable ball collision right when the skill is launched
		context.target->SetCollisionMode(ECollisionMode::Block);
		context.target->SetApplyGravity(true);	// Re-enable gravity for the ball right when the skill is launched

		auto owner = context.owner;
		auto target = context.target;
		switch (owner->GetPlayerID()) {
		case 1:
			// Player 1
			target->SetVelocity(FVector(ball1LaunchVelocityX, ball1LaunchVelocityY, 0.0f));
			break;
		case 2:
			// Player 2
			target->SetVelocity(FVector(ball2LaunchVelocityX, ball2LaunchVelocityY, 0.0f));
			break;
		default:
			break;
		}
		AudioManager::getInstance()->PlaySFX("skillA_launch", 1.5f);
	}
}


// SKill C_______________________________________________________________________


// SKill D_______________________________________________________________________