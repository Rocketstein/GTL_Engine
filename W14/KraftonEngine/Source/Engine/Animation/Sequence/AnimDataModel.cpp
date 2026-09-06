#include "AnimDataModel.h"
#include "Animation/Notify/AnimNotify.h"
#include "Animation/Notify/AnimNotifyState.h"
#include "Asset/AssetPackage.h"
#include "Object/GarbageCollection.h"
#include "Object/Object.h"

#include <algorithm>

void UAnimDataModel::Serialize(FArchive& Ar)
{
    Serialize(Ar, FAssetPackageHeader::CurrentVersion);
}

void UAnimDataModel::Serialize(FArchive& Ar, uint32 PackageVersion)
{
    UObject::Serialize(Ar);

    Ar << PlayLength;
    Ar << FrameRate;
    Ar << NumFrames;
    Ar << BoneAnimationTracks;

    // Morph curves are authored data owned by this animation. They are appended before notifies.
    Ar << MorphTargetCurves;

    // Notifies 는 Outer 인지 직렬화 — Notify/NotifyState 객체 클래스명 + UPROPERTY(Save) payload
    // 까지 round-trip. ObjectFactory::Create 가 Outer 를 받아야 라이프타임 체인 형성되므로
    // TArray operator<< (raw 만) 사용 못 함, 명시적 루프로 entry 별 Serialize(Ar, this) 호출.
    const bool bHasNotifyTracks =
        PackageVersion >= static_cast<uint32>(EAssetPackageSerializationVersion::AnimNotifyTracksPayload);
    if (bHasNotifyTracks)
    {
        Ar << NotifyTrackCount;
    }
    else if (Ar.IsLoading())
    {
        NotifyTrackCount = 1;
    }

    int32 NotifyCount = static_cast<int32>(Notifies.size());
    Ar << NotifyCount;
    if (Ar.IsLoading())
    {
        Notifies.clear();
        Notifies.resize(NotifyCount);
    }
    for (int32 i = 0; i < NotifyCount; ++i)
    {
        Notifies[i].Serialize(Ar, this, bHasNotifyTracks);
    }

    if (Ar.IsLoading())
    {
        NotifyTrackCount = std::clamp(NotifyTrackCount, 1, MaxNotifyTracks);
        for (FAnimNotifyEvent& Notify : Notifies)
        {
            Notify.TrackIndex = std::clamp(Notify.TrackIndex, 0, NotifyTrackCount - 1);
        }
    }
}


void UAnimDataModel::AddReferencedObjects(FReferenceCollector& Collector)
{
    UObject::AddReferencedObjects(Collector);
    for (FAnimNotifyEvent& NotifyEvent : Notifies)
    {
        if (NotifyEvent.Notify && !IsValid(NotifyEvent.Notify))
        {
            NotifyEvent.Notify = nullptr;
        }
        if (NotifyEvent.NotifyState && !IsValid(NotifyEvent.NotifyState))
        {
            NotifyEvent.NotifyState = nullptr;
        }
        Collector.AddReferencedObject(NotifyEvent.Notify, "AnimDataModel.Notify");
        Collector.AddReferencedObject(NotifyEvent.NotifyState, "AnimDataModel.NotifyState");
    }
}
