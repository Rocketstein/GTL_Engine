#pragma once
#include "Player.h"

class GameController
{
public:
	virtual ~GameController() = default;
	virtual void SetEnabled(bool) = 0;
	virtual bool GetEnabled() = 0;
	virtual void SetControllTarget(Player* target) = 0;
	virtual void Update() = 0;
};