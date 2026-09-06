#include "SkeletalMeshComponent.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Sequence/AnimSequenceBase.h"
#include "Animation/Instance/AnimSingleNodeInstance.h"
#include "Animation/PoseContext.h"
#include "Asset/AssetRegistry.h"
#include "Core/Logging/Log.h"
#include "Core/ProjectSettings.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Reflection/UClass.h"
#include "Physics/Asset/BodySetup.h"
#include "Physics/Asset/PhysicsAsset.h"
#include "Physics/BodyInstance.h"
#include "Physics/ConstraintInstance.h"
#include "Physics/IPhysicsScene.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    // 스케일이 섞인 affine 행렬에서 rigid FTransform(스케일 1)을 추출한다.
    // FQuat::FromMatrix 는 스케일을 나눠내지 않고 raw 원소로 쿼터니언을 뽑은 뒤 마지막에
    // Normalize 만 하므로, 비단위 스케일이 섞이면 회전 자체가 왜곡된다
    // (예: uniform 2배 스케일에서 90도 회전이 ~106도로 추출됨).
    // 물리<->본 변환은 컴포넌트 월드 스케일(S)의 역수(1/S)를 행렬에 싣게 되므로,
    // 회전 추출 전에 각 축을 정규화해 스케일을 제거해야 한다. 래그돌 본은 rigid 이므로
    // 스케일은 1로 고정.
    FTransform MakeRigidTransform(const FMatrix& Mat)
    {
        const FVector Scale = Mat.GetScale();
        const float SX = Scale.X > 1e-6f ? Scale.X : 1.0f;
        const float SY = Scale.Y > 1e-6f ? Scale.Y : 1.0f;
        const float SZ = Scale.Z > 1e-6f ? Scale.Z : 1.0f;

        FMatrix Rot = Mat;
        Rot.M[0][0] /= SX; Rot.M[0][1] /= SX; Rot.M[0][2] /= SX;
        Rot.M[1][0] /= SY; Rot.M[1][1] /= SY; Rot.M[1][2] /= SY;
        Rot.M[2][0] /= SZ; Rot.M[2][1] /= SZ; Rot.M[2][2] /= SZ;
        Rot.M[3][0] = 0.0f; Rot.M[3][1] = 0.0f; Rot.M[3][2] = 0.0f;

        return FTransform(Mat.GetLocation(), Rot.ToQuat(), FVector(1.0f, 1.0f, 1.0f));
    }

    // 두 스케일-1 컴포넌트-글로벌(Anim, Phys)을 Weight 로 블렌드한 rigid 행렬을 반환한다.
    // 위치는 선형 보간, 회전은 구면 보간(slerp). Weight 0=Anim, 1=Phys.
    FMatrix BlendComponentGlobal(const FMatrix& Anim, const FMatrix& Phys, float Weight)
    {
        const FVector PA = Anim.GetLocation();
        const FVector PP = Phys.GetLocation();
        const FVector P(
            PA.X + (PP.X - PA.X) * Weight,
            PA.Y + (PP.Y - PA.Y) * Weight,
            PA.Z + (PP.Z - PA.Z) * Weight);

        // 스케일 1 행렬이라 ToQuat 안전.
        const FQuat Q = FQuat::Slerp(Anim.ToQuat(), Phys.ToQuat(), Weight);
        return FTransform(P, Q, FVector(1.0f, 1.0f, 1.0f)).ToMatrix();
    }
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    DestroyPhysicsState();
    ClearAnimInstance();
}

void USkeletalMeshComponent::OnCreatePhysicsState()
{
    if (InstantiatePhysicsAssetRefPose())
    {
        // 래그돌 트리거 전까지는 모든 바디가 키네마틱으로 anim 포즈를 추종한다.
        // (저작된 PhysicsType 이 Simulated 라도 시작 시 떨어지지 않도록 강제.)
        UWorld* World = GetWorld();
        IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
        if (PhysicsScene)
        {
            for (FBodyInstance* Body : Bodies)
            {
                if (Body && Body->IsValidBodyInstance())
                {
                    Body->SetInstanceSimulatePhysics(PhysicsScene, false);
                }
            }
        }

        UActorComponent::OnCreatePhysicsState();
    }
}

void USkeletalMeshComponent::OnDestroyPhysicsState()
{
    TermArticulated();
    UActorComponent::OnDestroyPhysicsState();
}

bool USkeletalMeshComponent::ShouldCreatePhysicsState() const
{
    // physics asset 이 연결돼 있으면 충돌-쿼리 enable 여부와 무관하게 바디를 생성한다.
    // (IsCollisionEnabled 는 scene query 등록을 게이트할 뿐 articulated body 존재와 별개.
    //  바디가 스폰 시 만들어져야 키네마틱으로 anim 을 추종하다가 자연스럽게 래그돌로 전환됨.)
    // World/PhysicsScene 존재는 상위 UActorComponent::RecreatePhysicsState 가 이미 검사.
    UPhysicsAsset* PhysAsset = GetPhysicsAsset();
    return GetSkeletalMesh() != nullptr && PhysAsset && !PhysAsset->BodySetups.empty();
}

