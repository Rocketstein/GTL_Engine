#include "GoalTrigger.h"

void GoalTrigger::OnTrigger(IActor* Other)
{
    float AV = Other->GetAngularVelocity();
    Other->SetAngularVelocity(AV - GetPushDirection() * power);
}

void GoalTrigger::SetPushDirection(float newDirection)
{
    Direction = newDirection;
}

float GoalTrigger::GetPushDirection() const
{
    return Direction;
}

void GoalTrigger::SetPushPower(float newPower)
{
    power = newPower;
}

float GoalTrigger::GetPushPower() const
{
    return power;
}


