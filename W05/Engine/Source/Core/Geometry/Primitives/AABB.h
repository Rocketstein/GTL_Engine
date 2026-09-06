#pragma once

#include "Core/Math/Vector3.h"

namespace Geometry
{
    struct FAABB
    {
        FVector3 Min;
        FVector3 Max;

        constexpr FAABB() : Min(), Max() {}

        constexpr FAABB(const FVector3 &InMin, const FVector3 &InMax) : Min(InMin), Max(InMax) {}

        constexpr const FVector3 &GetMin() const { return Min; }
        constexpr const FVector3 &GetMax() const { return Max; }

        constexpr void SetMin(const FVector3 &InMin) { Min = InMin; }
        constexpr void SetMax(const FVector3 &InMax) { Max = InMax; }

        constexpr FVector3 GetCenter() const { return (Min + Max) * 0.5f; }

        constexpr FVector3 GetExtent() const { return (Max - Min) * 0.5f; }

        constexpr FVector3 GetSize() const { return Max - Min; }

        bool IsValid() const noexcept { return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z; }

        	bool Intersects(const FAABB &Other) const noexcept
        {
            return !(Max.X < Other.Min.X || Min.X > Other.Max.X || Max.Y < Other.Min.Y ||
                     Min.Y > Other.Max.Y || Max.Z < Other.Min.Z || Min.Z > Other.Max.Z);
        }

        bool Contains(const FAABB &Other) const noexcept
        {
            return Min.X <= Other.Min.X && Max.X >= Other.Max.X && Min.Y <= Other.Min.Y &&
                   Max.Y >= Other.Max.Y && Min.Z <= Other.Min.Z && Max.Z >= Other.Max.Z;
        }

        bool ContainsPoint(const FVector3 &Point) const noexcept
        {
            return Point.X >= Min.X && Point.X <= Max.X && Point.Y >= Min.Y && Point.Y <= Max.Y &&
                   Point.Z >= Min.Z && Point.Z <= Max.Z;
        }
    };
} // namespace Geometry