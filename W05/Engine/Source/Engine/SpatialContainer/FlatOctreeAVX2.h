#pragma once

#include "FlatOctreeBase.h"

/*
	AVX2 variant notes:
	- Uses 8-lane batch tests for object AABB vs frustum / ray traversal.
	- Build is enabled only for this translation unit via /arch:AVX2 in Engine.vcxproj.
	- Callers should use this class only on CPUs that support AVX2.
*/
class FFlatOctreeAVX2 : public FFlatOctreeBase
{
public:
	using FFlatOctreeBase::FFlatOctreeBase;
	virtual ~FFlatOctreeAVX2();

protected:
	virtual int32 GetBatchLaneCount() const override { return 8; }
	virtual FFrustumBatchClassification ClassifyFrustumBatch(const FFrustum& InFrustum, const int32* InObjectSlots, int32 InCount) const override;
	virtual uint32 RaycastBatchMask(const FRay& InNormalizedRay, const int32* InObjectSlots, int32 InCount, float* OutEntryDistances) const override;
};
