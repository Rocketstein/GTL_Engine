#pragma once

#include "Core/CoreTypes.h"

enum class ECollisionChannel
{
    WorldStatic,
    WorldDynamic,
    Projectile,
    Enemy,
    Player,
    Trigger,
    Pickup,
    FlyingEnemy,
    PickupSensor,
    PlayerHitbox,

    Count
};

enum class ECollisionResponse : uint8
{
    Ignore,
    Overlap,
    Block
};
