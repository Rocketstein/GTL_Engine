#include "FlatOctreeIntrinsics.h"
#include "Core/CoreMinimal.h"
#include <immintrin.h>
#include <cmath>
#include <limits>

FFlatOctreeIntrinsics::~FFlatOctreeIntrinsics() = default;

FFlatOctreeBase::FFrustumBatchClassification FFlatOctreeIntrinsics::ClassifyFrustumBatch(const FFrustum& InFrustum, const int32* InObjectSlots, int32 InCount) const
{
	float MinX[MaxBatchLaneCount], MinY[MaxBatchLaneCount], MinZ[MaxBatchLaneCount];
	float MaxX[MaxBatchLaneCount], MaxY[MaxBatchLaneCount], MaxZ[MaxBatchLaneCount];
	GatherObjectBounds(InObjectSlots, InCount, MinX, MinY, MinZ, MaxX, MaxY, MaxZ);

	const __m128 MinXVec = _mm_setr_ps(MinX[0], MinX[1], MinX[2], MinX[3]);
	const __m128 MinYVec = _mm_setr_ps(MinY[0], MinY[1], MinY[2], MinY[3]);
	const __m128 MinZVec = _mm_setr_ps(MinZ[0], MinZ[1], MinZ[2], MinZ[3]);
	const __m128 MaxXVec = _mm_setr_ps(MaxX[0], MaxX[1], MaxX[2], MaxX[3]);
	const __m128 MaxYVec = _mm_setr_ps(MaxY[0], MaxY[1], MaxY[2], MaxY[3]);
	const __m128 MaxZVec = _mm_setr_ps(MaxZ[0], MaxZ[1], MaxZ[2], MaxZ[3]);

	const __m128 Half = _mm_set1_ps(0.5f);
	const __m128 CenterXVec = _mm_mul_ps(_mm_add_ps(MinXVec, MaxXVec), Half);
	const __m128 CenterYVec = _mm_mul_ps(_mm_add_ps(MinYVec, MaxYVec), Half);
	const __m128 CenterZVec = _mm_mul_ps(_mm_add_ps(MinZVec, MaxZVec), Half);
	const __m128 ExtentXVec = _mm_mul_ps(_mm_sub_ps(MaxXVec, MinXVec), Half);
	const __m128 ExtentYVec = _mm_mul_ps(_mm_sub_ps(MaxYVec, MinYVec), Half);
	const __m128 ExtentZVec = _mm_mul_ps(_mm_sub_ps(MaxZVec, MinZVec), Half);

	const __m128 Zero = _mm_setzero_ps();
	const __m128 AbsMask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
	const FPlane4* Planes = InFrustum.GetPlanes();
	FFrustumBatchClassification Classification;
	Classification.VisibleMask = InCount >= 4 ? 0xFu : ((1u << InCount) - 1u);
	Classification.InsideMask = Classification.VisibleMask;

	for (int32 PlaneIndex = 0; PlaneIndex < FFrustum::PlaneCount; ++PlaneIndex)
	{
		const FPlane4& Plane = Planes[PlaneIndex];
		const __m128 A = _mm_set1_ps(Plane.A);
		const __m128 B = _mm_set1_ps(Plane.B);
		const __m128 C = _mm_set1_ps(Plane.C);
		const __m128 D = _mm_set1_ps(Plane.D);
		const __m128 AbsA = _mm_and_ps(A, AbsMask);
		const __m128 AbsB = _mm_and_ps(B, AbsMask);
		const __m128 AbsC = _mm_and_ps(C, AbsMask);

		__m128 Distance = _mm_add_ps(_mm_mul_ps(A, CenterXVec), _mm_mul_ps(B, CenterYVec));
		Distance = _mm_add_ps(Distance, _mm_mul_ps(C, CenterZVec));
		Distance = _mm_add_ps(Distance, D);

		__m128 Radius = _mm_add_ps(_mm_mul_ps(AbsA, ExtentXVec), _mm_mul_ps(AbsB, ExtentYVec));
		Radius = _mm_add_ps(Radius, _mm_mul_ps(AbsC, ExtentZVec));

		const int32 OutsideMask = _mm_movemask_ps(_mm_cmplt_ps(_mm_add_ps(Distance, Radius), Zero));
		const int32 InsidePlaneMask = _mm_movemask_ps(_mm_cmpgt_ps(_mm_sub_ps(Distance, Radius), Zero));
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

uint32 FFlatOctreeIntrinsics::RaycastBatchMask(const FRay& InNormalizedRay, const int32* InObjectSlots, int32 InCount, float* OutEntryDistances) const
{
	float MinX[MaxBatchLaneCount], MinY[MaxBatchLaneCount], MinZ[MaxBatchLaneCount];
	float MaxX[MaxBatchLaneCount], MaxY[MaxBatchLaneCount], MaxZ[MaxBatchLaneCount];
	GatherObjectBounds(InObjectSlots, InCount, MinX, MinY, MinZ, MaxX, MaxY, MaxZ);

	const __m128 MinXVec = _mm_setr_ps(MinX[0], MinX[1], MinX[2], MinX[3]);
	const __m128 MinYVec = _mm_setr_ps(MinY[0], MinY[1], MinY[2], MinY[3]);
	const __m128 MinZVec = _mm_setr_ps(MinZ[0], MinZ[1], MinZ[2], MinZ[3]);
	const __m128 MaxXVec = _mm_setr_ps(MaxX[0], MaxX[1], MaxX[2], MaxX[3]);
	const __m128 MaxYVec = _mm_setr_ps(MaxY[0], MaxY[1], MaxY[2], MaxY[3]);
	const __m128 MaxZVec = _mm_setr_ps(MaxZ[0], MaxZ[1], MaxZ[2], MaxZ[3]);

	const __m128 Zero = _mm_setzero_ps();
	__m128 TMin = Zero;
	__m128 TMax = _mm_set1_ps((std::numeric_limits<float>::max)());
	uint32 HitMask = InCount >= 4 ? 0xFu : ((1u << InCount) - 1u);

	auto ApplyAxis = [&](float OriginValue, float DirectionValue, const float InMin[4], const float InMax[4], __m128 MinVec, __m128 MaxVec)
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

		const __m128 OriginVec = _mm_set1_ps(OriginValue);
		const __m128 InvDirectionVec = _mm_set1_ps(1.0f / DirectionValue);
		const __m128 Entry = _mm_mul_ps(_mm_sub_ps(MinVec, OriginVec), InvDirectionVec);
		const __m128 Exit = _mm_mul_ps(_mm_sub_ps(MaxVec, OriginVec), InvDirectionVec);
		const __m128 AxisMin = _mm_min_ps(Entry, Exit);
		const __m128 AxisMax = _mm_max_ps(Entry, Exit);
		TMin = _mm_max_ps(TMin, AxisMin);
		TMax = _mm_min_ps(TMax, AxisMax);
	};

	ApplyAxis(InNormalizedRay.Origin.X, InNormalizedRay.Direction.X, MinX, MaxX, MinXVec, MaxXVec);
	ApplyAxis(InNormalizedRay.Origin.Y, InNormalizedRay.Direction.Y, MinY, MaxY, MinYVec, MaxYVec);
	ApplyAxis(InNormalizedRay.Origin.Z, InNormalizedRay.Direction.Z, MinZ, MaxZ, MinZVec, MaxZVec);

	const __m128 RangeValid = _mm_cmpge_ps(TMax, TMin);
	const __m128 ExitValid = _mm_cmpge_ps(TMax, Zero);
	const int32 ValidMask = _mm_movemask_ps(_mm_and_ps(RangeValid, ExitValid));
	HitMask &= static_cast<uint32>(ValidMask);

	float EntryValues[4] = {};
	_mm_storeu_ps(EntryValues, TMin);

	for (int32 Lane = 0; Lane < 4; ++Lane)
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
