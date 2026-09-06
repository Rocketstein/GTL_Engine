#include "FlatOctreeDXM.h"

#include <DirectXMath.h>
#include <cmath>
#include <limits>

FFlatOctreeDXM::~FFlatOctreeDXM() = default;

FFlatOctreeBase::FFrustumBatchClassification FFlatOctreeDXM::ClassifyFrustumBatch(const FFrustum& InFrustum, const int32* InObjectSlots, int32 InCount) const
{
	float MinX[MaxBatchLaneCount], MinY[MaxBatchLaneCount], MinZ[MaxBatchLaneCount];
	float MaxX[MaxBatchLaneCount], MaxY[MaxBatchLaneCount], MaxZ[MaxBatchLaneCount];
	GatherObjectBounds(InObjectSlots, InCount, MinX, MinY, MinZ, MaxX, MaxY, MaxZ);

	const DirectX::XMVECTOR MinXVec = DirectX::XMVectorSet(MinX[0], MinX[1], MinX[2], MinX[3]);
	const DirectX::XMVECTOR MinYVec = DirectX::XMVectorSet(MinY[0], MinY[1], MinY[2], MinY[3]);
	const DirectX::XMVECTOR MinZVec = DirectX::XMVectorSet(MinZ[0], MinZ[1], MinZ[2], MinZ[3]);
	const DirectX::XMVECTOR MaxXVec = DirectX::XMVectorSet(MaxX[0], MaxX[1], MaxX[2], MaxX[3]);
	const DirectX::XMVECTOR MaxYVec = DirectX::XMVectorSet(MaxY[0], MaxY[1], MaxY[2], MaxY[3]);
	const DirectX::XMVECTOR MaxZVec = DirectX::XMVectorSet(MaxZ[0], MaxZ[1], MaxZ[2], MaxZ[3]);

	const DirectX::XMVECTOR Half = DirectX::XMVectorReplicate(0.5f);
	const DirectX::XMVECTOR CenterXVec = DirectX::XMVectorMultiply(DirectX::XMVectorAdd(MinXVec, MaxXVec), Half);
	const DirectX::XMVECTOR CenterYVec = DirectX::XMVectorMultiply(DirectX::XMVectorAdd(MinYVec, MaxYVec), Half);
	const DirectX::XMVECTOR CenterZVec = DirectX::XMVectorMultiply(DirectX::XMVectorAdd(MinZVec, MaxZVec), Half);
	const DirectX::XMVECTOR ExtentXVec = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(MaxXVec, MinXVec), Half);
	const DirectX::XMVECTOR ExtentYVec = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(MaxYVec, MinYVec), Half);
	const DirectX::XMVECTOR ExtentZVec = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(MaxZVec, MinZVec), Half);

	const FPlane4* Planes = InFrustum.GetPlanes();
	FFrustumBatchClassification Classification;
	Classification.VisibleMask = InCount >= 4 ? 0xFu : ((1u << InCount) - 1u);
	Classification.InsideMask = Classification.VisibleMask;

	for (int32 PlaneIndex = 0; PlaneIndex < FFrustum::PlaneCount; ++PlaneIndex)
	{
		const DirectX::XMVECTOR A = DirectX::XMVectorReplicate(Planes[PlaneIndex].A);
		const DirectX::XMVECTOR B = DirectX::XMVectorReplicate(Planes[PlaneIndex].B);
		const DirectX::XMVECTOR C = DirectX::XMVectorReplicate(Planes[PlaneIndex].C);
		const DirectX::XMVECTOR D = DirectX::XMVectorReplicate(Planes[PlaneIndex].D);

		const DirectX::XMVECTOR Distance =
			DirectX::XMVectorAdd(
				DirectX::XMVectorAdd(DirectX::XMVectorMultiply(A, CenterXVec), DirectX::XMVectorMultiply(B, CenterYVec)),
				DirectX::XMVectorAdd(DirectX::XMVectorMultiply(C, CenterZVec), D));

		const DirectX::XMVECTOR Radius =
			DirectX::XMVectorAdd(
				DirectX::XMVectorAdd(DirectX::XMVectorMultiply(DirectX::XMVectorAbs(A), ExtentXVec), DirectX::XMVectorMultiply(DirectX::XMVectorAbs(B), ExtentYVec)),
				DirectX::XMVectorMultiply(DirectX::XMVectorAbs(C), ExtentZVec));

		DirectX::XMFLOAT4 DistancePlusRadius;
		DirectX::XMFLOAT4 DistanceMinusRadius;
		DirectX::XMStoreFloat4(&DistancePlusRadius, DirectX::XMVectorAdd(Distance, Radius));
		DirectX::XMStoreFloat4(&DistanceMinusRadius, DirectX::XMVectorSubtract(Distance, Radius));

		const float PlaneVisibleValues[4] = { DistancePlusRadius.x, DistancePlusRadius.y, DistancePlusRadius.z, DistancePlusRadius.w };
		const float PlaneInsideValues[4] = { DistanceMinusRadius.x, DistanceMinusRadius.y, DistanceMinusRadius.z, DistanceMinusRadius.w };
		for (int32 Lane = 0; Lane < InCount; ++Lane)
		{
			if (PlaneVisibleValues[Lane] < 0.0f)
			{
				Classification.VisibleMask &= ~(1u << Lane);
			}

			if (PlaneInsideValues[Lane] <= 0.0f)
			{
				Classification.InsideMask &= ~(1u << Lane);
			}
		}

		if (Classification.VisibleMask == 0)
		{
			break;
		}
	}

	Classification.InsideMask &= Classification.VisibleMask;
	return Classification;
}

