#include "FlatOctreeAVX2.h"

#include <immintrin.h>
#include <cmath>
#include <limits>

FFlatOctreeAVX2::~FFlatOctreeAVX2() = default;

FFlatOctreeBase::FFrustumBatchClassification FFlatOctreeAVX2::ClassifyFrustumBatch(const FFrustum& InFrustum, const int32* InObjectSlots, int32 InCount) const
{
	float MinX[MaxBatchLaneCount], MinY[MaxBatchLaneCount], MinZ[MaxBatchLaneCount];
	float MaxX[MaxBatchLaneCount], MaxY[MaxBatchLaneCount], MaxZ[MaxBatchLaneCount];
	GatherObjectBounds(InObjectSlots, InCount, MinX, MinY, MinZ, MaxX, MaxY, MaxZ);

	const __m256 MinXVec = _mm256_loadu_ps(MinX);
	const __m256 MinYVec = _mm256_loadu_ps(MinY);
	const __m256 MinZVec = _mm256_loadu_ps(MinZ);
	const __m256 MaxXVec = _mm256_loadu_ps(MaxX);
	const __m256 MaxYVec = _mm256_loadu_ps(MaxY);
	const __m256 MaxZVec = _mm256_loadu_ps(MaxZ);

	const __m256 Half = _mm256_set1_ps(0.5f);
	const __m256 CenterXVec = _mm256_mul_ps(_mm256_add_ps(MinXVec, MaxXVec), Half);
	const __m256 CenterYVec = _mm256_mul_ps(_mm256_add_ps(MinYVec, MaxYVec), Half);
	const __m256 CenterZVec = _mm256_mul_ps(_mm256_add_ps(MinZVec, MaxZVec), Half);
	const __m256 ExtentXVec = _mm256_mul_ps(_mm256_sub_ps(MaxXVec, MinXVec), Half);
	const __m256 ExtentYVec = _mm256_mul_ps(_mm256_sub_ps(MaxYVec, MinYVec), Half);
	const __m256 ExtentZVec = _mm256_mul_ps(_mm256_sub_ps(MaxZVec, MinZVec), Half);

	const __m256 Zero = _mm256_setzero_ps();
	const __m256 AbsMask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff));
	const FPlane4* Planes = InFrustum.GetPlanes();
	FFrustumBatchClassification Classification;
	Classification.VisibleMask = InCount >= 8 ? 0xFFu : ((1u << InCount) - 1u);
	Classification.InsideMask = Classification.VisibleMask;

	for (int32 PlaneIndex = 0; PlaneIndex < FFrustum::PlaneCount; ++PlaneIndex)
	{
		const FPlane4& Plane = Planes[PlaneIndex];
		const __m256 A = _mm256_set1_ps(Plane.A);
		const __m256 B = _mm256_set1_ps(Plane.B);
		const __m256 C = _mm256_set1_ps(Plane.C);
		const __m256 D = _mm256_set1_ps(Plane.D);
		const __m256 AbsA = _mm256_and_ps(A, AbsMask);
		const __m256 AbsB = _mm256_and_ps(B, AbsMask);
		const __m256 AbsC = _mm256_and_ps(C, AbsMask);

		__m256 Distance = _mm256_add_ps(_mm256_mul_ps(A, CenterXVec), _mm256_mul_ps(B, CenterYVec));
		Distance = _mm256_add_ps(Distance, _mm256_mul_ps(C, CenterZVec));
		Distance = _mm256_add_ps(Distance, D);

		__m256 Radius = _mm256_add_ps(_mm256_mul_ps(AbsA, ExtentXVec), _mm256_mul_ps(AbsB, ExtentYVec));
		Radius = _mm256_add_ps(Radius, _mm256_mul_ps(AbsC, ExtentZVec));

		const int32 OutsideMask = _mm256_movemask_ps(_mm256_cmp_ps(_mm256_add_ps(Distance, Radius), Zero, _CMP_LT_OQ));
		const int32 InsidePlaneMask = _mm256_movemask_ps(_mm256_cmp_ps(_mm256_sub_ps(Distance, Radius), Zero, _CMP_GT_OQ));
		Classification.VisibleMask &= ~static_cast<uint32>(OutsideMask);
		Classification.InsideMask &= static_cast<uint32>(InsidePlaneMask);
		if (Classification.VisibleMask == 0)
		{
			break;
		}
	}

	Classification.InsideMask &= Classification.VisibleMask;
	return Classification;
}