bool USkeletalMeshComponent::HasValidPhysicsState() const
{
    return IsPhysicsStateCreated() && !Bodies.empty();
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
    return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
    const bool bRecreatePhysicsState = IsPhysicsStateCreated();
    if (bRecreatePhysicsState)
    {
        DestroyPhysicsState();
    }

    Super::SetSkeletalMesh(InMesh);
    // Mesh 가 바뀌면 이전 AnimInstance 가 가리키던 본 인덱스/카운트가 무의미해진다.
    // 새 SkeletalMesh 기준으로 AnimInstance 를 재인스턴스화한다.
    InitializeAnimation();

    if (bRecreatePhysicsState)
    {
        CreatePhysicsState();
    }
}

void USkeletalMeshComponent::SetPhysicsAssetOverride(UPhysicsAsset* InPhysicsAsset)
{
    if (PhysicsAssetOverride == InPhysicsAsset)
    {
        return;
    }

    const bool bRecreatePhysicsState = IsPhysicsStateCreated();
    if (bRecreatePhysicsState)
    {
        DestroyPhysicsState();
    }

    PhysicsAssetOverride = InPhysicsAsset;

    if (bRecreatePhysicsState)
    {
        CreatePhysicsState();
    }
}

UPhysicsAsset* USkeletalMeshComponent::GetPhysicsAsset() const
{
    if (PhysicsAssetOverride)
    {
        return PhysicsAssetOverride;
    }

    USkeletalMesh* Mesh = GetSkeletalMesh();
    return Mesh ? Mesh->GetPhysicsAsset() : nullptr;
}

FBodyInstance* USkeletalMeshComponent::GetBodyInstance(FName BoneName) const
{
    for (FBodyInstance* Body : Bodies)
    {
        UBodySetup* Setup = Body ? Body->GetBodySetup() : nullptr;
        if (Setup && Setup->BoneName == BoneName)
        {
            return Body;
        }
    }

    return nullptr;
}

FBodyInstance* USkeletalMeshComponent::GetBodyInstance(int32 BoneIndex) const
{
    for (FBodyInstance* Body : Bodies)
    {
        if (Body && Body->InstanceBoneIndex == BoneIndex)
        {
            return Body;
        }
    }

    return nullptr;
}

void USkeletalMeshComponent::AddForceToBone(FName BoneName, const FVector& Force)
{
    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    if (!PhysicsScene) return;

    if (FBodyInstance* Body = GetBodyInstance(BoneName))
    {
        if (Body->IsValidBodyInstance())
        {
            PhysicsScene->AddForce(Body->GetPhysicsActorHandle(), Force);
        }
    }
}

void USkeletalMeshComponent::AddForceToAllBodies(const FVector& Force)
{
    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    if (!PhysicsScene) return;

    for (FBodyInstance* Body : Bodies)
    {
        if (Body && Body->IsValidBodyInstance())
        {
            PhysicsScene->AddForce(Body->GetPhysicsActorHandle(), Force);
        }
    }
}

FConstraintInstance* USkeletalMeshComponent::GetConstraintInstance(FName ChildBoneName) const
{
    for (FConstraintInstance* Constraint : Constraints)
    {
        if (Constraint && Constraint->ConstraintBone1 == ChildBoneName)
        {
            return Constraint;
        }
    }

    return nullptr;
}

void USkeletalMeshComponent::PlayAnimation(UAnimSequenceBase* NewAnimToPlay, bool bLooping)
{
    SetAnimationMode(EAnimationMode::AnimationSingleNode);
    SetAnimation(NewAnimToPlay);
    SetLooping(bLooping);
    SetPlaying(NewAnimToPlay != nullptr);
}

void USkeletalMeshComponent::StopAnimation()
{
    SetAnimation(nullptr);
    SetPlaying(false);

    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetCurrentTime(0.0f);
    }
}

// ──────────────────────────────────────────────
// Animation API
// ──────────────────────────────────────────────
void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InMode)
{
    if (AnimationMode == InMode) return;
    AnimationMode = InMode;
    InitializeAnimation();
}

bool USkeletalMeshComponent::CanUseAnimation(UAnimSequenceBase* InAsset) const
{
    if (!InAsset)
    {
        return true;
    }

    const USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh)
    {
        return false;
    }

    if (const UAnimSequence* Sequence = Cast<UAnimSequence>(InAsset))
    {
        FSkeletonCompatibilityReport Report;
        const bool bCompatible = FAssetRegistry::CheckAnimationForMesh(Sequence, Mesh, &Report);
        if (!bCompatible)
        {
            UE_LOG("SetAnimation rejected: skeleton mismatch. Anim=%s Mesh=%s Reason=%s",
                Sequence->GetName().c_str(),
                Mesh->GetName().c_str(),
                Report.Reason.c_str());
        }
        return bCompatible;
    }

    return true;
}

