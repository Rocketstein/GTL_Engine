#pragma once

#include "Object/Object.h"
#include "Animation/Sequence/BoneAnimationTrack.h"
#include "Animation/Notify/AnimNotifyEvent.h"

class FReferenceCollector;

// AnimSequence 의 직렬화 가능한 "원본" 데이터 모델.
// 압축/디시메이션은 추후 옵션. 현재는 raw 키프레임 + notify 만 보관.

#include "Source/Engine/Animation/Sequence/AnimDataModel.generated.h"

UCLASS()
class UAnimDataModel : public UObject
{
public:
	GENERATED_BODY()
    static constexpr int32 MaxNotifyTracks = 32;

    UAnimDataModel()           = default;
    ~UAnimDataModel() override = default;

    void Serialize(FArchive& Ar) override;
    void Serialize(FArchive& Ar, uint32 PackageVersion);
    void AddReferencedObjects(FReferenceCollector& Collector) override;
    // 수동 바이너리 포맷 — 반사 직렬화 비활성.
    bool ShouldReflectProperties() const override { return false; }

    float PlayLength = 0.0f;  // sec
    float FrameRate  = 30.0f; // fps
    int32 NumFrames  = 0;

    TArray<FBoneAnimationTrack> BoneAnimationTracks;
    TArray<FMorphTargetCurve>   MorphTargetCurves;
    TArray<FAnimNotifyEvent>    Notifies;
    int32                       NotifyTrackCount = 1;

    int32 GetNotifyTrackCount() const
    {
        return NotifyTrackCount > 0 ? NotifyTrackCount : 1;
    }

    int32 AddNotifyTrack()
    {
        if (NotifyTrackCount < 1)
        {
            NotifyTrackCount = 1;
        }
        if (NotifyTrackCount >= MaxNotifyTracks)
        {
            return -1;
        }
        return NotifyTrackCount++;
    }

    const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const
    {
        return BoneAnimationTracks;
    }

    TArray<FBoneAnimationTrack>& GetMutableBoneAnimationTracks()
    {
        return BoneAnimationTracks;
    }

    const TArray<FMorphTargetCurve>& GetMorphTargetCurves() const
    {
        return MorphTargetCurves;
    }

    TArray<FMorphTargetCurve>& GetMutableMorphTargetCurves()
    {
        return MorphTargetCurves;
    }

    int32 GetNumberOfFrames() const
    {
        return NumFrames;
    }

    void SetTiming(float InPlayLength, float InFrameRate, int32 InNumFrames)
    {
        PlayLength = InPlayLength;
        FrameRate  = InFrameRate;
        NumFrames  = InNumFrames;
    }
};
