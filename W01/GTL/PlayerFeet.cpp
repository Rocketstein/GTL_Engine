#include "PlayerFeet.h"

void PlayerFeet::Render(URenderer* renderer)
{
    if (!renderer || !GetSprite()) return;

    renderer->DrawSprite(GetSprite(), GetPosition(), GetRotation(), GetScale(), false);
}