void USkeletalMeshComponent::SetAnimation(UAnimSequenceBase* InAsset)
{
    if (!CanUseAnimation(InAsset))
    {
        return;
    }

    AnimationData.AnimToPlay = InAsset;

    if (UAnimSequence* Sequence = Cast<UAnimSequence>(InAsset))
    {
        AnimationData.AnimToPlayPath = Sequence->GetAssetPathFileName();
    }
    else if (!InAsset)
    {
        AnimationData.AnimToPlayPath = "None";
    }

    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetAnimationAsset(InAsset);
    }
}

void USkeletalMeshComponent::SetPlayRate(float InRate)
{
    AnimationData.PlayRate = InRate;
    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetPlayRate(InRate);
    }
}

void USkeletalMeshComponent::SetLooping(bool bInLoop)
{
    AnimationData.bLooping = bInLoop;
    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetLooping(bInLoop);
    }
}

void USkeletalMeshComponent::SetPlaying(bool bInPlay)
{
    AnimationData.bPlaying = bInPlay;
    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetPlaying(bInPlay);
    }
}

void USkeletalMeshComponent::SetAnimInstanceClass(UClass* InClass)
{
    if (AnimInstanceClass.Get() == InClass) return;
    AnimInstanceClass = InClass;   // TSubclassOf 가 IsA 가드로 검증 (잘못된 클래스 → nullptr).
    if (AnimationMode == EAnimationMode::AnimationCustom)
    {
        InitializeAnimation();
    }
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InInstance)
{
    if (AnimInstance == InInstance) return;
    ClearAnimInstance();
    AnimInstance = InInstance;
    if (AnimInstance)
    {
        AnimInstance->SetOuter(this);
        AnimInstance->SetOwningComponent(this);
        AnimInstance->NativeInitializeAnimation();
    }
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetAnimNodeInstance(FName NodeName) const
{
    (void)NodeName;
    return Cast<UAnimSingleNodeInstance>(AnimInstance);
}

void USkeletalMeshComponent::LoadAnimationFromPath()
{
    AnimationData.AnimToPlay = nullptr;

    if (AnimationData.AnimToPlayPath.empty() || AnimationData.AnimToPlayPath == "None")
    {
        return;
    }

    UAnimSequence* LoadedAnimation = FAnimationManager::Get().LoadAnimation(AnimationData.AnimToPlayPath.ToString());
    if (LoadedAnimation && CanUseAnimation(LoadedAnimation))
    {
        AnimationData.AnimToPlay = LoadedAnimation;
    }
    else
    {
        AnimationData.AnimToPlay = nullptr;
    }
}

void USkeletalMeshComponent::InitializeAnimation()
{
    if (!GetSkeletalMesh())
    {
        ClearAnimInstance();
        return;
    }
    if (AnimationMode == EAnimationMode::None)
    {
        ClearAnimInstance();
        return;
    }

    if (AnimationMode == EAnimationMode::AnimationSingleNode &&
        !AnimationData.AnimToPlay &&
        !AnimationData.AnimToPlayPath.empty() &&
        AnimationData.AnimToPlayPath != "None")
    {
        LoadAnimationFromPath();
    }

    if (AnimationMode == EAnimationMode::AnimationSingleNode && !CanUseAnimation(AnimationData.AnimToPlay))
    {
        AnimationData.AnimToPlay = nullptr;
        AnimationData.AnimToPlayPath = "None";
    }

    switch (AnimationMode)
    {
    case EAnimationMode::AnimationSingleNode:
    {
        ClearAnimInstance();

        UAnimSingleNodeInstance* Single =
            UObjectManager::Get().CreateObject<UAnimSingleNodeInstance>(this);
        AnimInstance = Single;
        Single->SetOwningComponent(this);
        Single->SetAnimationAsset(AnimationData.AnimToPlay);
        Single->SetPlayRate(AnimationData.PlayRate);
        Single->SetLooping(AnimationData.bLooping);
        Single->SetPlaying(AnimationData.bPlaying && AnimationData.AnimToPlay != nullptr);
        Single->NativeInitializeAnimation();
        break;
    }
    case EAnimationMode::AnimationCustom:
    {
        UClass* DesiredClass = AnimInstanceClass.Get();
        if (!DesiredClass)
        {
            ClearAnimInstance();
            return;
        }

        if (AnimInstance && AnimInstance->GetClass() == DesiredClass)
        {
            AnimInstance->SetOuter(this);
            AnimInstance->SetOwningComponent(this);
            AnimInstance->NativeInitializeAnimation();
            break;
        }

        ClearAnimInstance();

        UObject* Obj = FObjectFactory::Get().Create(DesiredClass->GetName(), this);
        AnimInstance = Cast<UAnimInstance>(Obj);
		if (!AnimInstance)
        {
            // 클래스가 등록 안됐거나 캐스트 실패 — 무관한 객체가 생성됐을 수 있으니 정리.
            if (Obj) UObjectManager::Get().DestroyObject(Obj);
            return;
        }
        AnimInstance->SetOwningComponent(this);

        AnimInstance->NativeInitializeAnimation();
        break;
    }
    default:
        break;
    }
}

void USkeletalMeshComponent::ClearAnimInstance()
{
    if (AnimInstance)
    {
        UObjectManager::Get().DestroyObject(AnimInstance);
        AnimInstance = nullptr;
    }
}

bool USkeletalMeshComponent::InstantiatePhysicsAssetRefPose()
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    UPhysicsAsset* PhysAsset = GetPhysicsAsset();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!PhysAsset || !Asset || Asset->Bones.empty())
    {
        return false;
    }

    TArray<FTransform> BoneWorldTransforms;
    BoneWorldTransforms.resize(Asset->Bones.size());

    const FMatrix& ComponentToWorld = GetWorldMatrix();
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Asset->Bones.size()); ++BoneIndex)
    {
        // 스케일-안전 추출: ComponentToWorld 의 스케일이 FTransform 분해 시 회전을
        // 왜곡하지 않도록 rigid 변환으로 만든다. (바디 actor 는 스케일을 갖지 않음.)
        const FMatrix BoneWorldMatrix = Asset->Bones[BoneIndex].GetReferenceGlobalPose() * ComponentToWorld;
        BoneWorldTransforms[BoneIndex] = MakeRigidTransform(BoneWorldMatrix);
    }

    return InstantiatePhysicsAsset_Internal(PhysAsset, BoneWorldTransforms);
}

