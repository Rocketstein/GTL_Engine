#pragma once

#include <cmath>

struct FMath
{
    static constexpr float Epsilon{1e-6f};

    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float InvPI = 0.31830988618379067154f;
    static constexpr float HalfPI = 1.57079632679489661923f;
    static constexpr float TwoPi = 6.28318530717958647692f;

    static constexpr float SmallNumber = 1.e-8f;
    static constexpr float KindaSmallNumber = 1.e-4f;

    static constexpr float DegreesToRadians(float Degrees) { return Degrees * (PI / 180.0f); }

    static constexpr float RadiansToDegrees(float Radians) { return Radians * (180.0f / PI); }

    static constexpr float Abs(float Value) { return (Value < 0.0f) ? -Value : Value; }

    static constexpr bool IsNearlyZero(float Value, float Tolerance = Epsilon)
    {
        return Abs(Value) <= Tolerance;
    }

    template <typename T> static inline T Clamp(const T Value, const T Min, const T Max)
    {
        return (Value < Min) ? Min : (Value > Max) ? Max : Value;
    }

    static float LerpAngle(float A, float B, float Alpha)
    {
        float Delta = std::fmod(B - A + 540.0f, 360.0f) - 180.0f;
        return A + Delta * Alpha;
    }

    static constexpr float Max(float A, float B) { return A > B ? A : B; }
    static constexpr float Min(float A, float B) { return A < B ? A : B; }
    static constexpr int   Max(int A, int B) { return A > B ? A : B; }
    static constexpr int   Min(int A, int B) { return A < B ? A : B; }
};
