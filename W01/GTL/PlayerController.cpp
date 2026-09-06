#include "PlayerController.h"
#include "InputSystem.h"

PlayerController::PlayerController() :
	controllTarget(nullptr),
	bEnabled(true)
{ }

PlayerController::~PlayerController()
{
}

void PlayerController::SetEnabled(bool)
{
	bEnabled = false;
}

bool PlayerController::GetEnabled()
{
	return bEnabled;
}

void PlayerController::SetControllTarget(Player* target)
{
	controllTarget = target;
}

void PlayerController::Update()
{
	if (!bEnabled)
		return;

	if (controllTarget == nullptr)
		return;

	for (auto e : actionMappings)
	{
		int keycode = e.keycode;
		FInputState inputState;
		inputState.Down = InputSystem::GetKeyDown(keycode);
		inputState.Hold = InputSystem::GetKey(keycode);
		inputState.Up = InputSystem::GetKeyDown(keycode);

		if (inputState.Down || inputState.Hold || inputState.Up)
		{
			switch (e.action)
			{
			case MoveLeft:
				controllTarget->OnLeft(inputState);
				break;
			case MoveRight:
				controllTarget->OnRight(inputState);
				break;
			case Jump:
				controllTarget->OnJump(inputState);
				break;
			case Kick:
				controllTarget->OnKick(inputState);
				break;
			case Skill:
				controllTarget->OnSkill(inputState);
				break;
			}
		}
	}
}

void PlayerController::SetInputMap(int playerId)
{
	if (playerId == 1)
	{
		SetInputActionMapping(EAction::MoveLeft, KEY_1P_MOVE_LEFT);
		SetInputActionMapping(EAction::MoveRight, KEY_1P_MOVE_RIGHT);
		SetInputActionMapping(EAction::Jump, KEY_1P_JUMP);
		SetInputActionMapping(EAction::Kick, KEY_1P_KICK);
		SetInputActionMapping(EAction::Skill, KEY_1P_SKILL);
	}
	else if(playerId == 2)
	{
		SetInputActionMapping(EAction::MoveLeft, KEY_2P_MOVE_LEFT);
		SetInputActionMapping(EAction::MoveRight, KEY_2P_MOVE_RIGHT);
		SetInputActionMapping(EAction::Jump, KEY_2P_JUMP);
		SetInputActionMapping(EAction::Kick, KEY_2P_KICK);
		SetInputActionMapping(EAction::Skill, KEY_2P_SKILL);
	}
}

void PlayerController::SetInputActionMapping(EAction action, int keycode)
{
	for (auto e : actionMappings)
	{
		if (e.action == action)
		{
			e.keycode = keycode;
			return;
		}
	}

	actionMappings.push_back({ action, keycode });
}