bool USkeletalMeshComponent::InstantiatePhysicsAsset_Internal(
    UPhysicsAsset* InPhysicsAsset,
    const TArray<FTransform>& BoneWorldTransforms)
{
    TermArticulated();

    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!InPhysicsAsset || !PhysicsScene || !Asset || BoneWorldTransforms.empty())
    {
        return false;
    }

    // 랙돌 self-collision 차단은 두 메커니즘을 병용한다:
    //  1) aggregate(selfCollision=false): 이 랙돌의 본 바디들끼리는 broad-phase 에서 충돌 제외.
    //     비인접 바디(WorldScale 로 부푼 캡슐들)가 겹쳐 다이내믹 전환 시 폭발하던 문제를 차단.
    //  2) self-collision 그룹(owner UUID, filter word3): aggregate 밖에 있는 자기 캐릭터 캡슐
    //     (컴포넌트 경로 바디)과의 충돌까지 막는다. 캡슐은 다른 생성 경로라 aggregate 에 못 넣는다.
    AActor* OwnerActor = GetOwner();
    const uint32 SelfCollisionGroup = OwnerActor ? OwnerActor->GetUUID() : GetUUID();

    // 본 바디 전체를 한 aggregate 로 묶는다(상한은 BodySetups 수). 프로젝트 세팅으로 토글 —
    // 끄면 바디를 씬에 직접 추가(성능 A/B 비교용). 어느 경우든 self-collision 은 아래
    // SetActorSelfCollisionGroup(filter word3)로 차단되므로 끄더라도 폭발하지 않는다.
    const bool bUseAggregate = FProjectSettings::Get().Physics.bUseRagdollAggregate;
    RagdollAggregate = bUseAggregate
        ? PhysicsScene->CreateAggregate(static_cast<uint32>(InPhysicsAsset->BodySetups.size()), /*bSelfCollision=*/false)
        : FPhysicsAggregateHandle{};

    const FVector WorldScale = GetWorldScale();
    for (UBodySetup* BodySetup : InPhysicsAsset->BodySetups)
    {
        if (!BodySetup)
        {
            continue;
        }

        const int32 BoneIndex = FindBoneIndex(BodySetup->BoneName.ToString());
        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(BoneWorldTransforms.size()))
        {
            UE_LOG("PhysicsAsset body skipped: bone not found. Mesh=%s Bone=%s",
                Mesh->GetName().c_str(),
                BodySetup->BoneName.ToString().c_str());
            continue;
        }

        FBodyInstance* BodyInstance = new FBodyInstance();
        BodyInstance->Scale3D = WorldScale;

        if (BodyInstance->InitBody(BodySetup, BoneWorldTransforms[BoneIndex], PhysicsScene, BoneIndex, RagdollAggregate))
        {
            PhysicsScene->SetActorSelfCollisionGroup(BodyInstance->GetPhysicsActorHandle(), SelfCollisionGroup);
            Bodies.push_back(BodyInstance);
        }
        else
        {
            delete BodyInstance;
            UE_LOG("PhysicsAsset body creation failed. Mesh=%s Bone=%s",
                Mesh->GetName().c_str(),
                BodySetup->BoneName.ToString().c_str());
        }
    }

    for (const FConstraintSetup& ConstraintSetup : InPhysicsAsset->ConstraintSetups)
    {
        FBodyInstance* ChildBody = GetBodyInstance(ConstraintSetup.ChildBone);
        FBodyInstance* ParentBody = GetBodyInstance(ConstraintSetup.ParentBone);
        if (!ChildBody || !ParentBody)
        {
            UE_LOG("PhysicsAsset constraint skipped: body not found. Mesh=%s Parent=%s Child=%s",
                Mesh->GetName().c_str(),
                ConstraintSetup.ParentBone.ToString().c_str(),
                ConstraintSetup.ChildBone.ToString().c_str());
            continue;
        }

        FConstraintInstance* ConstraintInstance = new FConstraintInstance();
        if (ConstraintInstance->InitConstraint(PhysicsScene, ChildBody, ParentBody, &ConstraintSetup, WorldScale))
        {
            Constraints.push_back(ConstraintInstance);
        }
        else
        {
            delete ConstraintInstance;
            UE_LOG("PhysicsAsset constraint creation failed. Mesh=%s Parent=%s Child=%s",
                Mesh->GetName().c_str(),
                ConstraintSetup.ParentBone.ToString().c_str(),
                ConstraintSetup.ChildBone.ToString().c_str());
        }
    }

    // 씬 주도 sync 핸들러로 등록 — 이후 Start/FinishSimulation 이 Pre/PostPhysicsSimulate 호출.
    if (!Bodies.empty())
    {
        PhysicsScene->RegisterBodySync(this);
    }

    return !Bodies.empty();
}

