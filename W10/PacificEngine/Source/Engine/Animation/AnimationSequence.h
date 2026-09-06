#pragma once

#include "Object/Object.h"
#include "Mesh/Skeleton.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

struct FRawAnimSequenceTrack
{
    TArray<FVector> PosKeys;
    TArray<FQuat> RotKeys;
    TArray<FVector> ScaleKeys;

    friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& Track)
    {
        Ar << Track.PosKeys << Track.RotKeys << Track.ScaleKeys;
        return Ar;
    }
};

// WARNING: FBX Importer 작성하면서 AI가 추가한 초안. 테스트되지 않은 코드입니다.
class UAnimationSequence : public UObject
{
public:
    DECLARE_CLASS(UAnimationSequence, UObject)

    UAnimationSequence() = default;

    void Serialize(FArchive& Ar) override;

    void SetSkeleton(USkeleton* InSkeleton) { Skeleton = InSkeleton; }
    USkeleton* GetSkeleton() const { return Skeleton; }

    void SetNumFrames(int32 InNum) { NumFrames = InNum; }
    int32 GetNumFrames() const { return NumFrames; }

    void SetSequenceLength(float InLen) { SequenceLength = InLen; }
    float GetSequenceLength() const { return SequenceLength; }

    void SetFPS(float InFPS) { FPS = InFPS; }
    float GetFPS() const { return FPS; }

    TArray<FRawAnimSequenceTrack>& GetTracks() { return Tracks; }
    const TArray<FRawAnimSequenceTrack>& GetTracks() const { return Tracks; }

private:
    USkeleton* Skeleton = nullptr;
    TArray<FRawAnimSequenceTrack> Tracks; // Index matches bone index in USkeleton
    int32 NumFrames = 0;
    float SequenceLength = 0.0f;
    float FPS = 30.0f;
};
// WARNING: FBX Importer 작성하면서 AI가 추가한 초안. 테스트되지 않은 코드입니다.