#pragma once
#include "Structs.h"
class UPrimitive
{
public:
	virtual void Move(const FWorldBounds& bounds, const bool& isGravity, const bool& isAngularVelocity) = 0;
	virtual FVector4 GetPosRadius() = 0;
	virtual float GetAngle() = 0;
	virtual ~UPrimitive() = default;
};