void USkeletalMeshComponent::TermArticulated()
{
    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;

    // 바디를 해제하기 전에 sync 핸들러부터 해제 — 반쯤 해체된 상태로 호출되지 않게.
    if (PhysicsScene)
    {
        PhysicsScene->UnregisterBodySync(this);
    }

    for (FConstraintInstance* Constraint : Constraints)
    {
        if (Constraint)
        {
            Constraint->TermConstraint(PhysicsScene);
            delete Constraint;
        }
    }
    Constraints.clear();

    for (FBodyInstance* Body : Bodies)
    {
        if (Body)
        {
            Body->TermBody(PhysicsScene);
            delete Body;
        }
    }
    Bodies.clear();

    // 바디를 모두 해제(aggregate 에서 자동 분리)한 뒤 빈 aggregate 를 해제한다.
    if (PhysicsScene && RagdollAggregate.IsValid())
    {
        PhysicsScene->ReleaseAggregate(RagdollAggregate);
    }
    RagdollAggregate = {};
}

void USkeletalMeshComponent::SetSimulatePhysics(bool bSimulate)
{
    Super::SetSimulatePhysics(bSimulate);
    // 단축 API: 켜면 전체 바디 가중치 1(순수 랙돌), 끄면 0(순수 anim) 으로 보간 전환.
    SetPhysicsBlendWeight(bSimulate ? 1.0f : 0.0f);
}

void USkeletalMeshComponent::SetPhysicsBlendWeight(float Weight, bool bInterpolate)
{
    Weight = std::clamp(Weight, 0.0f, 1.0f);

    // 물리가 켜질 예정인데 바디가 없으면 (PhysicsState 미생성) RefPose 로 인스턴스화.
    if (Weight > 0.0f && Bodies.empty())
    {
        InstantiatePhysicsAssetRefPose();
    }

    const bool bImmediate = (!bInterpolate || PhysicsBlendInterpSpeed <= 0.0f);
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body) continue;
        Body->PhysicsBlendWeightTarget = Weight;
        if (bImmediate) Body->PhysicsBlendWeight = Weight;
    }
    UpdateBodySimulationState();
}

void USkeletalMeshComponent::SetBodyPhysicsBlendWeight(FName BoneName, float Weight, bool bIncludeChildren, bool bInterpolate)
{
    Weight = std::clamp(Weight, 0.0f, 1.0f);

    if (Weight > 0.0f && Bodies.empty())
    {
        InstantiatePhysicsAssetRefPose();
    }

    TArray<int32> Indices;
    CollectBoneSubtree(BoneName, bIncludeChildren, Indices);

    const bool bImmediate = (!bInterpolate || PhysicsBlendInterpSpeed <= 0.0f);
    for (int32 BoneIdx : Indices)
    {
        FBodyInstance* Body = GetBodyInstance(BoneIdx);
        if (!Body) continue;
        Body->PhysicsBlendWeightTarget = Weight;
        if (bImmediate) Body->PhysicsBlendWeight = Weight;
    }
    UpdateBodySimulationState();
}

void USkeletalMeshComponent::CollectBoneSubtree(FName BoneName, bool bIncludeChildren, TArray<int32>& OutIndices) const
{
    OutIndices.clear();
    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!Asset) return;

    const int32 Root = FindBoneIndex(BoneName.ToString());
    if (Root < 0) return;
    OutIndices.push_back(Root);
    if (!bIncludeChildren) return;

    // 본은 parent-first 정렬. Root 보다 뒤에 있는 본 중 조상 체인에 Root 가 있으면 하위.
    const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
    for (int32 i = Root + 1; i < BoneCount; ++i)
    {
        int32 P = Asset->Bones[i].ParentIndex;
        while (P >= 0 && P != Root) { P = Asset->Bones[P].ParentIndex; }
        if (P == Root) OutIndices.push_back(i);
    }
}

bool USkeletalMeshComponent::IsSimulatingPhysics() const
{
    return AnyBodyPhysicsActive();
}

bool USkeletalMeshComponent::AnyBodyPhysicsActive() const
{
    for (FBodyInstance* Body : Bodies)
    {
        if (Body && (Body->PhysicsBlendWeight > 0.0f || Body->PhysicsBlendWeightTarget > 0.0f))
        {
            return true;
        }
    }
    return false;
}

