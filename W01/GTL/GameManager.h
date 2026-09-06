#pragma once
#include "IActor.h"
#include "Player.h"
#include "Particles.h"
#include "Constants.h"
#include "AudioManager.h"

using namespace GM_defaults;

class GameManager : public ParticleEmitter
{
public: 
	GameState* currentState;

	GameManager(GameState *gameState) : timeLeft(GM_defaults::InitialTimeLeft), currentState(gameState) {}
	~GameManager() = default;

	// Temporarily pauses the game for a specified duration (e.g., after a goal or at kickoff)
	void pauseGame(float pauseTime);

	// Resets the game, but does not reset the scores and time left. Used for kickoff after a goal.
	void resetGame(IActor* leftPlayer, IActor* rightplayer, IActor* ball);

	// Checks if the game has ended and returns the result: 0 for left player win, 1 for right player win, 2 for tie, -1 for ongoing game
	int checkVictory();

	// Updates the remaining time and checks for game over condition. Should be called every frame with the elapsed time since the last update.
	void updateTime(float deltaTime);

	// Updates the pause timer and transitions back to the Playing state when the pause duration has elapsed.
	void updatePause(float deltaTime);

	int checkGoalScored(IActor *ball); // Checks if a goal has been scored by either player. This should be called every frame to detect scoring events.

	// Handles the logic when a goal is scored, updating scores and resetting the game state for kickoff. The parameter indicates which player scored (true for left player, false for right player).
	void handleGoals(std::vector<IActor*>& actors, AudioManager* audioManager);

	// Handles player input, such as pausing the game or restarting it. This function should be called in the main game loop to process relevant input messages.
	void handlePlayerInput();

	int GetScore(int playerID);
	// Main game loop function that updates the game state, processes input, updates actors, and renders them.
	//void tick(float deltaTime, URenderer* renderer);

	void resetMatch();

	int GetLeftScore() const { return leftScore; }
	int GetRightScore() const { return rightScore; }
	float GetTimeLeft() const { return timeLeft; }

private:

	int leftScore = 0;
	int rightScore = 0;
	float timeLeft;
	float pauseTimer = 0.0f; // Timer to track remaining pause time
};