#pragma once

#include "Math/Vector.h"
#include "Engine/Core/CoreTypes.h"

class UCollider2DComponent;

enum class ECollision2DShapeType
{
    Box,
    Circle
};

struct FCollision2DShapeGeometry
{
    ECollision2DShapeType Type = ECollision2DShapeType::Box;
    FVector2 Center = FVector2(0.0f, 0.0f);
    FVector2 AxisX = FVector2(1.0f, 0.0f);
    FVector2 AxisY = FVector2(0.0f, 1.0f);
    FVector2 BoxExtent = FVector2(0.0f, 0.0f);
    float Radius = 0.0f;
};

struct FCollision2DContact
{
    FVector2 Normal = FVector2(1.0f, 0.0f);
    float PenetrationDepth = 0.0f;
};

struct FCollision2DBounds
{
    FVector2 Min;
    FVector2 Max;
};

struct FCollision2DPair
{
    UCollider2DComponent* A = nullptr;
    UCollider2DComponent* B = nullptr;
};

struct FCollision2DCell
{
    int32 X = 0;
    int32 Y = 0;
};

using FCollision2DCellKey = uint64;

inline FCollision2DCellKey MakeCollision2DCellKey(int32 X, int32 Y)
{
    return (static_cast<uint64>(static_cast<uint32>(X)) << 32) | static_cast<uint32>(Y);
}