#pragma once

#include <random>
#include <cmath>

class FRandom
{
public:
    // [0, 1)
    static float Float()
    {
        return GetFloatDist()(GetEngine());
    }

    // [Min, Max)
    static float Range(float Min, float Max)
    {
        return std::uniform_real_distribution<float>(Min, Max)(GetEngine());
    }

    // [Min, Max]
    static int Int(int Min, int Max)
    {
        return std::uniform_int_distribution<int>(Min, Max)(GetEngine());
    }

    // 0 ~ 2PI
    static float Angle()
    {
        return Range(0.0f, 2.0f * PI);
    }

    // 2D Unit Vector
    static std::pair<float, float> UnitVector2D()
    {
        float A = Angle();
        return { std::cos(A), std::sin(A) };
    }

    // 원 안 랜덤 (균일 분포)
    static std::pair<float, float> InCircle(float Radius)
    {
        float A = Angle();
        float R = std::sqrt(Float()) * Radius; // 중요: sqrt
        return { std::cos(A) * R, std::sin(A) * R };
    }

    // 링 영역 (Min ~ Max)
    static std::pair<float, float> InRing(float MinRadius, float MaxRadius)
    {
        float A = Angle();
        float R = std::sqrt(Range(MinRadius * MinRadius, MaxRadius * MaxRadius));
        return { std::cos(A) * R, std::sin(A) * R };
    }

private:
    static std::mt19937& GetEngine()
    {
        thread_local static std::mt19937 Engine{ std::random_device{}() };
        return Engine;
    }

    static std::uniform_real_distribution<float>& GetFloatDist()
    {
        thread_local static std::uniform_real_distribution<float> Dist(0.0f, 1.0f);
        return Dist;
    }

private:
    static constexpr float PI = 3.14159265358979323846f;
};