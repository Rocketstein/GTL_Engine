// 수학 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

#include <cmath>
#include <cstdlib>

namespace FMath
{
constexpr float Pi = 3.14159265358979323846f;
constexpr float DegToRad = Pi / 180.0f;
constexpr float RadToDeg = 180.0f / Pi;
constexpr float Epsilon = 1e-4f;

static constexpr int32 Permutation[512] = {
	63, 9, 212, 205, 31, 128, 72, 59, 137, 203, 195, 170, 181, 115, 165, 40, 116, 139, 175, 225, 132, 99, 222, 2, 41, 15, 197, 93, 169, 90, 228, 43, 221, 38, 206, 204, 73, 17, 97, 10, 96, 47, 32, 138, 136, 30, 219,
	78, 224, 13, 193, 88, 134, 211, 7, 112, 176, 19, 106, 83, 75, 217, 85, 0, 98, 140, 229, 80, 118, 151, 117, 251, 103, 242, 81, 238, 172, 82, 110, 4, 227, 77, 243, 46, 12, 189, 34, 188, 200, 161, 68, 76, 171, 194,
	57, 48, 247, 233, 51, 105, 5, 23, 42, 50, 216, 45, 239, 148, 249, 84, 70, 125, 108, 241, 62, 66, 64, 240, 173, 185, 250, 49, 6, 37, 26, 21, 244, 60, 223, 255, 16, 145, 27, 109, 58, 102, 142, 253, 120, 149, 160,
	124, 156, 79, 186, 135, 127, 14, 121, 22, 65, 54, 153, 91, 213, 174, 24, 252, 131, 192, 190, 202, 208, 35, 94, 231, 56, 95, 183, 163, 111, 147, 25, 67, 36, 92, 236, 71, 166, 1, 187, 100, 130, 143, 237, 178, 158,
	104, 184, 159, 177, 52, 214, 230, 119, 87, 114, 201, 179, 198, 3, 248, 182, 39, 11, 152, 196, 113, 20, 232, 69, 141, 207, 234, 53, 86, 180, 226, 74, 150, 218, 29, 133, 8, 44, 123, 28, 146, 89, 101, 154, 220, 126,
	155, 122, 210, 168, 254, 162, 129, 33, 18, 209, 61, 191, 199, 157, 245, 55, 164, 167, 215, 246, 144, 107, 235,

	63, 9, 212, 205, 31, 128, 72, 59, 137, 203, 195, 170, 181, 115, 165, 40, 116, 139, 175, 225, 132, 99, 222, 2, 41, 15, 197, 93, 169, 90, 228, 43, 221, 38, 206, 204, 73, 17, 97, 10, 96, 47, 32, 138, 136, 30, 219,
	78, 224, 13, 193, 88, 134, 211, 7, 112, 176, 19, 106, 83, 75, 217, 85, 0, 98, 140, 229, 80, 118, 151, 117, 251, 103, 242, 81, 238, 172, 82, 110, 4, 227, 77, 243, 46, 12, 189, 34, 188, 200, 161, 68, 76, 171, 194,
	57, 48, 247, 233, 51, 105, 5, 23, 42, 50, 216, 45, 239, 148, 249, 84, 70, 125, 108, 241, 62, 66, 64, 240, 173, 185, 250, 49, 6, 37, 26, 21, 244, 60, 223, 255, 16, 145, 27, 109, 58, 102, 142, 253, 120, 149, 160,
	124, 156, 79, 186, 135, 127, 14, 121, 22, 65, 54, 153, 91, 213, 174, 24, 252, 131, 192, 190, 202, 208, 35, 94, 231, 56, 95, 183, 163, 111, 147, 25, 67, 36, 92, 236, 71, 166, 1, 187, 100, 130, 143, 237, 178, 158,
	104, 184, 159, 177, 52, 214, 230, 119, 87, 114, 201, 179, 198, 3, 248, 182, 39, 11, 152, 196, 113, 20, 232, 69, 141, 207, 234, 53, 86, 180, 226, 74, 150, 218, 29, 133, 8, 44, 123, 28, 146, 89, 101, 154, 220, 126,
	155, 122, 210, 168, 254, 162, 129, 33, 18, 209, 61, 191, 199, 157, 245, 55, 164, 167, 215, 246, 144, 107, 235
};

inline float Grad1(int Hash, float X)
{
	// Slicing Perlin's 3D improved noise would give us only scales of -1, 0 and 1; this looks pretty bad so let's use a different sampling
	static constexpr float Grad1Scales[16] = { -8 / 8, -7 / 8., -6 / 8., -5 / 8., -4 / 8., -3 / 8., -2 / 8., -1 / 8., 1 / 8., 2 / 8., 3 / 8., 4 / 8., 5 / 8., 6 / 8., 7 / 8., 8 / 8 };
	return Grad1Scales[Hash & 15] * X;
}

inline float Clamp(float Val, float Lo, float Hi)
{
    if (Val >= Hi)
        return Hi;
    if (Val <= Lo)
        return Lo;
    return Val;
}

inline float Lerp(float A, float B, float T)
{
    return A + (B-A) * T;
}

inline FVector Lerp(FVector A, FVector B, float T)
{
    return A + (B-A) * T;
}

[[nodiscard]] static inline float FRand()
{
	constexpr int RandMax = 0x00ffffff < RAND_MAX ? 0x00ffffff : RAND_MAX;
	return (std::rand() & RandMax) / static_cast<float>(RandMax);
}

inline float SmoothCurve(float X)
{
	return X * X * X * (X * (X * 6.0f - 15.0f) + 10.0f);
}

inline float PerlinNoise1D(float X)
{
	const float Xf1 = std::floor(X);
	const int Xi = static_cast<int>(Xf1) & 255;
	X -= Xf1;
	const float Xm1 = X - 1.0f;

	const int A = Permutation[Xi];
	const int B = Permutation[Xi + 1];

	const float U = SmoothCurve(X);

	return 2.0f * Lerp(Grad1(A, X), Grad1(B, Xm1), U);
}

inline int RandHelper(int A)
{
	if (A <= 0)
	{
		return 0;
	}

	const int Value = static_cast<int>(std::trunc(FRand() * static_cast<float>(A)));
	return Value >= A ? A - 1 : Value;
}
} // namespace FMath

// 기존 매크로 호환 — 이행 완료 후 제거
#define M_PI FMath::Pi
#define DEG_TO_RAD FMath::DegToRad
#define RAD_TO_DEG FMath::RadToDeg
#define EPSILON FMath::Epsilon

// 기존 전역 Clamp 호환
inline float Clamp(float val, float lo, float hi)
{
    return FMath::Clamp(val, lo, hi);
}