uint32 FFlatOctreeAVX2::RaycastBatchMask(const FRay& InNormalizedRay, const int32* InObjectSlots, int32 InCount, float* OutEntryDistances) const
{
	float MinX[MaxBatchLaneCount], MinY[MaxBatchLaneCount], MinZ[MaxBatchLaneCount];
	float MaxX[MaxBatchLaneCount], MaxY[MaxBatchLaneCount], MaxZ[MaxBatchLaneCount];
	GatherObjectBounds(InObjectSlots, InCount, MinX, MinY, MinZ, MaxX, MaxY, MaxZ);

	const __m256 MinXVec = _mm256_loadu_ps(MinX);
	const __m256 MinYVec = _mm256_loadu_ps(MinY);
	const __m256 MinZVec = _mm256_loadu_ps(MinZ);
	const __m256 MaxXVec = _mm256_loadu_ps(MaxX);
	const __m256 MaxYVec = _mm256_loadu_ps(MaxY);
	const __m256 MaxZVec = _mm256_loadu_ps(MaxZ);

	const __m256 Zero = _mm256_setzero_ps();
	__m256 TMin = Zero;
	__m256 TMax = _mm256_set1_ps((std::numeric_limits<float>::max)());
	uint32 HitMask = InCount >= 8 ? 0xFFu : ((1u << InCount) - 1u);

	auto ApplyAxis = [&](float OriginValue, float DirectionValue, const float* InMin, const float* InMax, __m256 MinVec, __m256 MaxVec)
	{
		if (std::abs(DirectionValue) <= FMath::SmallNumber)
		{
			for (int32 Lane = 0; Lane < InCount; ++Lane)
			{
				if (OriginValue < InMin[Lane] || OriginValue > InMax[Lane])
				{
					HitMask &= ~(1u << Lane);
				}
			}

			return;
		}

		const __m256 OriginVec = _mm256_set1_ps(OriginValue);
		const __m256 InvDirectionVec = _mm256_set1_ps(1.0f / DirectionValue);
		const __m256 Entry = _mm256_mul_ps(_mm256_sub_ps(MinVec, OriginVec), InvDirectionVec);
		const __m256 Exit = _mm256_mul_ps(_mm256_sub_ps(MaxVec, OriginVec), InvDirectionVec);
		const __m256 AxisMin = _mm256_min_ps(Entry, Exit);
		const __m256 AxisMax = _mm256_max_ps(Entry, Exit);
		TMin = _mm256_max_ps(TMin, AxisMin);
		TMax = _mm256_min_ps(TMax, AxisMax);
	};

	ApplyAxis(InNormalizedRay.Origin.X, InNormalizedRay.Direction.X, MinX, MaxX, MinXVec, MaxXVec);
	ApplyAxis(InNormalizedRay.Origin.Y, InNormalizedRay.Direction.Y, MinY, MaxY, MinYVec, MaxYVec);
	ApplyAxis(InNormalizedRay.Origin.Z, InNormalizedRay.Direction.Z, MinZ, MaxZ, MinZVec, MaxZVec);

	const __m256 RangeValid = _mm256_cmp_ps(TMax, TMin, _CMP_GE_OQ);
	const __m256 ExitValid = _mm256_cmp_ps(TMax, Zero, _CMP_GE_OQ);
	const int32 ValidMask = _mm256_movemask_ps(_mm256_and_ps(RangeValid, ExitValid));
	HitMask &= static_cast<uint32>(ValidMask);

	float EntryValues[MaxBatchLaneCount] = {};
	_mm256_storeu_ps(EntryValues, TMin);

	for (int32 Lane = 0; Lane < MaxBatchLaneCount; ++Lane)
	{
		OutEntryDistances[Lane] = 0.0f;
	}

	for (int32 Lane = 0; Lane < InCount; ++Lane)
	{
		if ((HitMask & (1u << Lane)) != 0)
		{
			OutEntryDistances[Lane] = EntryValues[Lane] < 0.0f ? 0.0f : EntryValues[Lane];
		}
	}

	return HitMask;
}
