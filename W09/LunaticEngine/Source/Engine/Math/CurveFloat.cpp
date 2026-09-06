#include "CurveFloat.h"
#include <algorithm>

IMPLEMENT_CLASS(UCurveFloat, UObject)

UCurveFloat::UCurveFloat()
{
	ResetLinear();
}

float UCurveFloat::Evaluate(float NormalizedT) const
{
	if (Curve.empty())
	{
		return 0.0f;
	}

	const float T = (std::max)(0.0f, (std::min)(1.0f, NormalizedT));
	if (T <= Curve.front().X)
	{
		return Curve.front().Y;
	}
	if (T >= Curve.back().X)
	{
		return Curve.back().Y;
	}

	for (size_t Index = 1; Index < Curve.size(); ++Index)
	{
		const FVector2& Prev = Curve[Index - 1];
		const FVector2& Next = Curve[Index];
		if (T <= Next.X)
		{
			const float Range = Next.X - Prev.X;
			const float Alpha = Range > 1e-6f ? (T - Prev.X) / Range : 0.0f;
			return Prev.Y + (Next.Y - Prev.Y) * Alpha;
		}
	}

	return Curve.back().Y;
}

void UCurveFloat::AddKey(float Time, float Value)
{
	Curve.push_back(FVector2(
		(std::max)(0.0f, (std::min)(1.0f, Time)),
		Value));
	SortKeys();
}

void UCurveFloat::SortKeys()
{
	std::sort(Curve.begin(), Curve.end(), [](const FVector2& A, const FVector2& B)
		{
			return A.X < B.X;
		});
}

void UCurveFloat::ResetLinear()
{
	Curve.clear();
	Curve.push_back(FVector2(0.0f, 0.0f));
	Curve.push_back(FVector2(1.0f, 1.0f));
}
