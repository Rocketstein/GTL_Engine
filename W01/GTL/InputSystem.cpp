#include "InputSystem.h"

bool InputSystem::currentStates[256] = { false };
bool InputSystem::prevStates[256] = { false };