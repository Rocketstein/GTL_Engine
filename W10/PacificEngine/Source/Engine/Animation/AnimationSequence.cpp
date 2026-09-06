#include "Animation/AnimationSequence.h"
#include "Mesh/SkeletonManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UAnimationSequence, UObject)

// WARNING: FBX Importer 작성하면서 AI가 추가한 초안. 테스트되지 않은 코드입니다.
void UAnimationSequence::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    // 스켈레톤 참조 정보 직렬화 (이름 기반으로 저장하고 로드 시 SkeletonManager를 통해 해결)
    FString SkeletonPath;
    if (Ar.IsSaving() && Skeleton)
    {
        SkeletonPath = Skeleton->GetFName().ToString(); 
    }
    
    Ar << SkeletonPath;

    // 시퀀스 기본 정보 및 본 트랙 데이터 직렬화
    Ar << NumFrames;
    Ar << SequenceLength;
    Ar << FPS;
    Ar << Tracks;
}