float USkeletalMeshComponent::GetPhysicsBlendWeight() const
{
    float MaxWeight = 0.0f;
    for (FBodyInstance* Body : Bodies)
    {
        if (Body) MaxWeight = std::max(MaxWeight, Body->PhysicsBlendWeight);
    }
    return MaxWeight;
}

void USkeletalMeshComponent::RampBodyBlendWeights(float DeltaTime)
{
    const float Step = PhysicsBlendInterpSpeed * DeltaTime;
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body) continue;
        const float Target = Body->PhysicsBlendWeightTarget;
        float& W = Body->PhysicsBlendWeight;
        if (W == Target) continue;

        if (PhysicsBlendInterpSpeed <= 0.0f || std::abs(Target - W) <= Step)
        {
            W = Target;
        }
        else
        {
            W += (Target > W ? Step : -Step);
        }
    }
}

void USkeletalMeshComponent::UpdateBodySimulationState()
{
    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    if (!PhysicsScene)
    {
        return;
    }

    // 바디별로 (weight 또는 target>0)이면 다이내믹, 아니면 키네마틱. 변화 시에만 전환.
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || !Body->IsValidBodyInstance()) continue;
        const bool bShouldSimulate = (Body->PhysicsBlendWeight > 0.0f || Body->PhysicsBlendWeightTarget > 0.0f);
        if (bShouldSimulate != Body->bSimulatePhysics)
        {
            Body->SetInstanceSimulatePhysics(PhysicsScene, bShouldSimulate);
        }
    }
}

void USkeletalMeshComponent::BuildReferencePoseGlobals(TArray<FMatrix>& OutGlobals) const
{
    OutGlobals.clear();
    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!Asset) return;

    const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
    OutGlobals.resize(BoneCount);
    for (int32 i = 0; i < BoneCount; ++i)
    {
        const int32 ParentIndex = Asset->Bones[i].ParentIndex;
        const FMatrix Local = Asset->Bones[i].GetReferenceLocalPose();
        OutGlobals[i] = (ParentIndex >= 0) ? Local * OutGlobals[ParentIndex] : Local;
    }
}

void USkeletalMeshComponent::ApplyPhysicsBlendedPose(const TArray<FMatrix>& AnimGlobals)
{
    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!PhysicsScene || !Asset || Asset->Bones.empty() || Bodies.empty())
    {
        return;
    }

    const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
    if (static_cast<int32>(AnimGlobals.size()) != BoneCount)
    {
        return;
    }

    // 바디 actor 변환은 월드 공간이므로 컴포넌트 공간으로 끌어내린다(스케일 1 정규화).
    const FMatrix WorldToComponent = GetWorldMatrix().GetInverse();

    TArray<FMatrix> BlendedGlobal;   // 본별 블렌드 컴포넌트-공간 글로벌 (parent-first 누적)
    BlendedGlobal.resize(BoneCount);
    TArray<FTransform> LocalPose;
    LocalPose.resize(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const int32 ParentIndex = Asset->Bones[i].ParentIndex;

        FBodyInstance* Body = GetBodyInstance(i);
        if (Body && Body->IsValidBodyInstance() && Body->PhysicsBlendWeight > 0.0f)
        {
            // 시뮬 바디의 컴포넌트-공간 글로벌(스케일 1) 과 anim 글로벌을 바디 weight 로 블렌드.
            const float W = std::clamp(Body->PhysicsBlendWeight, 0.0f, 1.0f);
            const FTransform BodyWorld = Body->GetUnrealWorldTransform(PhysicsScene);
            const FMatrix PhysG = MakeRigidTransform(BodyWorld.ToMatrix() * WorldToComponent).ToMatrix();
            BlendedGlobal[i] = BlendComponentGlobal(AnimGlobals[i], PhysG, W);
        }
        else
        {
            // weight 0 바디 또는 바디 없는 본: anim 로컬을 블렌드된 부모 글로벌에 누적(anim 추종).
            const FMatrix AnimLocal = (ParentIndex >= 0)
                ? AnimGlobals[i] * AnimGlobals[ParentIndex].GetInverse()
                : AnimGlobals[i];
            BlendedGlobal[i] = (ParentIndex >= 0) ? AnimLocal * BlendedGlobal[ParentIndex] : AnimLocal;
        }

        // 블렌드 글로벌(스케일 1)에서 부모 기준 로컬로 환산 → 분해 안전.
        const FMatrix LocalMatrix = (ParentIndex >= 0)
            ? BlendedGlobal[i] * BlendedGlobal[ParentIndex].GetInverse()
            : BlendedGlobal[i];
        LocalPose[i] = FTransform(LocalMatrix);
    }

    SetBoneLocalTransforms(LocalPose);
}

