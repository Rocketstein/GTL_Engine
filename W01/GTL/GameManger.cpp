#include "GameManager.h"
#include <iostream>

void GameManager::pauseGame(float pauseTime)
{
	pauseTimer = pauseTime;
	*currentState = (pauseTime > 0.0f) ? Paused : Playing;
}

void GameManager::resetGame(IActor *leftPlayer, IActor *rightPlayer, IActor* ball)
{
	Player* left = dynamic_cast<Player*>(leftPlayer);
	Player* right = dynamic_cast<Player*>(rightPlayer);
	if (!left || !right) {
		return;
	}

	left->SetPosition(FVector(leftPlayerX, leftPlayerY));
	right->SetPosition(FVector(rightPlayerX, rightPlayerY));
	ball->SetPosition(FVector(kickoffX, kickoffY));

	// Additionally, velocities to zero
	ball->Reset();
	leftPlayer->Reset();
	rightPlayer->Reset();
	pauseGame(kickoffPause);
}

int GameManager::checkVictory()
{
	if (timeLeft <= 0.0f)
	{
		*currentState = GameOver;
		if (leftScore > rightScore)
		{
			// Left player wins
			return 0;
		}
		else if (rightScore > leftScore)
		{
			// Right player wins
			return 1;
		}
		else
		{
			// It's a tie
			return 2;
		}
	}
	return -1; // Game is still ongoing
}

void GameManager::updateTime(float deltaTime) { 
	if (timeLeft > 0.0f) {
		timeLeft -= deltaTime;
		if (timeLeft < 0.0f) {
			timeLeft = 0.0f; // Ensure time doesn't go negative
		}
	}
	else {
		// Disable player controls and end the game
		// Check player states to reflect game over
		*currentState = GameOver;
	}
}

void GameManager::updatePause(float deltaTime) {
	if (*currentState == Paused) {
		pauseTimer -= deltaTime;
		if (pauseTimer <= 0.0f) {
			*currentState = Playing;
			AudioManager::getInstance()->PlaySFX("whistle");
		}
	}
}

void GameManager::handleGoals(std::vector<IActor*>& actors, AudioManager* audioManager) {
	auto whoScored = checkGoalScored(actors[2]);	// 0 = No goal scored
	if (whoScored) {
		audioManager->PlaySFX("crowd_reaction", 2.0f);
		// Goal scored, reset the game for kickoff
		// Add particle confetti to the ball location upon scoring a goal
		ParticleSequence::GoalEffect(actors[2]->GetPosition(), GetParticleCollections());
		resetGame(actors[0], actors[1], actors[2]);

		// Award Skill Gauge to opponent on goal scored
		Player* p1 = dynamic_cast<Player*>(actors[0]);
		Player* p2 = dynamic_cast<Player*>(actors[1]);
		switch (whoScored) {
		case 1:
			if (p1) { p1->AwardSkillGauge(skillGaugeCompensation); }
			break;
		case 2:
			if (p2) { p2->AwardSkillGauge(skillGaugeCompensation); }
			break;
		}
	}
}

int GameManager::checkGoalScored(IActor *ball)
{
	auto ballX = ball->GetPosition().x;
	auto ballY = ball->GetPosition().y;
	// Implement logic to check if the ball has entered either player's goal area
	if (ballX >= leftGoalX && ballX <= leftGoalX + goalWidth) {
		if (ballY >= goalY && ballY < goalY + goalHeight) {
			rightScore++;
			return 1; // Goal scored for right player
		}
	}
	else if (ballX >= rightGoalX && ballX <= rightGoalX + goalWidth) {
		if (ballY >= goalY && ballY < goalY + goalHeight) {
			leftScore++;
			return 2; // Goal scored for right player
		}
	}
	return 0; // Goal not scored
}

void GameManager::handlePlayerInput()
{
	// Handle player input based on the current game state
	if (*currentState == Playing) {
		// Process input for both players

	}
	else if (*currentState == Paused) {
		// Optionally allow certain inputs to resume the game or navigate menus

	}
	else if (*currentState == GameOver) {
		// Optionally allow input to restart the game or exit

	}
}

int GameManager::GetScore(int playerID)
{
	if (playerID == 1)
		return leftScore;
	else
		return rightScore;
}

void GameManager::resetMatch()
{
	leftScore = 0;
	rightScore = 0;
	timeLeft = InitialTimeLeft;
	pauseTimer = 0.0f;
	*currentState = Playing;
}