#pragma once
#include <vector>
#include "Player.h"
#include "GameController.h"

class PlayerController : public GameController
{
public:
	PlayerController();
	~PlayerController();
	void SetEnabled(bool);
	bool GetEnabled();
	void SetControllTarget(Player* target);
	void Update();
	void SetInputMap(int playerId);
private:
	void SetInputActionMapping(EAction action, int keycode);
	bool bEnabled;
	std::vector<FActionKeyPair> actionMappings;
	Player* controllTarget;
};