uint32 FFlatOctreeDXM::RaycastBatchMask(const FRay& InNormalizedRay, const int32* InObjectSlots, int32 InCount, float* OutEntryDistances) const
{
	float MinX[MaxBatchLaneCount], MinY[MaxBatchLaneCount], MinZ[MaxBatchLaneCount];
	float MaxX[MaxBatchLaneCount], MaxY[MaxBatchLaneCount], MaxZ[MaxBatchLaneCount];
	GatherObjectBounds(InObjectSlots, InCount, MinX, MinY, MinZ, MaxX, MaxY, MaxZ);

	const DirectX::XMVECTOR MinXVec = DirectX::XMVectorSet(MinX[0], MinX[1], MinX[2], MinX[3]);
	const DirectX::XMVECTOR MinYVec = DirectX::XMVectorSet(MinY[0], MinY[1], MinY[2], MinY[3]);
	const DirectX::XMVECTOR MinZVec = DirectX::XMVectorSet(MinZ[0], MinZ[1], MinZ[2], MinZ[3]);
	const DirectX::XMVECTOR MaxXVec = DirectX::XMVectorSet(MaxX[0], MaxX[1], MaxX[2], MaxX[3]);
	const DirectX::XMVECTOR MaxYVec = DirectX::XMVectorSet(MaxY[0], MaxY[1], MaxY[2], MaxY[3]);
	const DirectX::XMVECTOR MaxZVec = DirectX::XMVectorSet(MaxZ[0], MaxZ[1], MaxZ[2], MaxZ[3]);

	DirectX::XMVECTOR TMin = DirectX::XMVectorZero();
	DirectX::XMVECTOR TMax = DirectX::XMVectorReplicate((std::numeric_limits<float>::max)());
	uint32 HitMask = InCount >= 4 ? 0xFu : ((1u << InCount) - 1u);

	auto ApplyAxis = [&](float OriginValue, float DirectionValue, DirectX::XMVECTOR MinVec, DirectX::XMVECTOR MaxVec)
	{
		if (std::abs(DirectionValue) <= FMath::SmallNumber)
		{
			for (int32 Lane = 0; Lane < InCount; ++Lane)
			{
				const float MinValue = DirectX::XMVectorGetByIndex(MinVec, Lane);
				const float MaxValue = DirectX::XMVectorGetByIndex(MaxVec, Lane);
				if (OriginValue < MinValue || OriginValue > MaxValue)
				{
					HitMask &= ~(1u << Lane);
				}
			}

			return;
		}

		const DirectX::XMVECTOR Origin = DirectX::XMVectorReplicate(OriginValue);
		const DirectX::XMVECTOR InvDirection = DirectX::XMVectorReplicate(1.0f / DirectionValue);
		const DirectX::XMVECTOR Entry = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(MinVec, Origin), InvDirection);
		const DirectX::XMVECTOR Exit = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(MaxVec, Origin), InvDirection);
		TMin = DirectX::XMVectorMax(TMin, DirectX::XMVectorMin(Entry, Exit));
		TMax = DirectX::XMVectorMin(TMax, DirectX::XMVectorMax(Entry, Exit));
	};

	ApplyAxis(InNormalizedRay.Origin.X, InNormalizedRay.Direction.X, MinXVec, MaxXVec);
	ApplyAxis(InNormalizedRay.Origin.Y, InNormalizedRay.Direction.Y, MinYVec, MaxYVec);
	ApplyAxis(InNormalizedRay.Origin.Z, InNormalizedRay.Direction.Z, MinZVec, MaxZVec);

	DirectX::XMFLOAT4 EntryDistances;
	DirectX::XMFLOAT4 ExitDistances;
	DirectX::XMStoreFloat4(&EntryDistances, TMin);
	DirectX::XMStoreFloat4(&ExitDistances, TMax);

	const float EntryValues[4] = { EntryDistances.x, EntryDistances.y, EntryDistances.z, EntryDistances.w };
	const float ExitValues[4] = { ExitDistances.x, ExitDistances.y, ExitDistances.z, ExitDistances.w };

	for (int32 Lane = 0; Lane < 4; ++Lane)
	{
		OutEntryDistances[Lane] = 0.0f;
	}

	for (int32 Lane = 0; Lane < InCount; ++Lane)
	{
		if ((HitMask & (1u << Lane)) == 0)
		{
			continue;
		}

		if (EntryValues[Lane] > ExitValues[Lane] || ExitValues[Lane] < 0.0f)
		{
			HitMask &= ~(1u << Lane);
			continue;
		}

		OutEntryDistances[Lane] = EntryValues[Lane] < 0.0f ? 0.0f : EntryValues[Lane];
	}

	return HitMask;
}
