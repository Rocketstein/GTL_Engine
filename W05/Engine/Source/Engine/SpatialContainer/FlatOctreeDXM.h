#pragma once

#include "FlatOctreeBase.h"

class FFlatOctreeDXM : public FFlatOctreeBase
{
public:
	using FFlatOctreeBase::FFlatOctreeBase;
	virtual ~FFlatOctreeDXM();

protected:
	virtual int32 GetBatchLaneCount() const override { return 4; }
	virtual FFrustumBatchClassification ClassifyFrustumBatch(const FFrustum& InFrustum, const int32* InObjectSlots, int32 InCount) const override;
	virtual uint32 RaycastBatchMask(const FRay& InNormalizedRay, const int32* InObjectSlots, int32 InCount, float* OutEntryDistances) const override;
};