void USkeletalMeshComponent::SyncKinematicBodiesToAnim(const TArray<FMatrix>& AnimGlobals)
{
    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    if (!PhysicsScene || AnimGlobals.empty())
    {
        return;
    }

    const FMatrix& ComponentToWorld = GetWorldMatrix();
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }
        // 다이내믹 바디는 제외 — SetKinematicTarget 이 강제로 키네마틱 전환시켜 시뮬레이션을
        // 깨뜨린다. 판정 기준은 UpdateBodySimulationState 와 동일한 bSimulatePhysics 를 쓴다.
        // (weight>0 만으로 판단하면, 전환 시작 프레임처럼 weight==0 이지만 target>0 이라
        //  이미 다이내믹으로 전환된 바디를 다시 키네마틱으로 강제해 영구 고정된다.)
        if (Body->bSimulatePhysics)
        {
            continue;
        }

        const int32 BoneIndex = Body->InstanceBoneIndex;
        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(AnimGlobals.size()))
        {
            continue;
        }

        // BoneWorldMatrix 에는 컴포넌트 월드 스케일이 실려 있어 단순 FTransform 분해 시
        // 키네마틱 타깃 회전이 깨진다. 스케일-안전 rigid 변환으로 추출.
        const FMatrix BoneWorldMatrix = AnimGlobals[BoneIndex] * ComponentToWorld;
        Body->SetKinematicTarget(PhysicsScene, MakeRigidTransform(BoneWorldMatrix));
    }
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    // 이 tick 은 TG_PrePhysics — 물리 simulate 이전(World::Tick)에 돈다. 여기서는 simulate 의
    // "준비"만 한다: 블렌드 가중치 보간 + 바디 시뮬 상태 + anim 평가 + 기준 포즈 캐시.
    // 실제 키네마틱 타깃 쓰기(Pre)와 시뮬 결과 반영(Post)은 씬이 simulate 전후로
    // PrePhysicsSimulate / PostPhysicsSimulate 핸들러를 통해 구동한다(한 프레임 지연 제거).

    // 1) 바디별 블렌드 가중치를 목표로 보간(부드러운 전환) 후 바디 시뮬 상태 갱신.
    RampBodyBlendWeights(DeltaTime);
    UpdateBodySimulationState();

    // 2) anim 평가 — 블렌드의 기준 포즈. 평가 결과는 BoneEditLocalMatrices 에 반영된다.
    const bool bAnim = EvaluateAnimInstance(DeltaTime);

    // 3) anim 기준 컴포넌트-글로벌을 캐시(핸들러가 simulate 전후로 소비). anim 이 없으면 ref 포즈.
    bCachedWantBlend = AnyBodyPhysicsActive() && !Bodies.empty();
    CachedAnimGlobals.clear();
    if (bCachedWantBlend || !Bodies.empty())
    {
        if (bAnim) BuildBoneEditGlobalMatrices(CachedAnimGlobals);
        else       BuildReferencePoseGlobals(CachedAnimGlobals);
    }

    // 4) 베이스 tick (transform/bounds, no-anim 폴백 시 CPU 스키닝). 기존 분기 보존:
    //    blend/anim 경로는 UMeshComponent(스키닝 생략 — GPU 경로 + Post 에서 본 포즈 갱신),
    //    no-anim·no-blend 폴백만 Super(USkinnedMeshComponent, CPU 스키닝 포함).
    if (bAnim || bCachedWantBlend)
    {
        UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
    }
    else
    {
        Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    }
}

void USkeletalMeshComponent::PreparePhysicsPreview(float DeltaTime)
{
    // 에디터 프리뷰: 게임의 TickComponent 가 안 돌므로 Scene->Tick 직전에 직접 캐시한다.
    // anim 인스턴스가 없어 ref 포즈를 기준으로 한다. 이후 Scene->Tick 이 Pre/PostPhysicsSimulate
    // 핸들러를 통해 키네마틱 추종 + 시뮬 결과 반영을 수행한다.
    RampBodyBlendWeights(DeltaTime);
    UpdateBodySimulationState();

    bCachedWantBlend = AnyBodyPhysicsActive() && !Bodies.empty();
    CachedAnimGlobals.clear();
    if (bCachedWantBlend || !Bodies.empty())
    {
        BuildReferencePoseGlobals(CachedAnimGlobals);
    }
}

void USkeletalMeshComponent::PrePhysicsSimulate(float DeltaTime)
{
    // simulate 직전: 키네마틱(weight 0) 바디를 캐시된 anim 포즈로 추종시킨다(자식 조인트 앵커).
    if (!Bodies.empty())
    {
        SyncKinematicBodiesToAnim(CachedAnimGlobals);
    }
}

void USkeletalMeshComponent::PostPhysicsSimulate(float DeltaTime)
{
    // fetchResults 직후: 활성 바디가 있으면 시뮬 결과를 본 포즈에 블렌드 반영.
    if (bCachedWantBlend)
    {
        ApplyPhysicsBlendedPose(CachedAnimGlobals);
    }
}

