#pragma once
#include "Object/Object.h"


// In Engine loop, GameInstance is initialized and manages data shared throughout the game.
class UGameInstance : public UObject
{
public:
	DECLARE_CLASS(UGameInstance, UObject)

	virtual void Init() {}
	virtual void Shutdown() {}
};
