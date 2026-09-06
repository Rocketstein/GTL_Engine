#pragma once

#include "Component/Primitive/SkinnedMeshComponent.h"
#include "Animation/AnimationMode.h"
#include "Object/Ptr/SubclassOf.h"
#include "Physics/PhysicsHandles.h"
#include "Physics/IPhysicsBodySync.h"

#include "Source/Engine/Component/Primitive/SkeletalMeshComponent.generated.h"

class UAnimInstance;
class UAnimSingleNodeInstance;
class UAnimSequenceBase;
class UClass;
class UPhysicsAsset;
struct FBodyInstance;
struct FConstraintInstance;

// SkeletalMesh 전용 render proxy만 제공하는 얇은 wrapper.
// Skinning/bone/material/bounds 상태는 모두 USkinnedMeshComponent가 소유한다.
UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent, public IPhysicsBodySync
{
public:
	GENERATED_BODY()
	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override;

    // Render access 섹션: SceneProxy
    FPrimitiveSceneProxy* CreateSceneProxy() override;

    // Mesh 가 바뀌면 AnimInstance 도 새 SkeletalMesh 기준으로 재구성해야 하므로 override.
    void SetSkeletalMesh(USkeletalMesh* InMesh) override;

    // SingleNode 재생 편의 API.
    void PlayAnimation(UAnimSequenceBase* NewAnimToPlay, bool bLooping);
    void StopAnimation();

    // Animation 섹션: Mode 에 따라 AnimInstance 의 생성/파기를 컴포넌트가 책임진다.
    //   - None              : AnimInstance 미생성. BoneEdit 만 적용.
    //   - AnimationSingleNode: UAnimSingleNodeInstance 자동 생성, AnimationData 로 구동.
    //   - AnimationCustom   : AnimInstanceClass 가 가리키는 자식 클래스를 FObjectFactory 로 인스턴스화.
    void SetAnimationMode(EAnimationMode InMode);
    EAnimationMode GetAnimationMode() const { return AnimationMode; }

    // SingleNode 모드용 헬퍼. Custom 모드에선 무시 (자체 인스턴스가 자체 시퀀스를 관리).
    void SetAnimation(UAnimSequenceBase* InAsset);
    bool CanUseAnimation(UAnimSequenceBase* InAsset) const;
    UAnimSequenceBase* GetAnimation() const { return AnimationData.AnimToPlay.Get(); }
    void SetPlayRate(float InRate);
    void SetLooping(bool bInLoop);
    void SetPlaying(bool bInPlay);
    const FSingleAnimationPlayData& GetAnimationData() const { return AnimationData; }

    // Custom 모드용. 클래스 변경 시 다음 InitializeAnimation 에서 재인스턴스화.
    // 슬롯은 TSubclassOf<UAnimInstance> — 잘못된 클래스 대입은 nullptr 로 흡수.
    void SetAnimInstanceClass(UClass* InClass);
    UClass* GetAnimInstanceClass() const { return AnimInstanceClass.Get(); }

    // 외부에서 직접 만든 인스턴스 주입 (테스트 / 특수 케이스). Mode 와 무관하게 즉시 교체.
    void SetAnimInstance(UAnimInstance* InInstance);
    UAnimInstance* GetAnimInstance() const { return AnimInstance; }

    void SetPhysicsAssetOverride(UPhysicsAsset* InPhysicsAsset);
    UPhysicsAsset* GetPhysicsAssetOverride() const { return PhysicsAssetOverride; }
    UPhysicsAsset* GetPhysicsAsset() const;

    // 랙돌 제어: 바디별 PhysicsBlendWeight(0=순수 anim, 1=순수 물리)로 anim 포즈와 시뮬
    // 포즈를 본별 블렌드한다. SetSimulatePhysics 는 전체 바디를 0/1 로 두는 단축 API.
    void SetSimulatePhysics(bool bSimulate) override;
    bool IsSimulatingPhysics() const;   // 활성 바디(weight 또는 target>0)가 하나라도 있으면 true

    // 전체 바디의 블렌드 가중치 설정([0,1] 클램프). bInterpolate=true 면 PhysicsBlendInterpSpeed
    // 로 목표까지 보간(서서히 쓰러짐/일어남), false 면 즉시. weight>0 이 되면 바디가 없을 경우
    // RefPose 로 인스턴스화한다.
    void  SetPhysicsBlendWeight(float Weight, bool bInterpolate = true);
    // 부분 래그돌: 특정 본(및 옵션으로 그 하위 본 전체)의 바디만 블렌드 가중치 설정.
    //   예) 오른팔만 물리: SetBodyPhysicsBlendWeight("Bip001 R UpperArm", 1.0f, true). 나머지는 anim 유지.
    void  SetBodyPhysicsBlendWeight(FName BoneName, float Weight, bool bIncludeChildren = true, bool bInterpolate = true);
    float GetPhysicsBlendWeight() const;   // 최대 바디 가중치(정보용)
    void  SetPhysicsBlendInterpSpeed(float InSpeed) { PhysicsBlendInterpSpeed = InSpeed; }

    // 에디터 프리뷰 전용: Scene->Tick 직전에 호출해 기준 포즈를 캐시한다(anim 인스턴스가 없어
    // ref 포즈 사용). 이후 Scene->Tick 이 등록된 핸들러(Pre/PostPhysicsSimulate)를 통해 키네마틱
    // 추종과 시뮬 결과 반영을 구동한다. 게임 월드에선 TickComponent 가 캐시를 채우므로 불필요.
    void  PreparePhysicsPreview(float DeltaTime);

    // IPhysicsBodySync — 씬이 simulate 전후로 호출(World::Tick / 에디터 Scene->Tick).
    //   Pre  : 키네마틱(weight 0) 바디를 캐시된 anim 포즈로 추종(simulate 이전).
    //   Post : 시뮬 결과를 본 포즈에 블렌드 반영(fetchResults 이후).
    void PrePhysicsSimulate(float DeltaTime) override;
    void PostPhysicsSimulate(float DeltaTime) override;
    const TArray<FBodyInstance*>& GetBodies() const { return Bodies; }
    const TArray<FConstraintInstance*>& GetConstraints() const { return Constraints; }
    FBodyInstance* GetBodyInstance(FName BoneName) const;
    FBodyInstance* GetBodyInstance(int32 BoneIndex) const;

    // 랙돌 본에 힘/임펄스 적용 — 컴포넌트 바디와 동일한 handle 기반 force 경로를 공유한다.
    // 해당 본의 FBodyInstance 가 시뮬레이션(dynamic) 중일 때만 효과(키네마틱/없음이면 no-op).
    void AddForceToBone(FName BoneName, const FVector& Force);
    void AddForceToAllBodies(const FVector& Force);
    FConstraintInstance* GetConstraintInstance(FName ChildBoneName) const;

    // SingleNode 모드에서 현재 자동 생성된 노드를 반환한다. NodeName 은 현재 단일 노드 구조에서는 무시한다.
    UAnimSingleNodeInstance* GetAnimNodeInstance(FName NodeName) const;

    // Mode/Class/SkeletalMesh 변경 후 일관성 재정렬. SetSkeletalMesh override 안에서 자동 호출됨.
    void InitializeAnimation();
    void ClearAnimInstance();

    // Editor / 직렬화 통합.
    void GetEditableProperties(TArray<FPropertyValue>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void Serialize(FArchive& Ar) override;

protected:
    void OnCreatePhysicsState() override;
    void OnDestroyPhysicsState() override;
    bool ShouldCreatePhysicsState() const override;
    bool HasValidPhysicsState() const override;

    // 매 프레임 AnimInstance 평가 → 결과 포즈를 SetBoneLocalTransforms 로 푸시.
    // 이 경로가 CPU skinning 과 bounds dirty 를 한 번에 처리한다.
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

    bool EvaluateAnimInstance(float DeltaTime);

private:
    void LoadAnimationFromPath();
    bool InstantiatePhysicsAssetRefPose();
    bool InstantiatePhysicsAsset_Internal(UPhysicsAsset* InPhysicsAsset, const TArray<FTransform>& BoneWorldTransforms);
    void TermArticulated();

    // anim 컴포넌트-글로벌(AnimGlobals)과 각 바디의 시뮬 포즈를 그 바디의 PhysicsBlendWeight
    // 로 본별 블렌드(컴포넌트 공간 위치 lerp + 회전 slerp)해 SetBoneLocalTransforms 로 푸시.
    // weight=1 본은 순수 물리, 0 본은 순수 anim. 바디 없는 본은 anim 로컬을 블렌드된 부모에 누적.
    void ApplyPhysicsBlendedPose(const TArray<FMatrix>& AnimGlobals);

    // weight==0(키네마틱) 바디만 anim 본 월드 변환을 키네마틱 타깃으로 밀어 추종시킨다.
    // (dynamic 바디에 호출하면 강제로 키네마틱 전환되므로 반드시 제외.) 부분 래그돌에서
    // dynamic 자식의 조인트가 애니메이션되는 부모에 올바르게 앵커되도록 보장.
    void SyncKinematicBodiesToAnim(const TArray<FMatrix>& AnimGlobals);

    // 레퍼런스 포즈 기준 컴포넌트-공간 글로벌(블렌드 anim 기준이 없을 때 폴백).
    void BuildReferencePoseGlobals(TArray<FMatrix>& OutGlobals) const;

    // 각 바디 weight 를 Target 으로 InterpSpeed 만큼 보간.
    void RampBodyBlendWeights(float DeltaTime);
    // 각 바디를 (weight>0 || target>0) 이면 다이내믹, 아니면 키네마틱으로 전환(변화 시에만).
    void UpdateBodySimulationState();
    // 활성 바디(weight 또는 target>0)가 하나라도 있는지.
    bool AnyBodyPhysicsActive() const;
    // BoneName 의 본 인덱스와 (bIncludeChildren 면) 그 하위 본 전체를 OutIndices 에 모은다.
    void CollectBoneSubtree(FName BoneName, bool bIncludeChildren, TArray<int32>& OutIndices) const;

protected:
    // Animation 런타임 상태.
    UPROPERTY(Edit, Save, Category="Animation", DisplayName="Animation Mode", Enum=EAnimationMode)
    EAnimationMode             AnimationMode = EAnimationMode::None;
    UPROPERTY(Edit, Save, Category="Animation", DisplayName="Animation Data", Type=Struct)
    FSingleAnimationPlayData   AnimationData;
    UPROPERTY(Edit, Save, Category="Animation", DisplayName="Anim Instance Class", Type=ClassRef, AllowedClass=UAnimInstance)
    TSubclassOf<UAnimInstance> AnimInstanceClass;
    UPROPERTY(Save, Instanced, Category="Animation", DisplayName="Anim Instance", Type=ObjectRef, AllowedClass=UAnimInstance)
    UAnimInstance*             AnimInstance  = nullptr;

    UPhysicsAsset*             PhysicsAssetOverride = nullptr;
    TArray<FBodyInstance*>     Bodies;
    TArray<FConstraintInstance*> Constraints;
    // 이 랙돌의 모든 본 바디를 묶는 aggregate(selfCollision=false). 바디들끼리의 내부 충돌을
    // broad-phase 에서 차단한다. TermArticulated 에서 바디 해제 후 마지막에 release.
    FPhysicsAggregateHandle    RagdollAggregate;

    // pre-simulate 단계(TickComponent/PreparePhysicsPreview)에서 캐시되는 anim 기준 포즈와
    // 블렌드 여부. simulate 전후의 Pre/PostPhysicsSimulate 핸들러가 이 캐시를 소비한다 —
    // 한 프레임 안에서 anim→sync→simulate→blend 가 같은 포즈로 일관되게 묶이도록.
    TArray<FMatrix>            CachedAnimGlobals;
    bool                       bCachedWantBlend = false;

    // 블렌드 가중치는 바디별(FBodyInstance::PhysicsBlendWeight/Target)로 보관한다.
    //   weight 0   : 순수 anim(바디 키네마틱 추종)
    //   0<weight<1 : anim↔시뮬 본별 블렌드
    //   weight 1   : 순수 랙돌
    // 부분 래그돌은 일부 바디만 weight>0 으로 둬서 "맞은 부위만" 물리가 되게 한다.
    float                      PhysicsBlendInterpSpeed = 4.0f;  // /sec (~0.25s, 0 이하면 즉시)
};