// ──────────────────────────────────────────────
// Editor / 직렬화 통합
// ──────────────────────────────────────────────
void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyValue>& OutProps)
{
    Super::GetEditableProperties(OutProps);

    // AnimInstance 자체 properties (Speed 등) 도 패널에 같이 노출 — 컴포넌트가 forward.
    // 자식이 자기 카테고리(예: "Animation|Character") 로 그룹화.
    if (AnimInstance) AnimInstance->GetEditableProperties(OutProps);
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
    Super::PostEditProperty(PropertyName);
    if (!PropertyName) return;

    if (std::strcmp(PropertyName, "AnimationMode") == 0)
    {
        InitializeAnimation();
    }
    else if (std::strcmp(PropertyName, "AnimInstanceClass") == 0)
    {
        // 클래스 슬롯이 바뀌면 Custom 모드에서 인스턴스 재생성 필요. (ours — Phase 6)
        if (AnimationMode == EAnimationMode::AnimationCustom) InitializeAnimation();
    }
    else if (std::strcmp(PropertyName, "AnimationData") == 0)
    {
        LoadAnimationFromPath();

        if (AnimInstance)
        {
            InitializeAnimation();
        }
    }
    else if (std::strcmp(PropertyName, "AnimToPlayPath") == 0)
    {
        // theirs (main): FAnimationManager 가 path 로 실제 UAnimSequence 로딩 — Phase 4 의 TODO 해소.
        // Mode 가 None 이면 SingleNode 로 자동 전환, AnimInstance 없으면 Initialize, 있으면 SingleNode setter 들 갱신.
        LoadAnimationFromPath();

        if (AnimationMode == EAnimationMode::None)
        {
            AnimationMode = EAnimationMode::AnimationSingleNode;
        }

        if (!AnimInstance)
        {
            InitializeAnimation();
        }
        else if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
        {
            if (!CanUseAnimation(AnimationData.AnimToPlay))
            {
                AnimationData.AnimToPlay = nullptr;
                AnimationData.AnimToPlayPath = "None";
            }
            SingleNode->SetAnimationAsset(AnimationData.AnimToPlay);
            SingleNode->SetPlayRate(AnimationData.PlayRate);
            SingleNode->SetLooping(AnimationData.bLooping);
            SingleNode->SetPlaying(AnimationData.bPlaying && AnimationData.AnimToPlay != nullptr);
        }
    }
    else if (std::strcmp(PropertyName, "PlayRate") == 0)
    {
        SetPlayRate(AnimationData.PlayRate);
    }
    else if (std::strcmp(PropertyName, "bLooping") == 0)
    {
        SetLooping(AnimationData.bLooping);
    }
    else if (std::strcmp(PropertyName, "bPlaying") == 0)
    {
        SetPlaying(AnimationData.bPlaying);
    }

    // AnimInstance 자체 properties 는 자식이 자체 PostEdit 처리. 컴포넌트는 dispatch 만.
    // 컴포넌트가 인식한 이름과 겹치지 않는 한 무해 (자식이 모르는 이름은 no-op).
    if (AnimInstance) AnimInstance->PostEditProperty(PropertyName);
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);

    uint8 ModeRaw = static_cast<uint8>(AnimationMode);
    Ar << ModeRaw;
    AnimationMode = static_cast<EAnimationMode>(ModeRaw);

    // AnimToPlay 의 path 만 라운드트립. 실제 포인터 복원은 InitializeAnimation() → LoadAnimationFromPath() 가 처리.
    FString AnimToPlayPath = Ar.IsSaving() ? AnimationData.AnimToPlayPath.ToString() : FString();
    Ar << AnimToPlayPath;
    if (Ar.IsLoading())
    {
        AnimationData.AnimToPlayPath.SetPath(AnimToPlayPath);
    }
    Ar << AnimationData.PlayRate;
    Ar << AnimationData.bLooping;
    Ar << AnimationData.bPlaying;

}

bool USkeletalMeshComponent::EvaluateAnimInstance(float DeltaTime)
{
    if (!AnimInstance) return false;

    USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh) return false;
    FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
    if (!Asset || Asset->Bones.empty()) return false;

    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        if (!CanUseAnimation(SingleNode->GetAnimationAsset()))
        {
            SingleNode->SetAnimationAsset(nullptr);
            return false;
        }
    }

    AnimInstance->UpdateAnimation(DeltaTime);

    // Root motion 적용은 UCharacterMovementComponent 가 책임.
    // CMC::TickComponent (TG_DuringPhysics) 가 매 frame 이 AnimInstance->ConsumeRootMotion 으로
    // 누적값을 가져가 capsule 이동 / 회전에 반영한다 (sweep / floor stick 통과).
    // Mesh 는 actor transform 을 직접 만지지 않는다 — UE 본가 패턴.
    //
    // 주의: CMC 가 없는 actor 에 root motion 켠 anim 을 붙이면 누적값이 anywhere 도
    // 소비되지 않아 in-place 로 보인다. ACharacter 외 케이스에서 root motion 이 필요해지면
    // 별도 소비 경로가 추가되어야 한다.

    FPoseContext Out;
    Out.SkeletalMesh = Mesh;
    Out.Pose.resize(Asset->Bones.size());
    Out.ResetToRefPose();
    AnimInstance->EvaluatePose(Out);

    SetAnimationPose(Out.Pose, Out.MorphWeights);
    return true;
}
